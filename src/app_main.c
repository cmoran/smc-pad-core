/**
 * @file app_main.c
 * @brief SMC-Pad Clean-Slate Open Firmware Entry Point (JieLi AC6966C / BR25)
 *
 * Implements a high-performance, low-latency, "dumb" MIDI controller pipeline:
 * - Hardware interrupt event loop servicing 16 velocity pads, 8 encoders, and buttons
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
    for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
        if (g_config.pads[bank][p].note == note) {
            uint32_t color = (vel > 0)
                ? ((g_config.led_mode == SMC_LED_MODE_HOST)
                    ? (((uint32_t)vel * 2u) << 8) | vel
                    : g_config.pad_hit_color)
                : g_config.pad_idle_color;
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
    /* Tell the bootloader that this application booted cleanly */
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

    /* 5. Set Initial Resting Pad Colors from Config */
#ifndef SMC_HOST_TEST
    for (int p = 0; p < SMC_NUM_PADS; p++) {
        smcpad_rgb_set(p, g_config.pad_idle_color);
    }
    smcpad_rgb_show();
#else
    smc_rgb_led_init();
    smc_rgb_led_set_all(g_config.pad_idle_color);
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

    /* 2. Process hardware inputs (pads, buttons, encoders) */
    smcpad_hw_event_t hw_ev;
    while (smcpad_event_get(&hw_ev)) {
        smc_event_t ev;
        switch (hw_ev.type) {
        case 1: /* SMCPAD_EV_PAD_ON (Pad Strike) */
            ev.type = EVENT_PAD_STRIKE;
            ev.index = hw_ev.index;
            ev.value = hw_ev.value;
            smcpad_rgb_set(ev.index, g_config.pad_hit_color);
            smcpad_rgb_show();
            smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            break;

        case 2: /* SMCPAD_EV_PAD_OFF (Pad Release) */
            ev.type = EVENT_PAD_RELEASE;
            ev.index = hw_ev.index;
            ev.value = 0;
            smcpad_rgb_set(ev.index, g_config.pad_idle_color);
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
            if (hw_ev.index == 8) { /* Shift (button 8) */
                g_midi_engine.shift_active = true;
                smcpad_led_set(8, 1);
            } else if (hw_ev.index == 6 && g_midi_engine.shift_active) { /* Shift + Pause (btn 6) = Panic */
                smc_midi_panic(&g_midi_engine);
                for (int p = 0; p < SMC_NUM_PADS; p++) smcpad_rgb_set(p, g_config.pad_idle_color);
                smcpad_rgb_show();
            } else if (hw_ev.index == 1) { /* Pad bank (btn 1) */
                g_pad_bank = (g_pad_bank + 1) & 3;
                smc_config_set_pad_bank(g_pad_bank);
                smcpad_led_set(1, (g_pad_bank > 0));
            } else if (hw_ev.index == 2) { /* Knob bank (btn 2) */
                g_encoder_bank = (g_encoder_bank + 1) & 3;
                smc_config_set_encoder_bank(g_encoder_bank);
                smcpad_led_set(2, (g_encoder_bank > 0));
            }
            break;

        case 4: /* SMCPAD_EV_BUTTON_UP */
            if (hw_ev.index == 8) {
                g_midi_engine.shift_active = false;
                smcpad_led_set(8, 0);
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
