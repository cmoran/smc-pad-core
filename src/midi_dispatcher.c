/**
 * @file midi_dispatcher.c
 * @brief MIDI 1.0 Event Dispatcher with Dual-Transport Output, Virtual Banking, and Host RX
 */

#include "midi_dispatcher.h"
#include <string.h>

void smc_midi_init(smc_midi_dispatcher_t *dispatcher,
                  void (*send_cb)(const smc_midi_raw_msg_t *msg)) {
    if (!dispatcher) {
        return;
    }

    memset(dispatcher, 0, sizeof(smc_midi_dispatcher_t));
    dispatcher->send_midi_msg = send_cb;
    dispatcher->send_ble_msg = NULL;
    dispatcher->config = NULL;
    dispatcher->active_pads_mask = 0;
    dispatcher->shift_active = false;
    dispatcher->octave_offset = 0;
    dispatcher->semitone_offset = 0;
    dispatcher->channel_override = -1;

    /* Initialize sounding notes to 0xFF (no note currently sounding) */
    memset(dispatcher->sounding_notes, 0xFF, sizeof(dispatcher->sounding_notes));
    memset(dispatcher->sounding_channels, 0, sizeof(dispatcher->sounding_channels));

    /* Default GM drum layout for Bank 0 fallback */
    for (uint8_t i = 0; i < SMC_NUM_PADS; i++) {
        dispatcher->pad_configs[i].midi_channel = 0;
        dispatcher->pad_configs[i].note_number = (uint8_t)(36 + i);
    }
}

void smc_midi_set_config(smc_midi_dispatcher_t *dispatcher, const smc_config_t *config) {
    if (dispatcher) {
        dispatcher->config = config;
    }
}

void smc_midi_set_ble_sink(smc_midi_dispatcher_t *dispatcher,
                           void (*ble_send_cb)(const smc_midi_raw_msg_t *msg)) {
    if (dispatcher) {
        dispatcher->send_ble_msg = ble_send_cb;
    }
}

void smc_midi_set_host_callbacks(smc_midi_dispatcher_t *dispatcher,
                                smc_midi_host_note_cb note_cb,
                                smc_midi_sysex_byte_cb sysex_cb,
                                void *ctx) {
    if (!dispatcher) return;
    dispatcher->host_note_cb = note_cb;
    dispatcher->sysex_cb = sysex_cb;
    dispatcher->host_cb_ctx = ctx;
}

void smc_midi_set_octave_offset(smc_midi_dispatcher_t *dispatcher, int8_t semitones) {
    if (!dispatcher) return;
    if (semitones < -48) semitones = -48;
    if (semitones > 48) semitones = 48;
    dispatcher->octave_offset = semitones;
}

int8_t smc_midi_get_octave_offset(const smc_midi_dispatcher_t *dispatcher) {
    return dispatcher ? dispatcher->octave_offset : 0;
}

void smc_midi_set_semitone_offset(smc_midi_dispatcher_t *dispatcher, int8_t semitones) {
    if (!dispatcher) return;
    if (semitones < -12) semitones = -12;
    if (semitones > 12) semitones = 12;
    dispatcher->semitone_offset = semitones;
}

int8_t smc_midi_get_semitone_offset(const smc_midi_dispatcher_t *dispatcher) {
    return dispatcher ? dispatcher->semitone_offset : 0;
}

void smc_midi_set_channel_override(smc_midi_dispatcher_t *dispatcher, int8_t channel) {
    if (!dispatcher) return;
    if (channel > 15) channel = 15;
    dispatcher->channel_override = channel;
}

int8_t smc_midi_get_channel_override(const smc_midi_dispatcher_t *dispatcher) {
    return dispatcher ? dispatcher->channel_override : -1;
}

static void emit_midi_3byte(smc_midi_dispatcher_t *dispatcher, uint8_t status, uint8_t d1, uint8_t d2) {
    if (!dispatcher) return;

    smc_midi_raw_msg_t msg;
    msg.bytes[0] = status;
    msg.bytes[1] = d1;
    msg.bytes[2] = d2;
    msg.len = 3;

    /* Dispatch to USB-MIDI */
    if (dispatcher->send_midi_msg) {
        dispatcher->send_midi_msg(&msg);
    }

    /* Dispatch concurrently to BLE-MIDI */
    if (dispatcher->send_ble_msg) {
        dispatcher->send_ble_msg(&msg);
    }
}

void smc_midi_process_event(smc_midi_dispatcher_t *dispatcher,
                           smc_encoder_state_t *encoder_state,
                           const smc_event_t *event) {
    if (!dispatcher || !event) {
        return;
    }

    switch (event->type) {
        case EVENT_PAD_STRIKE:
            if (event->index < SMC_NUM_PADS) {
                uint8_t pad_idx = event->index;
                uint8_t ch = 0;
                uint8_t note = 36 + pad_idx;

                if (dispatcher->config) {
                    const smc_pad_cfg_t *p_cfg = smc_config_get_active_pad(dispatcher->config, pad_idx);
                    if (p_cfg) {
                        ch = p_cfg->channel & 0x0Fu;
                        note = p_cfg->note;
                    }
                } else {
                    ch = dispatcher->pad_configs[pad_idx].midi_channel & 0x0Fu;
                    note = dispatcher->pad_configs[pad_idx].note_number;
                }

                uint8_t vel = (uint8_t)event->value;

                /* Apply octave and semitone transposition */
                int16_t transposed = (int16_t)note + dispatcher->octave_offset + dispatcher->semitone_offset;
                if (transposed < 0) transposed = 0;
                if (transposed > 127) transposed = 127;
                note = (uint8_t)transposed;

                /* Apply channel override if configured */
                if (dispatcher->channel_override >= 0) {
                    ch = (uint8_t)(dispatcher->channel_override & 0x0Fu);
                }

                /* Record sounding note and channel to guarantee matching Note-Off */
                dispatcher->sounding_notes[pad_idx] = note;
                dispatcher->sounding_channels[pad_idx] = ch;

                /* Emit Note-On */
                emit_midi_3byte(dispatcher, (uint8_t)(0x90u | ch), note, vel);

                /* Mark pad active in bitmask */
                dispatcher->active_pads_mask |= (uint16_t)(1u << pad_idx);
            }
            break;

        case EVENT_PAD_RELEASE:
            if (event->index < SMC_NUM_PADS) {
                uint8_t pad_idx = event->index;
                uint8_t ch = 0;
                uint8_t note = 0;

                /* If this pad had a sounding note recorded, use it directly */
                if (dispatcher->sounding_notes[pad_idx] != 0xFF) {
                    note = dispatcher->sounding_notes[pad_idx];
                    ch = dispatcher->sounding_channels[pad_idx];
                    dispatcher->sounding_notes[pad_idx] = 0xFF;
                } else {
                    /* Fallback to current config mapping */
                    if (dispatcher->config) {
                        const smc_pad_cfg_t *p_cfg = smc_config_get_active_pad(dispatcher->config, pad_idx);
                        if (p_cfg) {
                            ch = p_cfg->channel & 0x0Fu;
                            note = p_cfg->note;
                        }
                    } else {
                        ch = dispatcher->pad_configs[pad_idx].midi_channel & 0x0Fu;
                        note = dispatcher->pad_configs[pad_idx].note_number;
                    }

                    int16_t transposed = (int16_t)note + dispatcher->octave_offset + dispatcher->semitone_offset;
                    if (transposed < 0) transposed = 0;
                    if (transposed > 127) transposed = 127;
                    note = (uint8_t)transposed;

                    if (dispatcher->channel_override >= 0) {
                        ch = (uint8_t)(dispatcher->channel_override & 0x0Fu);
                    }
                }

                /* Emit Note-Off */
                emit_midi_3byte(dispatcher, (uint8_t)(0x80u | ch), note, 0);

                /* Clear pad bit */
                dispatcher->active_pads_mask &= (uint16_t)~(1u << pad_idx);
            }
            break;

        case EVENT_ENCODER_DELTA:
            if (encoder_state && event->index < SMC_NUM_ENCODERS) {
                uint8_t enc_idx = event->index;
                uint8_t ch = 0;
                uint8_t cc = 16 + enc_idx;

                if (dispatcher->config) {
                    const smc_encoder_cfg_t *e_cfg = smc_config_get_active_encoder(dispatcher->config, enc_idx);
                    if (e_cfg) {
                        ch = e_cfg->channel & 0x0Fu;
                        cc = e_cfg->cc;
                    }
                } else {
                    uint8_t bank = smc_encoder_get_bank(encoder_state);
                    ch = encoder_state->configs[bank][enc_idx].midi_channel & 0x0Fu;
                    cc = encoder_state->configs[bank][enc_idx].cc_number;
                }

                uint8_t bank = smc_encoder_get_bank(encoder_state);
                uint8_t val = encoder_state->configs[bank][enc_idx].value;

                /* Emit Control Change */
                emit_midi_3byte(dispatcher, (uint8_t)(0xB0u | ch), cc, val);
            }
            break;

        case EVENT_BUTTON_DOWN:
            if (event->index == BTN_SHIFT) {
                dispatcher->shift_active = true;
            } else if (event->index == BTN_PAUSE && dispatcher->shift_active) {
                /* Shift + Pause -> Instant Panic */
                smc_midi_panic(dispatcher);
            }
            break;

        case EVENT_BUTTON_UP:
            if (event->index == BTN_SHIFT) {
                dispatcher->shift_active = false;
            }
            break;

        default:
            break;
    }
}

void smc_midi_panic(smc_midi_dispatcher_t *dispatcher) {
    if (!dispatcher) {
        return;
    }

    /* 1. Emit CC 120 (All Sound Off) & CC 123 (All Notes Off) on all 16 channels */
    for (uint8_t ch = 0; ch < 16; ch++) {
        emit_midi_3byte(dispatcher, (uint8_t)(0xB0u | ch), 120, 0);
        emit_midi_3byte(dispatcher, (uint8_t)(0xB0u | ch), 123, 0);
    }

    /* 2. Emit explicit Note-Off for any tracked active pad */
    for (uint8_t i = 0; i < SMC_NUM_PADS; i++) {
        if (dispatcher->active_pads_mask & (1u << i)) {
            uint8_t ch = 0;
            uint8_t note = 0;

            if (dispatcher->sounding_notes[i] != 0xFF) {
                note = dispatcher->sounding_notes[i];
                ch = dispatcher->sounding_channels[i];
                dispatcher->sounding_notes[i] = 0xFF;
            } else {
                if (dispatcher->config) {
                    const smc_pad_cfg_t *p_cfg = smc_config_get_active_pad(dispatcher->config, i);
                    if (p_cfg) {
                        ch = p_cfg->channel & 0x0Fu;
                        note = p_cfg->note;
                    }
                } else {
                    ch = dispatcher->pad_configs[i].midi_channel & 0x0Fu;
                    note = dispatcher->pad_configs[i].note_number;
                }
            }

            emit_midi_3byte(dispatcher, (uint8_t)(0x80u | ch), note, 0);
        }
        dispatcher->sounding_notes[i] = 0xFF;
    }

    /* 3. Clear active mask */
    dispatcher->active_pads_mask = 0;
}

bool smc_midi_is_pad_active(const smc_midi_dispatcher_t *dispatcher, uint8_t pad_idx) {
    if (!dispatcher || pad_idx >= SMC_NUM_PADS) {
        return false;
    }
    return (dispatcher->active_pads_mask & (1u << pad_idx)) != 0;
}

void smc_midi_rx_feed_byte(smc_midi_dispatcher_t *dispatcher, uint8_t byte) {
    if (!dispatcher) return;

    /* Real-time messages (0xF8..0xFF) pass immediately */
    if (byte >= 0xF8u) {
        return;
    }

    /* SysEx start */
    if (byte == 0xF0u) {
        dispatcher->rx_in_sysex = true;
        if (dispatcher->sysex_cb) {
            dispatcher->sysex_cb(byte, dispatcher->host_cb_ctx);
        }
        return;
    }

    /* Inside SysEx stream */
    if (dispatcher->rx_in_sysex) {
        if (dispatcher->sysex_cb) {
            dispatcher->sysex_cb(byte, dispatcher->host_cb_ctx);
        }
        if (byte == 0xF7u || (byte & 0x80u)) {
            dispatcher->rx_in_sysex = false;
        }
        return;
    }

    /* Channel voice status byte (0x80..0xEF) */
    if (byte & 0x80u) {
        dispatcher->rx_running_status = byte;
        dispatcher->rx_idx = 0;
        uint8_t type = byte & 0xF0u;
        if (type == 0xC0u || type == 0xD0u) {
            dispatcher->rx_expected_len = 1;
        } else {
            dispatcher->rx_expected_len = 2;
        }
        return;
    }

    /* Data bytes under running status */
    if (dispatcher->rx_running_status && dispatcher->rx_idx < 2) {
        dispatcher->rx_bytes[dispatcher->rx_idx++] = byte;

        if (dispatcher->rx_idx >= dispatcher->rx_expected_len) {
            uint8_t type = dispatcher->rx_running_status & 0xF0u;
            uint8_t ch = dispatcher->rx_running_status & 0x0Fu;

            if (type == 0x90u) {
                /* Note-On (velocity 0 is treated as Note-Off) */
                if (dispatcher->host_note_cb) {
                    dispatcher->host_note_cb(ch, dispatcher->rx_bytes[0], dispatcher->rx_bytes[1], dispatcher->host_cb_ctx);
                }
            } else if (type == 0x80u) {
                /* Note-Off */
                if (dispatcher->host_note_cb) {
                    dispatcher->host_note_cb(ch, dispatcher->rx_bytes[0], 0, dispatcher->host_cb_ctx);
                }
            }

            dispatcher->rx_idx = 0; /* Ready for next data bytes under running status */
        }
    }
}

void smc_midi_rx_feed_bytes(smc_midi_dispatcher_t *dispatcher, const uint8_t *data, size_t len) {
    if (!dispatcher || !data) return;
    for (size_t i = 0; i < len; i++) {
        smc_midi_rx_feed_byte(dispatcher, data[i]);
    }
}
