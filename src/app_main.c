/**
 * @file app_main.c
 * @brief SMC-Pad Clean-Slate Open Firmware Entry Point (JieLi AC6966C / BR25)
 *
 * Implements a high-performance, low-latency, "dumb" MIDI controller pipeline:
 * - Hardware interrupt event loop servicing 16 velocity pads, 8 encoders, and buttons
 * - Per-bank distinctive RGB LED color palettes with instant matrix transition
 * - Octave transposition via Left / Right Arrow buttons (with LED flash)
 * - Shift layer matrix combinations (direct bank jump, curve select, channel shift, panic)
 * - Note Repeat rhythmic engine with Button 9 LED toggle
 * - Non-volatile Flash configuration & 4 virtual banks (pads & encoders)
 * - Open SysEx protocol (0x7D) for bidirectional WebMIDI parameter tuning
 * - Dual-output concurrent USB-MIDI and BLE-MIDI dispatching
 * - Host DAW reactive LED feedback via incoming MIDI Note-On/CC
 */

#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"
#include "matrix.h"
#include "pad_adc.h"
#include "midi_dispatcher.h"
#include "rgb_led.h"
#include "smc_config.h"
#include "sysex_protocol.h"
#include "ble_midi.h"
#include "velocity_curve.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#ifndef SMC_HOST_TEST
/* Hardware runtime bindings from smcpad_core.o */
extern void smcpad_boot_ok(void);
extern int  smcpad_event_get(void *ev);
extern void smcpad_led_set(int led, int on);
extern void smcpad_rgb_set(int pad, uint32_t rgb);
extern void smcpad_rgb_show(void);
extern int  smcpad_usbmidi_write(const uint8_t packet[4]);
extern int  smcpad_usbmidi_read(uint8_t packet[4]);
extern uint32_t smcpad_millis(void);
extern void smcpad_delay(uint32_t ms);

typedef struct {
    uint8_t type;
    uint8_t index;
    int16_t value;
} smcpad_hw_event_t;

static inline uint32_t app_get_millis(void) {
    return smcpad_millis();
}
#else
static uint32_t s_host_millis = 0;
static inline uint32_t app_get_millis(void) {
    return s_host_millis++;
}
#endif

/* Static system state structures (Strict zero dynamic allocation) */
static smc_event_queue_t     g_event_queue;
static smc_encoder_state_t   g_encoder_engine;
static smc_midi_dispatcher_t g_midi_engine;
static smc_config_t          g_config;
static smc_sysex_engine_t    g_sysex_engine;
static smc_ble_midi_t        g_ble_engine;

static volatile uint32_t     g_system_ticks = 0;
static uint8_t               g_pad_bank = 0;
static uint8_t               g_encoder_bank = 0;
static int8_t                g_octave_offset = 0;
static int8_t                g_channel_offset = -1;

/* Note Repeat Division Table (at 120 BPM) */
static const uint16_t g_repeat_periods_ms[8] = {
    500, /* Pad 1: 1/4 */
    333, /* Pad 2: 1/4t */
    250, /* Pad 3: 1/8 */
    167, /* Pad 4: 1/8t */
    125, /* Pad 5: 1/16 (default) */
    83,  /* Pad 6: 1/16t */
    62,  /* Pad 7: 1/32 */
    42   /* Pad 8: 1/32t */
};

/* Note Repeat Engine state */
static bool                  g_note_repeat_enabled = false;
static bool                  g_note_repeat_cfg_mode = false;
static bool                  g_note_repeat_btn_held = false;
static uint8_t               g_repeat_rate_idx = 4; /* Default 1/16 = 125 ms */
static uint32_t              g_repeat_cycle_start_ms = 0;
static bool                  g_repeat_gate_active = false;
static uint16_t              g_held_pads_mask = 0;
static uint8_t               g_pad_velocities[SMC_NUM_PADS];

/* ========================================================================= */
/* Hardware Callbacks (HAL)                                                  */
/* ========================================================================= */

static void hal_send_midi_packet(const smc_midi_raw_msg_t *msg) {
    if (!msg || msg->len == 0) return;

#ifndef SMC_HOST_TEST
    uint8_t cin = (msg->bytes[0] >> 4) & 0x0Fu;
    uint8_t packet[4] = {
        cin,
        msg->bytes[0],
        (msg->len > 1) ? msg->bytes[1] : 0,
        (msg->len > 2) ? msg->bytes[2] : 0
    };
    smcpad_usbmidi_write(packet);
#endif
}

static void hal_send_ble_packet(const smc_midi_raw_msg_t *msg) {
    if (msg && msg->len > 0) {
        smc_ble_midi_send_message(&g_ble_engine, msg->bytes, msg->len, g_system_ticks);
    }
}

static void hal_send_sysex_packet(const uint8_t *data, size_t len) {
#ifndef SMC_HOST_TEST
    size_t offset = 0;
    while (offset < len) {
        size_t rem = len - offset;
        uint8_t cin = 0x04;
        uint8_t p[4] = { 0, 0, 0, 0 };
        if (rem >= 3) {
            p[1] = data[offset++];
            p[2] = data[offset++];
            p[3] = data[offset++];
            if (rem == 3 && p[3] == 0xF7) cin = 0x07;
        } else if (rem == 2) {
            p[1] = data[offset++];
            p[2] = data[offset++];
            cin = 0x06;
        } else {
            p[1] = data[offset++];
            cin = 0x05;
        }
        p[0] = cin;
        smcpad_usbmidi_write(p);
    }
#endif
    smc_ble_midi_send_sysex(&g_ble_engine, data, (uint16_t)len, g_system_ticks);
}

static void hal_host_note_feedback(uint8_t ch, uint8_t note, uint8_t vel, void *ctx) {
    (void)ch;
    (void)ctx;
    uint8_t bank = smc_config_get_pad_bank();
    const smc_bank_palette_t *pal = smc_rgb_led_get_bank_palette(bank);

    for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
        if (g_config.pads[bank][p].note == note) {
            uint32_t color = (vel > 0)
                ? ((g_config.led_mode == SMC_LED_MODE_HOST)
                    ? (((uint32_t)vel * 2u) << 8) | vel
                    : pal->hit_color)
                : pal->idle_color;
#ifndef SMC_HOST_TEST
            smcpad_rgb_set(p, color);
            smcpad_rgb_show();
#else
            smc_rgb_led_set_pad(p, color);
            smc_rgb_led_show();
#endif
        }
    }
}

static void hal_host_sysex_feed(uint8_t b, void *ctx) {
    (void)ctx;
    smc_sysex_feed_byte(&g_sysex_engine, b);
}

/* ========================================================================= */
/* Application Entry Points (setup / loop)                                   */
/* ========================================================================= */

static void restore_bank_leds(void) {
#ifndef SMC_HOST_TEST
    const smc_bank_palette_t *p_pal = smc_rgb_led_get_bank_palette(g_pad_bank);
    for (int p = 0; p < SMC_NUM_PADS; p++) {
        smcpad_rgb_set(p, p_pal->idle_color);
    }
    smcpad_rgb_show();
#else
    smc_rgb_led_set_bank_theme(g_pad_bank);
    smc_rgb_led_show();
#endif
}

static void refresh_shift_status_view(void) {
    int8_t octave = smc_midi_get_octave_offset(&g_midi_engine);
    int8_t semi = smc_midi_get_semitone_offset(&g_midi_engine);
    smc_rgb_led_render_shift_status(g_pad_bank, (uint8_t)g_config.velocity_curve, octave, semi);
#ifndef SMC_HOST_TEST
    for (int p = 0; p < SMC_NUM_PADS; p++) {
        smcpad_rgb_set(p, smc_rgb_led_get_pad(p));
    }
    smcpad_rgb_show();
#else
    smc_rgb_led_show();
#endif
}

static void refresh_repeat_cfg_leds(void) {
#ifndef SMC_HOST_TEST
    for (uint8_t p = 0; p < 8; p++) {
        /* Selected division is White, others are dim blue (0x000030) */
        smcpad_rgb_set(p, (p == g_repeat_rate_idx) ? SMC_COLOR_WHITE : 0x000030u);
    }
    for (uint8_t p = 8; p < SMC_NUM_PADS; p++) {
        smcpad_rgb_set(p, 0x000000u);
    }
    smcpad_rgb_show();
#endif
}

void setup(void) {
#ifndef SMC_HOST_TEST
    /* Handshake with DFU bootloader to confirm successful boot */
    smcpad_boot_ok();
#else
    REG32(SMC_BOOT_COUNT_ADDR) = 0;
#endif

    /* 1. Initialize queue & configuration */
    smc_queue_init(&g_event_queue);
    smc_config_init(&g_config, NULL, NULL);
    smc_encoder_init(&g_encoder_engine, 2);

    /* 2. Initialize SysEx Protocol Engine */
    smc_sysex_init(&g_sysex_engine, &g_config, hal_send_sysex_packet);

    /* 3. Initialize BLE-MIDI Engine */
    smc_ble_midi_init(&g_ble_engine, NULL);

    /* 4. Initialize MIDI Dispatcher */
    smc_midi_init(&g_midi_engine, hal_send_midi_packet);
    smc_midi_set_config(&g_midi_engine, &g_config);
    smc_midi_set_ble_sink(&g_midi_engine, hal_send_ble_packet);
    smc_midi_set_host_callbacks(&g_midi_engine, hal_host_note_feedback, hal_host_sysex_feed, NULL);

    /* 5. Startup Boot Animation */
#ifndef SMC_HOST_TEST
    smc_rgb_led_boot_animation(smcpad_delay, smcpad_rgb_show);
#else
    smc_rgb_led_init();
    smc_rgb_led_boot_animation(NULL, NULL);
#endif

    /* 6. Set Initial Resting Pad Colors from Bank Palette */
    restore_bank_leds();
}

void loop(void) {
    g_system_ticks++;

    /* 1. Drain incoming USB-MIDI packets from host DAW or WebMIDI */
#ifndef SMC_HOST_TEST
    uint8_t rx_pkt[4];
    while (smcpad_usbmidi_read(rx_pkt)) {
        uint8_t cin = rx_pkt[0] & 0x0Fu;
        if (cin == 0x08 || cin == 0x09 || cin == 0x0B || cin == 0x04 || cin == 0x07) {
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[1]);
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[2]);
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[3]);
        } else if (cin == 0x06) {
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[1]);
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[2]);
        } else if (cin == 0x05) {
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[1]);
        }
    }

    /* 2. Process Note Repeat rhythmic pulses (Gated duty cycle to prevent polyphony overload) */
    if (g_note_repeat_enabled && g_held_pads_mask > 0 && !g_note_repeat_cfg_mode) {
        uint32_t now = app_get_millis();
        uint16_t period = g_repeat_periods_ms[g_repeat_rate_idx];
        uint16_t gate_ms = (uint16_t)((uint32_t)period * 60u / 100u);
        if (gate_ms < 15) gate_ms = 15;

        if (!g_repeat_gate_active) {
            if ((now - g_repeat_cycle_start_ms >= period) || (g_repeat_cycle_start_ms == 0)) {
                g_repeat_cycle_start_ms = now;
                g_repeat_gate_active = true;

                for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
                    if (g_held_pads_mask & (1u << p)) {
                        smc_event_t ev_on = {
                            .type = EVENT_PAD_STRIKE,
                            .index = p,
                            .value = g_pad_velocities[p],
                            .timestamp = now
                        };
                        smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev_on);
                        smcpad_rgb_set(p, smc_rgb_led_get_bank_palette(g_pad_bank)->hit_color);
                    }
                }
                smcpad_rgb_show();
            }
        } else {
            /* Gate active: check if 60% gate interval elapsed */
            if (now - g_repeat_cycle_start_ms >= gate_ms) {
                g_repeat_gate_active = false;

                for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
                    if (g_held_pads_mask & (1u << p)) {
                        smc_event_t ev_off = {
                            .type = EVENT_PAD_RELEASE,
                            .index = p,
                            .value = 0,
                            .timestamp = now
                        };
                        smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev_off);
                        smcpad_rgb_set(p, smc_rgb_led_get_bank_palette(g_pad_bank)->idle_color);
                    }
                }
                smcpad_rgb_show();
            }
        }
    }

    /* 3. Process hardware inputs (pads, buttons, encoders) */
    smcpad_hw_event_t hw_ev;
    while (smcpad_event_get(&hw_ev)) {
        smc_event_t ev;
        switch (hw_ev.type) {
        case 1: /* SMCPAD_EV_PAD_ON (Pad Strike) */
            /* If in Note Repeat Configuration Mode: */
            if (g_note_repeat_cfg_mode) {
                if (hw_ev.index < 8) {
                    g_repeat_rate_idx = hw_ev.index;
                    refresh_repeat_cfg_leds();
                }
                break;
            }

            /* If Shift is active: execute stock Shift shortcuts instead of emitting Note-On */
            if (g_midi_engine.shift_active) {
                if (hw_ev.index < 8) {
                    /* Shift + Pads 1-8 (indices 0..7): Switch Bank/Preset */
                    g_pad_bank = hw_ev.index % SMC_NUM_BANKS;
                    smc_config_set_pad_bank(g_pad_bank);
                    smcpad_led_set(1, (g_pad_bank > 0));
                    refresh_shift_status_view();
                } else if (hw_ev.index >= 8 && hw_ev.index <= 11) {
                    /* Shift + Pads 9-12 (indices 8..11): Velocity Curve (Linear, Soft, Hard, Fixed) */
                    g_config.velocity_curve = (smc_velocity_curve_type_t)(hw_ev.index - 8);
                    refresh_shift_status_view();
                } else if (hw_ev.index == 12) {
                    /* Shift + Pad 13 (index 12): Transpose Semitone Down (-1) */
                    int8_t semi = smc_midi_get_semitone_offset(&g_midi_engine) - 1;
                    if (semi < -12) semi = -12;
                    smc_midi_set_semitone_offset(&g_midi_engine, semi);
                    refresh_shift_status_view();
                } else if (hw_ev.index == 13) {
                    /* Shift + Pad 14 (index 13): Transpose Semitone Up (+1) */
                    int8_t semi = smc_midi_get_semitone_offset(&g_midi_engine) + 1;
                    if (semi > 12) semi = 12;
                    smc_midi_set_semitone_offset(&g_midi_engine, semi);
                    refresh_shift_status_view();
                } else if (hw_ev.index == 14 || hw_ev.index == 15) {
                    /* Track held status for dual-press detection */
                    g_held_pads_mask |= (uint16_t)(1u << hw_ev.index);

                    /* Shift + Pad 15 & Pad 16 dual-press: Reset Transpose & Octave */
                    if ((g_held_pads_mask & (1u << 14)) && (g_held_pads_mask & (1u << 15))) {
                        g_octave_offset = 0;
                        smc_midi_set_octave_offset(&g_midi_engine, 0);
                        smc_midi_set_semitone_offset(&g_midi_engine, 0);
                    } else if (hw_ev.index == 14) {
                        /* Shift + Pad 15: Octave Down (-12) */
                        g_octave_offset -= 12;
                        if (g_octave_offset < -48) g_octave_offset = -48;
                        smc_midi_set_octave_offset(&g_midi_engine, g_octave_offset);
                    } else {
                        /* Shift + Pad 16: Octave Up (+12) */
                        g_octave_offset += 12;
                        if (g_octave_offset > 48) g_octave_offset = 48;
                        smc_midi_set_octave_offset(&g_midi_engine, g_octave_offset);
                    }
                    refresh_shift_status_view();
                }
                break;
            }

            /* Normal Pad Strike */
            g_held_pads_mask |= (uint16_t)(1u << hw_ev.index);

            /* Calculate velocity shaping if applicable */
            uint8_t vel = (uint8_t)hw_ev.value;
            if (g_config.velocity_curve == VELOCITY_CURVE_FIXED) {
                vel = 127;
            } else if (g_config.velocity_curve != VELOCITY_CURVE_LINEAR) {
                vel = smc_calculate_velocity(vel, 1, 127, g_config.velocity_curve);
            }
            g_pad_velocities[hw_ev.index] = vel;

            if (g_note_repeat_enabled) {
                /* When note repeat is enabled, reset cycle timer so repeat engine triggers immediately */
                if (!g_repeat_gate_active) {
                    g_repeat_cycle_start_ms = 0;
                }
            } else {
                ev.type = EVENT_PAD_STRIKE;
                ev.index = hw_ev.index;
                ev.value = vel;
                smcpad_rgb_set(ev.index, smc_rgb_led_get_bank_palette(g_pad_bank)->hit_color);
                smcpad_rgb_show();
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            }
            break;

        case 2: /* SMCPAD_EV_PAD_OFF (Pad Release) */
            g_held_pads_mask &= (uint16_t)~(1u << hw_ev.index);

            if (g_note_repeat_cfg_mode) {
                break;
            }

            if (g_midi_engine.shift_active) {
                refresh_shift_status_view();
                break;
            }

            if (g_note_repeat_enabled) {
                /* In Note Repeat mode: if this pad was sounding, turn it off immediately */
                ev.type = EVENT_PAD_RELEASE;
                ev.index = hw_ev.index;
                ev.value = 0;
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
                smcpad_rgb_set(ev.index, smc_rgb_led_get_bank_palette(g_pad_bank)->idle_color);
                smcpad_rgb_show();

                if (g_held_pads_mask == 0) {
                    g_repeat_gate_active = false;
                    g_repeat_cycle_start_ms = 0;
                }
            } else {
                ev.type = EVENT_PAD_RELEASE;
                ev.index = hw_ev.index;
                ev.value = 0;
                smcpad_rgb_set(ev.index, smc_rgb_led_get_bank_palette(g_pad_bank)->idle_color);
                smcpad_rgb_show();
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            }
            break;

        case 5: /* SMCPAD_EV_ENCODER */
            /* If Note Repeat button is held, Knob 1 (index 0) adjusts rate division */
            if (g_note_repeat_btn_held && hw_ev.index == 0) {
                int new_rate = (int)g_repeat_rate_idx + hw_ev.value;
                if (new_rate < 0) new_rate = 0;
                if (new_rate > 7) new_rate = 7;
                g_repeat_rate_idx = (uint8_t)new_rate;
                if (g_note_repeat_cfg_mode) {
                    refresh_repeat_cfg_leds();
                }
                break;
            }

            ev.type = EVENT_ENCODER_DELTA;
            ev.index = hw_ev.index;
            ev.value = hw_ev.value;
            if (ev.index < SMC_NUM_ENCODERS) {
                uint8_t bank = smc_config_get_encoder_bank();
                int32_t val = (int32_t)g_encoder_engine.configs[bank][ev.index].value + ev.value;
                if (val < 0) val = 0;
                if (val > 127) val = 127;
                g_encoder_engine.configs[bank][ev.index].value = (uint8_t)val;
            }
            smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            break;

        case 3: /* SMCPAD_EV_BUTTON_DOWN */
            if (hw_ev.index == 8) { /* Shift (Btn 8) */
                g_midi_engine.shift_active = true;
                smcpad_led_set(8, 1);
                refresh_shift_status_view();
            } else if (hw_ev.index == 9) { /* Note Repeat (Btn 9) */
                g_note_repeat_btn_held = true;
                if (g_midi_engine.shift_active) {
                    /* Shift + Note Repeat = Toggle Repeat Config Mode */
                    g_note_repeat_cfg_mode = !g_note_repeat_cfg_mode;
                    if (g_note_repeat_cfg_mode) {
                        refresh_repeat_cfg_leds();
                    } else {
                        restore_bank_leds();
                    }
                } else {
                    /* Normal Note Repeat Toggle */
                    g_note_repeat_enabled = !g_note_repeat_enabled;
                    smcpad_led_set(9, g_note_repeat_enabled);
                    if (!g_note_repeat_enabled && g_repeat_gate_active) {
                        for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
                            if (g_held_pads_mask & (1u << p)) {
                                smc_event_t ev_off = { .type = EVENT_PAD_RELEASE, .index = p, .value = 0 };
                                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev_off);
                                smcpad_rgb_set(p, smc_rgb_led_get_bank_palette(g_pad_bank)->idle_color);
                            }
                        }
                        smcpad_rgb_show();
                        g_repeat_gate_active = false;
                    }
                }
            } else if (hw_ev.index == 6 && g_midi_engine.shift_active) { /* Shift + Pause (Btn 6) = Panic */
                smc_midi_panic(&g_midi_engine);
                restore_bank_leds();
            } else if (hw_ev.index == 1) { /* Pad Bank (Btn 1) */
                g_pad_bank = (g_pad_bank + 1) & 3;
                smc_config_set_pad_bank(g_pad_bank);
                restore_bank_leds();
                smcpad_led_set(1, (g_pad_bank > 0));
            } else if (hw_ev.index == 2) { /* Knob Bank (Btn 2) */
                g_encoder_bank = (g_encoder_bank + 1) & 3;
                smc_config_set_encoder_bank(g_encoder_bank);
                smcpad_led_set(2, (g_encoder_bank > 0));
            } else if (hw_ev.index == 3) { /* Left Arrow (Btn 3) */
                smcpad_led_set(3, 1);
                if (g_midi_engine.shift_active) {
                    /* Shift + Left: Decrement MIDI channel (0..15) */
                    g_channel_offset = (g_channel_offset <= 0) ? 15 : g_channel_offset - 1;
                    smc_midi_set_channel_override(&g_midi_engine, g_channel_offset);
                } else {
                    /* Left: Octave Down (-12 semitones) */
                    g_octave_offset -= 12;
                    if (g_octave_offset < -48) g_octave_offset = -48;
                    smc_midi_set_octave_offset(&g_midi_engine, g_octave_offset);
                }
            } else if (hw_ev.index == 4) { /* Right Arrow (Btn 4) */
                smcpad_led_set(4, 1);
                if (g_midi_engine.shift_active) {
                    /* Shift + Right: Increment MIDI channel (0..15) */
                    g_channel_offset = (g_channel_offset < 0) ? 1 : (g_channel_offset + 1) & 15;
                    smc_midi_set_channel_override(&g_midi_engine, g_channel_offset);
                } else {
                    /* Right: Octave Up (+12 semitones) */
                    g_octave_offset += 12;
                    if (g_octave_offset > 48) g_octave_offset = 48;
                    smc_midi_set_octave_offset(&g_midi_engine, g_octave_offset);
                }
            }
            break;

        case 4: /* SMCPAD_EV_BUTTON_UP */
            if (hw_ev.index == 8) {
                g_midi_engine.shift_active = false;
                smcpad_led_set(8, 0);
                if (g_note_repeat_cfg_mode) {
                    g_note_repeat_cfg_mode = false;
                    restore_bank_leds();
                }
            } else if (hw_ev.index == 9) {
                g_note_repeat_btn_held = false;
            } else if (hw_ev.index == 3) {
                smcpad_led_set(3, 0);
            } else if (hw_ev.index == 4) {
                smcpad_led_set(4, 0);
            }
            break;

        default:
            break;
        }
    }
#else
    /* Host simulation test queue drain */
    smc_event_t ev;
    if (smc_queue_pop(&g_event_queue, &ev)) {
        smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
    }
#endif
}

#ifdef SMC_HOST_TEST
int main(void) {
    setup();
    while (1) {
        loop();
    }
    return 0;
}
#endif
