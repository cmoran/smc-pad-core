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

typedef struct {
    uint8_t type;
    uint8_t index;
    int16_t value;
} smcpad_hw_event_t;
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

/* Note Repeat Engine state */
static bool                  g_note_repeat_enabled = false;
static uint16_t              g_held_pads_mask = 0;
static uint8_t               g_pad_velocities[SMC_NUM_PADS];
static uint32_t              g_last_repeat_tick = 0;

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

    /* 5. Set Initial Resting Pad Colors from Bank Palette */
    const smc_bank_palette_t *pal = smc_rgb_led_get_bank_palette(g_pad_bank);
#ifndef SMC_HOST_TEST
    for (int p = 0; p < SMC_NUM_PADS; p++) {
        smcpad_rgb_set(p, pal->idle_color);
    }
    smcpad_rgb_show();
#else
    smc_rgb_led_init();
    smc_rgb_led_set_bank_theme(g_pad_bank);
    smc_rgb_led_show();
#endif
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

    /* 2. Process Note Repeat rhythmic pulses (1/16th note = 125 ms) */
    if (g_note_repeat_enabled && g_held_pads_mask > 0) {
        if ((g_system_ticks - g_last_repeat_tick) >= 125) {
            g_last_repeat_tick = g_system_ticks;
            for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
                if (g_held_pads_mask & (1u << p)) {
                    smc_event_t ev_off = { .type = EVENT_PAD_RELEASE, .index = p, .value = 0 };
                    smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev_off);

                    smc_event_t ev_on = { .type = EVENT_PAD_STRIKE, .index = p, .value = g_pad_velocities[p] };
                    smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev_on);

                    smcpad_rgb_set(p, smc_rgb_led_get_bank_palette(g_pad_bank)->hit_color);
                }
            }
            smcpad_rgb_show();
        }
    }

    /* 3. Process hardware inputs (pads, buttons, encoders) */
    smcpad_hw_event_t hw_ev;
    while (smcpad_event_get(&hw_ev)) {
        smc_event_t ev;
        switch (hw_ev.type) {
        case 1: /* SMCPAD_EV_PAD_ON (Pad Strike) */
            if (g_midi_engine.shift_active) {
                /* Shift + Pad 0..3: Direct Bank Select */
                if (hw_ev.index < 4) {
                    g_pad_bank = hw_ev.index;
                    smc_config_set_pad_bank(g_pad_bank);
                    const smc_bank_palette_t *p_pal = smc_rgb_led_get_bank_palette(g_pad_bank);
                    for (int p = 0; p < SMC_NUM_PADS; p++) smcpad_rgb_set(p, p_pal->idle_color);
                    smcpad_rgb_show();
                    smcpad_led_set(1, (g_pad_bank > 0));
                } else if (hw_ev.index >= 4 && hw_ev.index < 8) {
                    /* Shift + Pad 4..7: Velocity Curve Select */
                    g_config.velocity_curve = hw_ev.index - 4;
                    smcpad_rgb_set(hw_ev.index, SMC_COLOR_WHITE);
                    smcpad_rgb_show();
                }
                break;
            }

            g_held_pads_mask |= (uint16_t)(1u << hw_ev.index);
            g_pad_velocities[hw_ev.index] = (uint8_t)hw_ev.value;

            ev.type = EVENT_PAD_STRIKE;
            ev.index = hw_ev.index;
            ev.value = hw_ev.value;
            smcpad_rgb_set(ev.index, smc_rgb_led_get_bank_palette(g_pad_bank)->hit_color);
            smcpad_rgb_show();
            smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            break;

        case 2: /* SMCPAD_EV_PAD_OFF (Pad Release) */
            g_held_pads_mask &= (uint16_t)~(1u << hw_ev.index);
            if (g_midi_engine.shift_active) {
                const smc_bank_palette_t *p_pal = smc_rgb_led_get_bank_palette(g_pad_bank);
                smcpad_rgb_set(hw_ev.index, p_pal->idle_color);
                smcpad_rgb_show();
                break;
            }

            ev.type = EVENT_PAD_RELEASE;
            ev.index = hw_ev.index;
            ev.value = 0;
            smcpad_rgb_set(ev.index, smc_rgb_led_get_bank_palette(g_pad_bank)->idle_color);
            smcpad_rgb_show();
            smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            break;

        case 5: /* SMCPAD_EV_ENCODER */
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
            } else if (hw_ev.index == 6 && g_midi_engine.shift_active) { /* Shift + Pause (Btn 6) = Panic */
                smc_midi_panic(&g_midi_engine);
                const smc_bank_palette_t *p_pal = smc_rgb_led_get_bank_palette(g_pad_bank);
                for (int p = 0; p < SMC_NUM_PADS; p++) smcpad_rgb_set(p, p_pal->idle_color);
                smcpad_rgb_show();
            } else if (hw_ev.index == 1) { /* Pad Bank (Btn 1) */
                g_pad_bank = (g_pad_bank + 1) & 3;
                smc_config_set_pad_bank(g_pad_bank);
                const smc_bank_palette_t *p_pal = smc_rgb_led_get_bank_palette(g_pad_bank);
                for (int p = 0; p < SMC_NUM_PADS; p++) smcpad_rgb_set(p, p_pal->idle_color);
                smcpad_rgb_show();
                smcpad_led_set(1, (g_pad_bank > 0));
            } else if (hw_ev.index == 2) { /* Knob Bank (Btn 2) */
                g_encoder_bank = (g_encoder_bank + 1) & 3;
                smc_config_set_encoder_bank(g_encoder_bank);
                smcpad_led_set(2, (g_encoder_bank > 0));
            } else if (hw_ev.index == 3) { /* Left Arrow (Btn 3) */
                smcpad_led_set(3, 1);
                if (g_midi_engine.shift_active) {
                    /* Shift + Left: Decrement MIDI channel (1..16) */
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
                    /* Shift + Right: Increment MIDI channel (1..16) */
                    g_channel_offset = (g_channel_offset < 0) ? 1 : (g_channel_offset + 1) & 15;
                    smc_midi_set_channel_override(&g_midi_engine, g_channel_offset);
                } else {
                    /* Right: Octave Up (+12 semitones) */
                    g_octave_offset += 12;
                    if (g_octave_offset > 48) g_octave_offset = 48;
                    smc_midi_set_octave_offset(&g_midi_engine, g_octave_offset);
                }
            } else if (hw_ev.index == 9) { /* Note Repeat (Btn 9) */
                g_note_repeat_enabled = !g_note_repeat_enabled;
                smcpad_led_set(9, g_note_repeat_enabled);
            }
            break;

        case 4: /* SMCPAD_EV_BUTTON_UP */
            if (hw_ev.index == 8) {
                g_midi_engine.shift_active = false;
                smcpad_led_set(8, 0);
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
