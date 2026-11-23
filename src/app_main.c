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
extern void smcpad_yield(void);
extern int  smcpad_serial_write(const void *buf, int len);
extern int  smcpad_battery_mv(void);
extern int  smcpad_usb_power(void);

/* JieLi BLE Peripheral runtime declarations from smcpad_core.o */
enum {
    SMCPAD_BLE_CONNECTED = 1,
    SMCPAD_BLE_DISCONNECTED,
    SMCPAD_BLE_SUBSCRIBE,
};

typedef struct {
    uint16_t (*read)(uint16_t handle, uint16_t offset, uint8_t *buf, uint16_t size);
    void (*write)(uint16_t handle, uint16_t offset, const uint8_t *data, uint16_t len);
    void (*event)(int event, uint16_t handle, uint16_t value);
} smcpad_ble_callbacks_t;

extern void     smcpad_ble_begin(const smcpad_ble_callbacks_t *cb);
extern int      smcpad_ble_ready(void);
extern void     smcpad_ble_set_name(const char *name);
extern void     smcpad_ble_set_profile(const uint8_t *db, uint16_t len);
extern void     smcpad_ble_set_adv(const uint8_t *adv, uint8_t adv_len, const uint8_t *rsp, uint8_t rsp_len,
                                   uint16_t interval, int connectable);
extern void     smcpad_ble_advertise(int on);
extern uint16_t smcpad_ble_connection(void);
extern uint16_t smcpad_ble_mtu(void);
extern int      smcpad_ble_notify(uint16_t handle, const uint8_t *data, uint16_t len);
extern uint16_t smcpad_ble_queued(void);
extern void     smcpad_ble_set_conn_interval(uint16_t min, uint16_t max);
extern int      smcpad_ble_subscribed(uint16_t handle);
extern void     smcpad_ble_disconnect(void);

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

/* Hardware button/LED indices emitted by bsp/smcpad_core.o */
typedef enum {
    SMCPAD_BTN_BLUETOOTH   = 0,
    SMCPAD_BTN_PAD_BANK    = 1,
    SMCPAD_BTN_KNOB_BANK   = 2,
    SMCPAD_BTN_LEFT_ARROW  = 3,
    SMCPAD_BTN_RIGHT_ARROW = 4,
    SMCPAD_BTN_PLAY        = 5,
    SMCPAD_BTN_STOP        = 6,
    SMCPAD_BTN_REC         = 7,
    SMCPAD_BTN_SHIFT       = 8,
    SMCPAD_BTN_NOTE_REPEAT = 9
} smcpad_hw_btn_t;

/* Static system state structures (Strict zero dynamic allocation) */
static smc_event_queue_t     g_event_queue;
static smc_encoder_state_t   g_encoder_engine;
static smc_midi_dispatcher_t g_midi_engine;
static smc_config_t          g_config;
static smc_sysex_engine_t    g_sysex_engine;
static smc_ble_midi_t        g_ble_engine;

static volatile uint32_t     g_system_ticks = 0;
static uint8_t               g_preset_idx = 0;
static uint8_t               g_pad_bank = 0;
static uint8_t               g_encoder_bank = 0;
static int8_t                g_octave_offset = 0;
static int8_t                g_channel_offset = -1;

/* Note Repeat Swing Percentages (50% = Straight, up to 66%) */
static const uint8_t g_swing_percentages[5] = { 50, 54, 58, 62, 66 };

/* Note Repeat Engine state */
static bool                  g_note_repeat_enabled = false;
static bool                  g_note_repeat_cfg_mode = false;
static bool                  g_note_repeat_btn_held = false;
static bool                  g_repeat_sync_enabled = false;
static volatile uint32_t     g_midi_clock_ticks = 0;
static volatile bool         g_midi_clock_running = false;
static uint32_t              g_last_repeat_clock_tick = 0;
static uint32_t              g_last_clock_tick_ms = 0;
static uint16_t              g_clock_tick_interval_ms = 21; /* ~120 BPM (24 PPQN -> 20.8 ms/tick) */
static uint16_t              g_repeat_bpm = 120;        /* 30..300 BPM */
static uint8_t               g_repeat_rate_idx = 4;     /* Default 1/16 */
static uint8_t               g_repeat_swing_idx = 0;    /* 0..4 (50%, 54%, 58%, 62%, 66%) */
static bool                  g_repeat_latch_enabled = false;
static uint16_t              g_latched_pads_mask = 0;
static uint16_t              g_held_pads_mask = 0;
static uint8_t               g_pad_velocities[SMC_NUM_PADS];
static uint32_t              g_repeat_cycle_start_ms = 0;
static uint32_t              g_last_tap_time_ms = 0;
static uint16_t              g_repeat_current_period_ms = 125;
static bool                  g_repeat_gate_active = false;
static uint8_t               g_repeat_swing_step = 0;

/* Battery monitoring state */
static uint32_t              g_last_battery_check_ms = 0;
static bool                  g_low_battery_alert = false;

/* Rotary Encoder Step Accumulators for Knobs 1-2 */
static int8_t                g_enc_rate_accum = 0;
static int8_t                g_enc_swing_accum = 0;

static void app_midi_clock_cb(uint8_t msg_type, void *ctx) {
    (void)ctx;
    if (msg_type == 0xF8u) { /* Timing Clock (24 PPQN) */
        uint32_t now = app_get_millis();
        if (g_last_clock_tick_ms > 0) {
            uint32_t dt = now - g_last_clock_tick_ms;
            if (dt >= 4u && dt <= 200u) {
                /* Smooth external clock tick interval (ms per 24-PPQN pulse) */
                g_clock_tick_interval_ms = (uint16_t)(((uint32_t)g_clock_tick_interval_ms * 3u + dt) / 4u);
            }
        }
        g_last_clock_tick_ms = now;
        g_midi_clock_ticks++;
    } else if (msg_type == 0xFAu) { /* Start */
        g_midi_clock_ticks = 0;
        g_last_repeat_clock_tick = 0;
        g_last_clock_tick_ms = 0;
        g_midi_clock_running = true;
    } else if (msg_type == 0xFBu) { /* Continue */
        g_midi_clock_running = true;
    } else if (msg_type == 0xFCu) { /* Stop */
        g_midi_clock_running = false;
    }
}

static uint16_t get_repeat_clock_ticks(uint8_t rate_idx) {
    switch (rate_idx & 0x07u) {
        case 0: return 24; /* 1/4 */
        case 1: return 16; /* 1/4t */
        case 2: return 12; /* 1/8 */
        case 3: return 8;  /* 1/8t */
        case 4: return 6;  /* 1/16 */
        case 5: return 4;  /* 1/16t */
        case 6: return 3;  /* 1/32 */
        case 7: return 2;  /* 1/32t */
        default: return 6;
    }
}

static uint16_t get_repeat_base_period_ms(void) {
    uint32_t bpm = g_repeat_bpm;
    if (bpm < 30) bpm = 30;
    if (bpm > 300) bpm = 300;
    uint32_t quarter_ms = 60000u / bpm;

    switch (g_repeat_rate_idx & 0x07u) {
        case 0: return (uint16_t)quarter_ms;                    /* 1/4 */
        case 1: return (uint16_t)((quarter_ms * 2u) / 3u);      /* 1/4t */
        case 2: return (uint16_t)(quarter_ms / 2u);             /* 1/8 */
        case 3: return (uint16_t)(quarter_ms / 3u);             /* 1/8t */
        case 4: return (uint16_t)(quarter_ms / 4u);             /* 1/16 */
        case 5: return (uint16_t)(quarter_ms / 6u);             /* 1/16t */
        case 6: return (uint16_t)(quarter_ms / 8u);             /* 1/32 */
        case 7: return (uint16_t)(quarter_ms / 12u);            /* 1/32t */
        default: return (uint16_t)(quarter_ms / 4u);
    }
}

static uint16_t get_current_repeat_period_ms(void) {
    uint16_t base = get_repeat_base_period_ms();
    uint8_t swing_pct = g_swing_percentages[g_repeat_swing_idx % 5];
    if (swing_pct == 50) {
        return base;
    }
    if (g_repeat_swing_step == 0) {
        return (uint16_t)(((uint32_t)base * 2u * (uint32_t)swing_pct) / 100u);
    } else {
        return (uint16_t)(((uint32_t)base * 2u * (100u - (uint32_t)swing_pct)) / 100u);
    }
}

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

static void hal_send_uart_midi(const smc_midi_raw_msg_t *msg) {
    if (!msg || msg->len == 0) return;
#ifndef SMC_HOST_TEST
    smcpad_serial_write(msg->bytes, (int)msg->len);
#endif
}

#define SMC_BLE_MIDI_HANDLE_VAL   12
#define SMC_BLE_GAP_NAME_HANDLE   3
#define SMC_BLE_RX_BUF_SIZE       128u

typedef struct {
    uint8_t buffer[SMC_BLE_RX_BUF_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} smc_ble_rx_fifo_t;

static smc_ble_rx_fifo_t     g_ble_rx_fifo;
static bool                  g_ble_enabled = true;
static bool                  g_ble_initialized = false;

/* Standard BTstack ATT database: GAP, GATT, and Apple/MMA BLE-MIDI GATT Service */
static const uint8_t s_ble_midi_profile_db[180] = {
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18, 0x0d, 0x00,
    0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x02, 0x03, 0x00, 0x00, 0x2a, 0x08,
    0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x2a, 0x0d, 0x00, 0x02, 0x00, 0x04,
    0x00, 0x03, 0x28, 0x02, 0x05, 0x00, 0x01, 0x2a, 0x0a, 0x00, 0x02, 0x00,
    0x05, 0x00, 0x01, 0x2a, 0x00, 0x00, 0x0a, 0x00, 0x02, 0x00, 0x06, 0x00,
    0x00, 0x28, 0x01, 0x18, 0x0d, 0x00, 0x02, 0x00, 0x07, 0x00, 0x03, 0x28,
    0x20, 0x08, 0x00, 0x05, 0x2a, 0x08, 0x00, 0x20, 0x01, 0x08, 0x00, 0x05,
    0x2a, 0x0a, 0x00, 0x0a, 0x01, 0x09, 0x00, 0x02, 0x29, 0x00, 0x00, 0x18,
    0x00, 0x02, 0x00, 0x0a, 0x00, 0x00, 0x28, 0x00, 0xc7, 0xc4, 0x4e, 0xe3,
    0x6c, 0x51, 0xa7, 0x33, 0x4b, 0xe8, 0xed, 0x5a, 0x0e, 0xb8, 0x03, 0x1b,
    0x00, 0x02, 0x00, 0x0b, 0x00, 0x03, 0x28, 0x16, 0x0c, 0x00, 0xf3, 0x6b,
    0x10, 0x9d, 0x66, 0xf2, 0xa9, 0xa1, 0x12, 0x41, 0x68, 0x38, 0xdb, 0xe5,
    0x72, 0x77, 0x16, 0x00, 0x16, 0x03, 0x0c, 0x00, 0xf3, 0x6b, 0x10, 0x9d,
    0x66, 0xf2, 0xa9, 0xa1, 0x12, 0x41, 0x68, 0x38, 0xdb, 0xe5, 0x72, 0x77,
    0x0a, 0x00, 0x0a, 0x01, 0x0d, 0x00, 0x02, 0x29, 0x00, 0x00, 0x00, 0x00
};

/* Advertising packet: Flags (0x06) + 128-bit Apple/MMA Service UUID */
static const uint8_t s_ble_adv_data[21] = {
    0x02, 0x01, 0x06,
    0x11, 0x07,
    0x00, 0xc7, 0xc4, 0x4e, 0xe3, 0x6c, 0x51, 0xa7,
    0x33, 0x4b, 0xe8, 0xed, 0x5a, 0x0e, 0xb8, 0x03
};

/* Scan Response packet: Complete Local Name "SMC-PAD" */
static const uint8_t s_ble_rsp_data[9] = {
    0x08, 0x09, 'S', 'M', 'C', '-', 'P', 'A', 'D'
};

#ifndef SMC_HOST_TEST
static uint16_t ble_cb_read(uint16_t handle, uint16_t offset, uint8_t *buf, uint16_t size) {
    if (handle == SMC_BLE_GAP_NAME_HANDLE) {
        static const char dev_name[] = "SMC-PAD";
        uint16_t len = (uint16_t)(sizeof(dev_name) - 1);
        if (!buf) return len;
        if (offset >= len) return 0;
        uint16_t rem = len - offset;
        if (size > rem) size = rem;
        for (uint16_t i = 0; i < size; i++) {
            buf[i] = (uint8_t)dev_name[offset + i];
        }
        return size;
    }
    return 0;
}

static void hal_ble_rx_on_byte(uint8_t byte, void *ctx) {
    (void)ctx;
    uint32_t next = (g_ble_rx_fifo.tail + 1u) & (SMC_BLE_RX_BUF_SIZE - 1u);
    if (next != g_ble_rx_fifo.head) {
        g_ble_rx_fifo.buffer[g_ble_rx_fifo.tail] = byte;
        __asm__ __volatile__("" ::: "memory");
        g_ble_rx_fifo.tail = next;
    }
}

static void ble_cb_write(uint16_t handle, uint16_t offset, const uint8_t *data, uint16_t len) {
    if (handle == SMC_BLE_MIDI_HANDLE_VAL && offset == 0 && data && len > 0) {
        smc_ble_midi_parse_rx(data, len, hal_ble_rx_on_byte, NULL);
    }
}

static void ble_cb_event(int event, uint16_t handle, uint16_t value) {
    switch (event) {
    case SMCPAD_BLE_CONNECTED:
        smc_ble_midi_set_connected(&g_ble_engine, true);
        /* Request low-latency 7.5ms - 15ms connection interval */
        smcpad_ble_set_conn_interval(6, 12);
        break;
    case SMCPAD_BLE_DISCONNECTED:
        smc_ble_midi_set_connected(&g_ble_engine, false);
        smc_ble_midi_set_subscribed(&g_ble_engine, false);
        if (g_ble_enabled) {
            smcpad_ble_advertise(1);
        }
        break;
    case SMCPAD_BLE_SUBSCRIBE:
        if (handle == SMC_BLE_MIDI_HANDLE_VAL) {
            bool sub = (value & 1) != 0;
            smc_ble_midi_set_subscribed(&g_ble_engine, sub);
            smc_ble_midi_set_mtu(&g_ble_engine, smcpad_ble_mtu());
        }
        break;
    default:
        break;
    }
}

static const smcpad_ble_callbacks_t s_ble_callbacks = {
    .read  = ble_cb_read,
    .write = ble_cb_write,
    .event = ble_cb_event
};
#endif

static bool hal_ble_raw_send(const uint8_t *data, uint16_t len) {
#ifndef SMC_HOST_TEST
    if (!g_ble_enabled || !g_ble_engine.subscribed) {
        return false;
    }
    if (smcpad_ble_queued() > 0) {
        return false;
    }
    return (smcpad_ble_notify(SMC_BLE_MIDI_HANDLE_VAL, data, len) == 1);
#else
    (void)data; (void)len;
    return true;
#endif
}

static void hal_send_ble_packet(const smc_midi_raw_msg_t *msg) {
    if (msg && msg->len > 0) {
        smc_ble_midi_send_message(&g_ble_engine, msg->bytes, msg->len, app_get_millis());
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
    smc_ble_midi_send_sysex(&g_ble_engine, data, (uint16_t)len, app_get_millis());
}

static uint32_t get_pad_idle_color(uint8_t p);
static void restore_bank_leds(void);

static void app_sysex_config_changed(uint8_t cmd, void *ctx) {
    (void)ctx;
    if (cmd == SMC_CMD_SET_SCALE_CFG || cmd == SMC_CMD_SET_RGB_CFG || cmd == SMC_CMD_FACTORY_RESET) {
        if (!g_midi_engine.shift_active && !g_note_repeat_cfg_mode) {
            restore_bank_leds();
        }
    }
}

static void hal_host_note_feedback(uint8_t ch, uint8_t note, uint8_t vel, void *ctx) {
    (void)ch;
    (void)ctx;
    uint8_t bank = smc_config_get_pad_bank();
    const smc_bank_palette_t *pal = smc_rgb_led_get_bank_palette(bank);
    bool updated = false;

    for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
        if (g_config.pads[bank][p].note == note) {
            uint32_t color = (vel > 0)
                ? ((g_config.led_mode == SMC_LED_MODE_HOST)
                    ? (((uint32_t)vel * 2u) << 8) | vel
                    : pal->hit_color)
                : get_pad_idle_color(p);
#ifndef SMC_HOST_TEST
            smcpad_rgb_set(p, color);
#else
            smc_rgb_led_set_pad(p, color);
#endif
            updated = true;
        }
    }

    if (updated) {
#ifndef SMC_HOST_TEST
        smcpad_rgb_show();
#else
        smc_rgb_led_show();
#endif
    }
}

static void hal_host_sysex_feed(uint8_t b, void *ctx) {
    (void)ctx;
    smc_sysex_feed_byte(&g_sysex_engine, b);
}

/* ========================================================================= */
/* Application Entry Points (setup / loop)                                   */
/* ========================================================================= */

static uint32_t get_pad_idle_color(uint8_t p) {
    if (g_config.scale.enabled) {
        uint8_t base = g_config.pads[g_pad_bank][0].note;
        uint8_t note = smc_scale_get_pad_note(&g_config.scale, p, base);
        uint8_t st = smc_scale_check_note(&g_config.scale, note);
        if (st == 2) return SMC_COLOR_SCALE_ROOT;
        if (st == 1) return SMC_COLOR_SCALE_NOTE;
        return SMC_COLOR_SCALE_DIM;
    }
    return smc_rgb_led_get_bank_palette(g_pad_bank)->idle_color;
}

static void restore_bank_leds(void) {
    if (g_config.scale.enabled) {
        uint8_t notes[SMC_NUM_PADS];
        uint8_t base = g_config.pads[g_pad_bank][0].note;
        for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
            notes[p] = smc_scale_get_pad_note(&g_config.scale, p, base);
        }
        smc_rgb_led_render_scale(&g_config.scale, notes);
#ifndef SMC_HOST_TEST
        for (int p = 0; p < SMC_NUM_PADS; p++) {
            smcpad_rgb_set(p, smc_rgb_led_get_pad(p));
        }
        smcpad_rgb_show();
#else
        smc_rgb_led_show();
#endif
        return;
    }

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
    uint8_t preset_pad = (g_pad_bank ? 4u : 0u) + (g_preset_idx & 0x03u);
    smc_rgb_led_render_shift_status(preset_pad, (uint8_t)g_config.velocity_curve, octave, semi);
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
    /* Pads 1-8 (indices 0..7): Rate Divisions (Dim Blue / Bright White) */
    for (uint8_t p = 0; p < 8; p++) {
        smcpad_rgb_set(p, (p == g_repeat_rate_idx) ? SMC_COLOR_WHITE : 0x000030u);
    }
    /* Pads 9-13 (indices 8..12): Swing (Dim Yellow / Bright Yellow) */
    for (uint8_t s = 0; s < 5; s++) {
        smcpad_rgb_set(8 + s, (s == g_repeat_swing_idx) ? 0xFFFF00u : 0x202000u);
    }
    /* Pad 14 (index 13): Latch (Dim Green / Bright Green) */
    smcpad_rgb_set(13, g_repeat_latch_enabled ? 0x00FF00u : 0x002000u);

    /* Pad 15 (index 14): Sync (Dim Cyan / Bright Cyan) */
    smcpad_rgb_set(14, g_repeat_sync_enabled ? 0x00FFFFu : 0x002020u);

    /* Pad 16 (index 15): Tap Tempo (Coral Red) */
    smcpad_rgb_set(15, 0xFF0040u);

    smcpad_rgb_show();
#endif
}

static void app_set_pad_rgb(uint8_t pad, uint32_t color) {
    smcpad_rgb_set(pad, color);
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
    smc_sysex_set_change_callback(&g_sysex_engine, app_sysex_config_changed, NULL);

    /* 3. Initialize BLE-MIDI Engine & Radio Peripheral */
    smc_ble_midi_init(&g_ble_engine, hal_ble_raw_send);
#ifndef SMC_HOST_TEST
    smcpad_ble_set_name("SMC-PAD");
    smcpad_ble_begin(&s_ble_callbacks);
    uint32_t t0 = smcpad_millis();
    while (!smcpad_ble_ready() && (smcpad_millis() - t0 < 1000)) {
        smcpad_delay(5);
    }
    smcpad_ble_set_profile(s_ble_midi_profile_db, sizeof(s_ble_midi_profile_db));
    smcpad_ble_set_adv(s_ble_adv_data, sizeof(s_ble_adv_data),
                       s_ble_rsp_data, sizeof(s_ble_rsp_data),
                       48, 1);
    smcpad_ble_advertise(1);
    g_ble_enabled = true;
    g_ble_initialized = true;
#endif

    /* 4. Initialize MIDI Dispatcher */
    smc_midi_init(&g_midi_engine, hal_send_midi_packet);
    smc_midi_set_config(&g_midi_engine, &g_config);
    smc_midi_set_ble_sink(&g_midi_engine, hal_send_ble_packet);
    smc_midi_set_uart_sink(&g_midi_engine, hal_send_uart_midi);
    smc_midi_set_clock_callback(&g_midi_engine, app_midi_clock_cb, NULL);
    smc_midi_set_host_callbacks(&g_midi_engine, hal_host_note_feedback, hal_host_sysex_feed, NULL);

    /* 5. Startup Boot Animation */
#ifndef SMC_HOST_TEST
    smc_rgb_led_boot_animation(app_set_pad_rgb, smcpad_rgb_show, smcpad_delay);
#else
    smc_rgb_led_init();
    smc_rgb_led_boot_animation(smc_rgb_led_set_pad, smc_rgb_led_show, NULL);
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
        if (cin == 0x03 || cin == 0x04 || cin == 0x07 || cin == 0x08 ||
            cin == 0x09 || cin == 0x0A || cin == 0x0B || cin == 0x0E) {
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[1]);
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[2]);
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[3]);
        } else if (cin == 0x02 || cin == 0x06 || cin == 0x0C || cin == 0x0D) {
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[1]);
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[2]);
        } else if (cin == 0x05 || cin == 0x0F) {
            smc_midi_rx_feed_byte(&g_midi_engine, rx_pkt[1]);
        }
    }

    /* 1b. Drain incoming BLE-MIDI bytes */
    while (g_ble_rx_fifo.head != g_ble_rx_fifo.tail) {
        uint8_t b = g_ble_rx_fifo.buffer[g_ble_rx_fifo.head];
        __asm__ __volatile__("" ::: "memory");
        g_ble_rx_fifo.head = (g_ble_rx_fifo.head + 1u) & (SMC_BLE_RX_BUF_SIZE - 1u);
        smc_midi_rx_feed_byte(&g_midi_engine, b);
    }

    /* 2. Process Note Repeat rhythmic pulses (Gated duty cycle to prevent polyphony overload) */
    uint16_t active_repeat_mask = g_held_pads_mask | g_latched_pads_mask;
    if (g_note_repeat_enabled && active_repeat_mask > 0 && !g_note_repeat_cfg_mode) {
        uint32_t now = app_get_millis();
        uint16_t step_period_ms = g_repeat_current_period_ms;
        if (g_repeat_sync_enabled) {
            uint16_t clock_ticks_per_step = get_repeat_clock_ticks(g_repeat_rate_idx);
            step_period_ms = (uint16_t)((uint32_t)g_clock_tick_interval_ms * clock_ticks_per_step);
            if (step_period_ms < 20u) step_period_ms = 20u;
        }
        uint16_t gate_ms = (uint16_t)((uint32_t)step_period_ms * 60u / 100u);
        if (gate_ms < 12) gate_ms = 12;

        bool trigger_step = false;
        if (g_repeat_sync_enabled) {
            uint16_t clock_ticks_per_step = get_repeat_clock_ticks(g_repeat_rate_idx);
            if (!g_repeat_gate_active && g_midi_clock_running) {
                if ((g_last_repeat_clock_tick == 0 && g_midi_clock_ticks > 0) ||
                    (g_midi_clock_ticks - g_last_repeat_clock_tick >= clock_ticks_per_step)) {
                    trigger_step = true;
                    g_last_repeat_clock_tick = g_midi_clock_ticks;
                }
            }
        } else {
            if (!g_repeat_gate_active) {
                if ((now - g_repeat_cycle_start_ms >= g_repeat_current_period_ms) || (g_repeat_cycle_start_ms == 0)) {
                    trigger_step = true;
                }
            }
        }

        if (trigger_step) {
            g_repeat_current_period_ms = get_current_repeat_period_ms();
            g_repeat_cycle_start_ms = now;
            g_repeat_gate_active = true;

            for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
                if (active_repeat_mask & (1u << p)) {
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
        } else if (g_repeat_gate_active) {
            /* Gate active: check if gate interval elapsed */
            if (now - g_repeat_cycle_start_ms >= gate_ms) {
                g_repeat_gate_active = false;
                g_repeat_swing_step = (g_repeat_swing_step + 1) & 1u;

                for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
                    if (active_repeat_mask & (1u << p)) {
                        smc_event_t ev_off = {
                            .type = EVENT_PAD_RELEASE,
                            .index = p,
                            .value = 0,
                            .timestamp = now
                        };
                        smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev_off);
                        smcpad_rgb_set(p, get_pad_idle_color(p));
                    }
                }
                smcpad_rgb_show();
            }
        }
    } else if (active_repeat_mask == 0) {
        g_last_repeat_clock_tick = 0;
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
                    /* Pads 1-8 (indices 0..7): Rate Divisions (1/4 to 1/32t) */
                    g_repeat_rate_idx = hw_ev.index;
                    refresh_repeat_cfg_leds();
                } else if (hw_ev.index >= 8 && hw_ev.index <= 12) {
                    /* Pads 9-13 (indices 8..12): Swing (50%, 54%, 58%, 62%, 66%) */
                    g_repeat_swing_idx = hw_ev.index - 8;
                    refresh_repeat_cfg_leds();
                } else if (hw_ev.index == 13) {
                    /* Pad 14 (index 13): Latch Toggle */
                    g_repeat_latch_enabled = !g_repeat_latch_enabled;
                    if (!g_repeat_latch_enabled) {
                        g_latched_pads_mask = 0;
                    }
                    refresh_repeat_cfg_leds();
                } else if (hw_ev.index == 14) {
                    /* Pad 15 (index 14): Sync */
                    g_repeat_sync_enabled = !g_repeat_sync_enabled;
                    g_last_repeat_clock_tick = 0;
                    refresh_repeat_cfg_leds();
                } else if (hw_ev.index == 15) {
                    /* Pad 16 (index 15): Tap Tempo */
                    uint32_t now = app_get_millis();
                    if (g_last_tap_time_ms > 0) {
                        uint32_t delta = now - g_last_tap_time_ms;
                        if (delta >= 200 && delta <= 2000) {
                            g_repeat_bpm = (uint16_t)(60000u / delta);
                            if (g_repeat_bpm < 30) g_repeat_bpm = 30;
                            if (g_repeat_bpm > 300) g_repeat_bpm = 300;
                        }
                    }
                    g_last_tap_time_ms = now;
#ifndef SMC_HOST_TEST
                    smcpad_rgb_set(15, SMC_COLOR_WHITE);
                    smcpad_rgb_show();
#endif
                }
                break;
            }

            /* If Shift is active: execute stock Shift shortcuts instead of emitting Note-On */
            if (g_midi_engine.shift_active) {
                if (hw_ev.index < 8) {
                    /* Shift + Pads 1-4 (indices 0..3): Switch Preset (Bank A, PAD BANK LED off) */
                    /* Shift + Pads 5-8 (indices 4..7): Switch Preset (Bank B, PAD BANK LED on) */
                    g_preset_idx = hw_ev.index % 4;
                    g_pad_bank = (hw_ev.index >= 4) ? 1 : 0;
                    smc_config_set_pad_bank(g_pad_bank);
                    smcpad_led_set(1, g_pad_bank);
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
            /* Calculate velocity shaping if applicable */
            uint8_t vel = (uint8_t)hw_ev.value;
            if (g_config.velocity_curve == VELOCITY_CURVE_FIXED) {
                vel = 127;
            } else if (g_config.velocity_curve != VELOCITY_CURVE_LINEAR) {
                vel = smc_calculate_velocity(vel, 1, 127, g_config.velocity_curve);
            }
            g_pad_velocities[hw_ev.index] = vel;

            if (g_note_repeat_enabled) {
                if (g_repeat_latch_enabled) {
                    /* Toggle latch state for this pad */
                    if (g_latched_pads_mask & (1u << hw_ev.index)) {
                        g_latched_pads_mask &= (uint16_t)~(1u << hw_ev.index);
                        g_held_pads_mask &= (uint16_t)~(1u << hw_ev.index);
                        ev.type = EVENT_PAD_RELEASE;
                        ev.index = hw_ev.index;
                        ev.value = 0;
                        ev.timestamp = app_get_millis();
                        smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
                        smcpad_rgb_set(hw_ev.index, get_pad_idle_color(hw_ev.index));
                        smcpad_rgb_show();
                    } else {
                        g_latched_pads_mask |= (uint16_t)(1u << hw_ev.index);
                        g_held_pads_mask |= (uint16_t)(1u << hw_ev.index);
                    }
                } else {
                    g_held_pads_mask |= (uint16_t)(1u << hw_ev.index);
                }

                if (!g_repeat_gate_active) {
                    g_repeat_cycle_start_ms = 0;
                }
            } else {
                g_held_pads_mask |= (uint16_t)(1u << hw_ev.index);
                ev.type = EVENT_PAD_STRIKE;
                ev.index = hw_ev.index;
                ev.value = vel;
                ev.timestamp = app_get_millis();
                smcpad_rgb_set(ev.index, smc_rgb_led_get_bank_palette(g_pad_bank)->hit_color);
                smcpad_rgb_show();
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            }
            break;

        case 2: /* SMCPAD_EV_PAD_OFF (Pad Release) */
            g_held_pads_mask &= (uint16_t)~(1u << hw_ev.index);

            if (g_note_repeat_cfg_mode) {
                if (hw_ev.index == 15) {
                    smcpad_rgb_set(15, 0xFF0040u);
                    smcpad_rgb_show();
                }
                break;
            }

            if (g_midi_engine.shift_active) {
                refresh_shift_status_view();
                break;
            }

            if (g_note_repeat_enabled) {
                /* If pad is latched, keep repeating after physical release */
                if (g_latched_pads_mask & (1u << hw_ev.index)) {
                    break;
                }

                /* Release non-latched pad */
                ev.type = EVENT_PAD_RELEASE;
                ev.index = hw_ev.index;
                ev.value = 0;
                ev.timestamp = app_get_millis();
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
                smcpad_rgb_set(ev.index, get_pad_idle_color(ev.index));
                smcpad_rgb_show();

                uint16_t active_mask = g_held_pads_mask | g_latched_pads_mask;
                if (active_mask == 0) {
                    g_repeat_gate_active = false;
                    g_repeat_cycle_start_ms = 0;
                    g_repeat_swing_step = 0;
                }
            } else {
                ev.type = EVENT_PAD_RELEASE;
                ev.index = hw_ev.index;
                ev.value = 0;
                ev.timestamp = app_get_millis();
                smcpad_rgb_set(ev.index, get_pad_idle_color(ev.index));
                smcpad_rgb_show();
                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            }
            break;

        case 5: /* SMCPAD_EV_ENCODER */
            /* If Note Repeat button is held or in repeat config mode, Knobs 1-4 adjust repeat parameters */
            if ((g_note_repeat_btn_held || g_note_repeat_cfg_mode) && hw_ev.index < 4) {
                if (hw_ev.index == 0) {
                    /* Knob 1: Rate (with 2-detent step accumulator) */
                    g_enc_rate_accum += (int8_t)hw_ev.value;
                    if (g_enc_rate_accum >= 2) {
                        if (g_repeat_rate_idx < 7) g_repeat_rate_idx++;
                        g_enc_rate_accum = 0;
                    } else if (g_enc_rate_accum <= -2) {
                        if (g_repeat_rate_idx > 0) g_repeat_rate_idx--;
                        g_enc_rate_accum = 0;
                    }
                    if (g_note_repeat_cfg_mode) {
                        refresh_repeat_cfg_leds();
                    }
                } else if (hw_ev.index == 1) {
                    /* Knob 2: Swing (with 2-detent step accumulator) */
                    g_enc_swing_accum += (int8_t)hw_ev.value;
                    if (g_enc_swing_accum >= 2) {
                        if (g_repeat_swing_idx < 4) g_repeat_swing_idx++;
                        g_enc_swing_accum = 0;
                    } else if (g_enc_swing_accum <= -2) {
                        if (g_repeat_swing_idx > 0) g_repeat_swing_idx--;
                        g_enc_swing_accum = 0;
                    }
                    if (g_note_repeat_cfg_mode) {
                        refresh_repeat_cfg_leds();
                    }
                } else if (hw_ev.index == 2) {
                    /* Knob 3: Tempo / BPM (30..300) */
                    int32_t new_bpm = (int32_t)g_repeat_bpm + (int32_t)hw_ev.value;
                    if (new_bpm < 30) new_bpm = 30;
                    if (new_bpm > 300) new_bpm = 300;
                    g_repeat_bpm = (uint16_t)new_bpm;
                } else if (hw_ev.index == 3) {
                    /* Knob 4: Latch Toggle */
                    if (hw_ev.value > 0) {
                        g_repeat_latch_enabled = true;
                    } else if (hw_ev.value < 0) {
                        g_repeat_latch_enabled = false;
                        g_latched_pads_mask = 0;
                    }
                    if (g_note_repeat_cfg_mode) {
                        refresh_repeat_cfg_leds();
                    }
                }
                break;
            }

            ev.type = EVENT_ENCODER_DELTA;
            ev.index = hw_ev.index;
            ev.value = hw_ev.value;
            if (ev.index < SMC_NUM_ENCODERS) {
                uint8_t bank = smc_config_get_encoder_bank();
                const smc_encoder_cfg_t *e_cfg = smc_config_get_active_encoder(&g_config, ev.index);
                int32_t step = (e_cfg && e_cfg->step_size > 0) ? (int32_t)e_cfg->step_size : 1;
                int32_t delta = (int32_t)ev.value * step;
                if (g_encoder_engine.acceleration_enabled) {
                    uint32_t now_enc = app_get_millis();
                    uint32_t dt = now_enc - g_encoder_engine.last_tick_timestamp[ev.index];
                    g_encoder_engine.last_tick_timestamp[ev.index] = now_enc;
                    if (dt < 25u) {
                        delta *= 4;
                    } else if (dt < 60u) {
                        delta *= 2;
                    }
                }
                ev.value = (int16_t)delta;
                int32_t min_v = e_cfg ? (int32_t)e_cfg->min_val : 0;
                int32_t max_v = e_cfg ? (int32_t)e_cfg->max_val : 127;
                int32_t val = (int32_t)g_encoder_engine.configs[bank][ev.index].value + delta;
                if (val < min_v) val = min_v;
                if (val > max_v) val = max_v;
                g_encoder_engine.configs[bank][ev.index].value = (uint8_t)val;
            }
            smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev);
            break;

        case 3: /* SMCPAD_EV_BUTTON_DOWN */
            if (hw_ev.index == SMCPAD_BTN_BLUETOOTH) {
                g_ble_enabled = !g_ble_enabled;
                if (g_ble_enabled) {
#ifndef SMC_HOST_TEST
                    if (!g_ble_initialized) {
                        smcpad_ble_set_name("SMC-PAD");
                        smcpad_ble_begin(&s_ble_callbacks);
                        uint32_t t0 = smcpad_millis();
                        while (!smcpad_ble_ready() && (smcpad_millis() - t0 < 1000)) {
                            smcpad_delay(5);
                        }
                        smcpad_ble_set_profile(s_ble_midi_profile_db, sizeof(s_ble_midi_profile_db));
                        smcpad_ble_set_adv(s_ble_adv_data, sizeof(s_ble_adv_data),
                                           s_ble_rsp_data, sizeof(s_ble_rsp_data),
                                           48, 1);
                        g_ble_initialized = true;
                    }
                    smcpad_ble_advertise(1);
#endif
                } else {
#ifndef SMC_HOST_TEST
                    smcpad_ble_advertise(0);
                    smcpad_ble_disconnect();
#endif
                    smc_ble_midi_set_connected(&g_ble_engine, false);
                    smc_ble_midi_set_subscribed(&g_ble_engine, false);
                    smcpad_led_set(SMCPAD_BTN_BLUETOOTH, 0);
                }
            } else if (hw_ev.index == SMCPAD_BTN_SHIFT) {
                g_midi_engine.shift_active = true;
                smcpad_led_set(SMCPAD_BTN_SHIFT, 1);
                refresh_shift_status_view();
            } else if (hw_ev.index == SMCPAD_BTN_NOTE_REPEAT) {
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
                    smcpad_led_set(SMCPAD_BTN_NOTE_REPEAT, g_note_repeat_enabled);
                    if (!g_note_repeat_enabled) {
                        uint16_t all_active = g_held_pads_mask | g_latched_pads_mask;
                        for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
                            if (all_active & (1u << p)) {
                                smc_event_t ev_off = {
                                    .type = EVENT_PAD_RELEASE,
                                    .index = p,
                                    .value = 0,
                                    .timestamp = app_get_millis()
                                };
                                smc_midi_process_event(&g_midi_engine, &g_encoder_engine, &ev_off);
                                smcpad_rgb_set(p, get_pad_idle_color(p));
                            }
                        }
                        smcpad_rgb_show();
                        g_latched_pads_mask = 0;
                        g_repeat_gate_active = false;
                        g_repeat_cycle_start_ms = 0;
                        g_repeat_swing_step = 0;
                    }
                }
            } else if (hw_ev.index == SMCPAD_BTN_PLAY) {
                smcpad_led_set(SMCPAD_BTN_PLAY, 1);
                /* Emit Mackie Play Note 94 (0x5E), vel 127, ch 0 */
                smc_midi_raw_msg_t play_msg = { .bytes = { 0x90, 0x5E, 127 }, .len = 3 };
                smc_midi_send_raw(&g_midi_engine, &play_msg);
            } else if (hw_ev.index == SMCPAD_BTN_STOP) {
                if (g_midi_engine.shift_active) {
                    /* Shift + Stop = Panic */
                    smc_midi_panic(&g_midi_engine);
                    restore_bank_leds();
                } else {
                    smcpad_led_set(SMCPAD_BTN_STOP, 1);
                    /* Emit Mackie Stop Note 93 (0x5D), vel 127, ch 0 */
                    smc_midi_raw_msg_t stop_msg = { .bytes = { 0x90, 0x5D, 127 }, .len = 3 };
                    smc_midi_send_raw(&g_midi_engine, &stop_msg);
                }
            } else if (hw_ev.index == SMCPAD_BTN_REC) {
                smcpad_led_set(SMCPAD_BTN_REC, 1);
                /* Emit Mackie Record Note 95 (0x5F), vel 127, ch 0 */
                smc_midi_raw_msg_t rec_msg = { .bytes = { 0x90, 0x5F, 127 }, .len = 3 };
                smc_midi_send_raw(&g_midi_engine, &rec_msg);
            } else if (hw_ev.index == SMCPAD_BTN_PAD_BANK) {
                g_pad_bank = (uint8_t)((g_pad_bank + 1u) & 0x03u);
                smc_config_set_pad_bank(g_pad_bank);
                restore_bank_leds();
                smcpad_led_set(SMCPAD_BTN_PAD_BANK, g_pad_bank != 0);
            } else if (hw_ev.index == SMCPAD_BTN_KNOB_BANK) {
                g_encoder_bank = (uint8_t)((g_encoder_bank + 1u) & 0x03u);
                smc_config_set_encoder_bank(g_encoder_bank);
                smc_encoder_set_bank(&g_encoder_engine, g_encoder_bank);
                smcpad_led_set(SMCPAD_BTN_KNOB_BANK, g_encoder_bank != 0);
            } else if (hw_ev.index == SMCPAD_BTN_LEFT_ARROW) {
                smcpad_led_set(SMCPAD_BTN_LEFT_ARROW, 1);
                if (g_preset_idx == 1 && !g_midi_engine.shift_active) {
                    /* Preset 2 (DAW mode): Mackie Bank Left Note 46 (0x2E) */
                    smc_midi_raw_msg_t bleft = { .bytes = { 0x90, 0x2E, 127 }, .len = 3 };
                    smc_midi_send_raw(&g_midi_engine, &bleft);
                } else if (g_midi_engine.shift_active) {
                    /* Shift + Left: Decrement MIDI channel (0..15) */
                    g_channel_offset = (g_channel_offset <= 0) ? 15 : g_channel_offset - 1;
                    smc_midi_set_channel_override(&g_midi_engine, g_channel_offset);
                } else {
                    /* Left: Octave Down (-12 semitones) */
                    g_octave_offset -= 12;
                    if (g_octave_offset < -48) g_octave_offset = -48;
                    smc_midi_set_octave_offset(&g_midi_engine, g_octave_offset);
                }
            } else if (hw_ev.index == SMCPAD_BTN_RIGHT_ARROW) {
                smcpad_led_set(SMCPAD_BTN_RIGHT_ARROW, 1);
                if (g_preset_idx == 1 && !g_midi_engine.shift_active) {
                    /* Preset 2 (DAW mode): Mackie Bank Right Note 47 (0x2F) */
                    smc_midi_raw_msg_t bright = { .bytes = { 0x90, 0x2F, 127 }, .len = 3 };
                    smc_midi_send_raw(&g_midi_engine, &bright);
                } else if (g_midi_engine.shift_active) {
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
            if (hw_ev.index == SMCPAD_BTN_SHIFT) {
                g_midi_engine.shift_active = false;
                smcpad_led_set(SMCPAD_BTN_SHIFT, 0);
                if (g_note_repeat_cfg_mode) {
                    g_note_repeat_cfg_mode = false;
                }
                restore_bank_leds();
            } else if (hw_ev.index == SMCPAD_BTN_NOTE_REPEAT) {
                g_note_repeat_btn_held = false;
            } else if (hw_ev.index == SMCPAD_BTN_PLAY) {
                smcpad_led_set(SMCPAD_BTN_PLAY, 0);
                smc_midi_raw_msg_t off = { .bytes = { 0x80, 0x5E, 0 }, .len = 3 };
                smc_midi_send_raw(&g_midi_engine, &off);
            } else if (hw_ev.index == SMCPAD_BTN_STOP && !g_midi_engine.shift_active) {
                smcpad_led_set(SMCPAD_BTN_STOP, 0);
                smc_midi_raw_msg_t off = { .bytes = { 0x80, 0x5D, 0 }, .len = 3 };
                smc_midi_send_raw(&g_midi_engine, &off);
            } else if (hw_ev.index == SMCPAD_BTN_REC) {
                smcpad_led_set(SMCPAD_BTN_REC, 0);
                smc_midi_raw_msg_t off = { .bytes = { 0x80, 0x5F, 0 }, .len = 3 };
                smc_midi_send_raw(&g_midi_engine, &off);
            } else if (hw_ev.index == SMCPAD_BTN_LEFT_ARROW) {
                smcpad_led_set(SMCPAD_BTN_LEFT_ARROW, 0);
                if (g_preset_idx == 1 && !g_midi_engine.shift_active) {
                    smc_midi_raw_msg_t off = { .bytes = { 0x80, 0x2E, 0 }, .len = 3 };
                    smc_midi_send_raw(&g_midi_engine, &off);
                }
            } else if (hw_ev.index == SMCPAD_BTN_RIGHT_ARROW) {
                smcpad_led_set(SMCPAD_BTN_RIGHT_ARROW, 0);
                if (g_preset_idx == 1 && !g_midi_engine.shift_active) {
                    smc_midi_raw_msg_t off = { .bytes = { 0x80, 0x2F, 0 }, .len = 3 };
                    smc_midi_send_raw(&g_midi_engine, &off);
                }
            }
            break;

        default:
            break;
        }
    }

    /* 4. Update Bluetooth Status LED */
    if (!g_ble_enabled) {
        smcpad_led_set(SMCPAD_BTN_BLUETOOTH, 0);
    } else {
        bool connected = (smcpad_ble_connection() > 0) && (smcpad_ble_subscribed(SMC_BLE_MIDI_HANDLE_VAL) > 0);
        if (connected) {
            smcpad_led_set(SMCPAD_BTN_BLUETOOTH, 1);
        } else {
            smcpad_led_set(SMCPAD_BTN_BLUETOOTH, ((app_get_millis() / 500u) & 1u) != 0);
        }
    }

    /* 5. Battery Voltage Monitoring & Low-Battery Alert */
    uint32_t now_battery = app_get_millis();
    if (now_battery - g_last_battery_check_ms >= 2000u) {
        g_last_battery_check_ms = now_battery;
#ifndef SMC_HOST_TEST
        int mv = smcpad_battery_mv();
        int usb = smcpad_usb_power();
        if (!usb && mv > 1000 && mv < 3500) {
            g_low_battery_alert = true;
        } else if (usb || mv >= 3600) {
            if (g_low_battery_alert) {
                g_low_battery_alert = false;
                smcpad_led_set(SMCPAD_BTN_LEFT_ARROW, 0);
                smcpad_led_set(SMCPAD_BTN_RIGHT_ARROW, 0);
            }
        }
#endif
    }
    if (g_low_battery_alert) {
        bool blink = ((now_battery / 250u) & 1u) != 0;
        smcpad_led_set(SMCPAD_BTN_LEFT_ARROW, blink ? 1 : 0);
        smcpad_led_set(SMCPAD_BTN_RIGHT_ARROW, blink ? 1 : 0);
    }

    smcpad_yield();
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
