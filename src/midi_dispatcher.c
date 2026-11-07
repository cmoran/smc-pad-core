/**
 * @file midi_dispatcher.c
 * @brief MIDI 1.0 Event Dispatcher Implementation
 */

#include "midi_dispatcher.h"

void smc_midi_init(smc_midi_dispatcher_t *dispatcher,
                  void (*send_cb)(const smc_midi_raw_msg_t *msg)) {
    if (!dispatcher) {
        return;
    }

    dispatcher->send_midi_msg = send_cb;
    dispatcher->active_pads_mask = 0;
    dispatcher->shift_active = false;

    /* Default map: Notes 36..51 (General MIDI drum layout) on Channel 0 (MIDI ch 1) */
    for (uint8_t i = 0; i < SMC_NUM_PADS; i++) {
        dispatcher->pad_configs[i].midi_channel = 0;
        dispatcher->pad_configs[i].note_number = (uint8_t)(36 + i);
    }
}

static void emit_midi_3byte(smc_midi_dispatcher_t *dispatcher, uint8_t status, uint8_t d1, uint8_t d2) {
    if (dispatcher && dispatcher->send_midi_msg) {
        smc_midi_raw_msg_t msg;
        msg.bytes[0] = status;
        msg.bytes[1] = d1;
        msg.bytes[2] = d2;
        msg.len = 3;
        dispatcher->send_midi_msg(&msg);
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
                uint8_t ch = dispatcher->pad_configs[pad_idx].midi_channel & 0x0F;
                uint8_t note = dispatcher->pad_configs[pad_idx].note_number;
                uint8_t vel = (uint8_t)event->value;

                /* Emit Note-On */
                emit_midi_3byte(dispatcher, (uint8_t)(0x90 | ch), note, vel);

                /* Mark pad active in bitmask */
                dispatcher->active_pads_mask |= (uint16_t)(1u << pad_idx);
            }
            break;

        case EVENT_PAD_RELEASE:
            if (event->index < SMC_NUM_PADS) {
                uint8_t pad_idx = event->index;
                uint8_t ch = dispatcher->pad_configs[pad_idx].midi_channel & 0x0F;
                uint8_t note = dispatcher->pad_configs[pad_idx].note_number;

                /* Emit Note-Off */
                emit_midi_3byte(dispatcher, (uint8_t)(0x80 | ch), note, 0);

                /* Clear pad bit */
                dispatcher->active_pads_mask &= (uint16_t)~(1u << pad_idx);
            }
            break;

        case EVENT_ENCODER_DELTA:
            if (encoder_state && event->index < SMC_NUM_ENCODERS) {
                uint8_t enc_idx = event->index;
                uint8_t bank = smc_encoder_get_bank(encoder_state);
                uint8_t ch = encoder_state->configs[bank][enc_idx].midi_channel & 0x0F;
                uint8_t cc = encoder_state->configs[bank][enc_idx].cc_number;
                uint8_t val = encoder_state->configs[bank][enc_idx].value;

                /* Emit Control Change */
                emit_midi_3byte(dispatcher, (uint8_t)(0xB0 | ch), cc, val);
            }
            break;

        case EVENT_BUTTON_DOWN:
            if (event->index == BTN_SHIFT) {
                dispatcher->shift_active = true;
            } else if (event->index == BTN_PAUSE && dispatcher->shift_active) {
                /* Shift + Pause -> Instant Panic */
                smc_midi_panic(dispatcher);
            } else if (event->index == BTN_PAD_BANK && dispatcher->shift_active && encoder_state) {
                /* Shift + Pad Bank -> Cycle Encoder Bank */
                uint8_t next_bank = (smc_encoder_get_bank(encoder_state) + 1) % SMC_NUM_BANKS;
                smc_encoder_set_bank(encoder_state, next_bank);
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

    /* 1. Emit CC 120 (All Sound Off) & CC 123 (All Notes Off) on all active channels */
    for (uint8_t ch = 0; ch < 16; ch++) {
        emit_midi_3byte(dispatcher, (uint8_t)(0xB0 | ch), 120, 0); /* CC 120 All Sound Off */
        emit_midi_3byte(dispatcher, (uint8_t)(0xB0 | ch), 123, 0); /* CC 123 All Notes Off */
    }

    /* 2. Emit explicit Note-Off for any tracked active pad */
    for (uint8_t i = 0; i < SMC_NUM_PADS; i++) {
        if (dispatcher->active_pads_mask & (1u << i)) {
            uint8_t ch = dispatcher->pad_configs[i].midi_channel & 0x0F;
            uint8_t note = dispatcher->pad_configs[i].note_number;
            emit_midi_3byte(dispatcher, (uint8_t)(0x80 | ch), note, 0);
        }
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
