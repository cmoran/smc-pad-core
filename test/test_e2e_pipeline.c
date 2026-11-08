/**
 * @file test_e2e_pipeline.c
 * @brief End-to-End Simulation of the SMC-Pad Reactive Pipeline:
 * Scanning, Virtual Banks, Flash Config, SysEx 0x7D, BLE-MIDI & Host LED Feedback
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
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

#define MAX_CAPTURED_MIDI 64
static smc_midi_raw_msg_t captured_usb_midi[MAX_CAPTURED_MIDI];
static uint32_t           captured_usb_count = 0;

static smc_midi_raw_msg_t captured_ble_midi[MAX_CAPTURED_MIDI];
static uint32_t           captured_ble_count = 0;

static uint8_t            captured_sysex_tx[64];
static size_t             captured_sysex_len = 0;

static void sim_send_usb_midi(const smc_midi_raw_msg_t *msg) {
    if (captured_usb_count < MAX_CAPTURED_MIDI) {
        captured_usb_midi[captured_usb_count++] = *msg;
    }
}

static void sim_send_ble_midi(const smc_midi_raw_msg_t *msg) {
    if (captured_ble_count < MAX_CAPTURED_MIDI) {
        captured_ble_midi[captured_ble_count++] = *msg;
    }
}

static void sim_send_sysex_tx(const uint8_t *data, size_t len) {
    if (len <= sizeof(captured_sysex_tx)) {
        memcpy(captured_sysex_tx, data, len);
        captured_sysex_len = len;
    }
}

/* Simulation state hooks */
static uint16_t sim_pad_levels[SMC_NUM_PADS];
static uint8_t  sim_mux_ch = 0;

static void hal_sr_clk(bool l) { (void)l; }
static void hal_sr_dat(bool l) { (void)l; }
static void hal_sr_lat(bool l) { (void)l; }
static void hal_sr_oe(bool l)  { (void)l; }
static bool hal_read_s0(void) { return false; }
static bool hal_read_s1(void) { return false; }
static void hal_set_phase(uint8_t p, bool a) { (void)p; (void)a; }
static void hal_set_seg(uint8_t s, bool a)   { (void)s; (void)a; }

static void hal_set_mux(uint8_t ch) { sim_mux_ch = ch; }
static void hal_delay_us(uint32_t us) { (void)us; }
static uint16_t hal_read_ch3(void) { return sim_pad_levels[sim_mux_ch]; }
static uint16_t hal_read_ch4(void) { return sim_pad_levels[sim_mux_ch + 8]; }

static smc_sysex_engine_t g_sim_sysex;
static smc_config_t       g_sim_cfg;

static void host_note_callback(uint8_t ch, uint8_t note, uint8_t vel, void *ctx) {
    (void)ch;
    (void)ctx;
    uint8_t bank = smc_config_get_pad_bank();
    for (uint8_t p = 0; p < SMC_NUM_PADS; p++) {
        if (g_sim_cfg.pads[bank][p].note == note) {
            if (vel > 0) {
                smc_rgb_led_set_pad(p, g_sim_cfg.pad_hit_color);
            } else {
                smc_rgb_led_set_pad(p, g_sim_cfg.pad_idle_color);
            }
        }
    }
}

static void host_sysex_callback(uint8_t byte, void *ctx) {
    (void)ctx;
    smc_sysex_feed_byte(&g_sim_sysex, byte);
}

int main(void) {
    printf("Running integrated end-to-end pipeline simulation test...\n");

    /* 1. Initialize queue */
    smc_event_queue_t queue;
    smc_queue_init(&queue);

    /* 2. Initialize encoder engine */
    smc_encoder_state_t encoder;
    smc_encoder_init(&encoder, 2);

    /* 3. Initialize matrix HAL & engine */
    smc_matrix_hal_t m_hal = {
        .sr_set_clk = hal_sr_clk,
        .sr_set_dat = hal_sr_dat,
        .sr_set_lat = hal_sr_lat,
        .sr_set_oe  = hal_sr_oe,
        .read_sense0 = hal_read_s0,
        .read_sense1 = hal_read_s1,
        .set_led_phase = hal_set_phase,
        .set_led_segment = hal_set_seg
    };
    smc_matrix_state_t matrix;
    smc_matrix_init(&matrix, &m_hal);

    /* 4. Initialize Configuration Engine */
    smc_config_init(&g_sim_cfg, NULL, NULL);

    /* 5. Initialize pad HAL & engine with auto-zero baseline */
    smc_pad_hal_t p_hal = {
        .set_mux_channel = hal_set_mux,
        .settling_delay_us = hal_delay_us,
        .read_adc_ch3 = hal_read_ch3,
        .read_adc_ch4 = hal_read_ch4
    };
    smc_pad_matrix_t pad_engine;
    smc_pad_init(&pad_engine, &p_hal,
                 g_sim_cfg.strike_threshold,
                 g_sim_cfg.release_threshold,
                 g_sim_cfg.dynamic_range,
                 g_sim_cfg.scan_window_ms,
                 (smc_velocity_curve_type_t)g_sim_cfg.velocity_curve);

    /* 6. Initialize SysEx Engine */
    smc_sysex_init(&g_sim_sysex, &g_sim_cfg, sim_send_sysex_tx);

    /* 7. Initialize MIDI Dispatcher with Dual Sinks & Host RX */
    smc_midi_dispatcher_t dispatcher;
    smc_midi_init(&dispatcher, sim_send_usb_midi);
    smc_midi_set_ble_sink(&dispatcher, sim_send_ble_midi);
    smc_midi_set_config(&dispatcher, &g_sim_cfg);
    smc_midi_set_host_callbacks(&dispatcher, host_note_callback, host_sysex_callback, NULL);

    /* 8. Initialize RGB LEDs */
    smc_rgb_led_init();
    smc_rgb_led_set_all(g_sim_cfg.pad_idle_color);

    /* Clear sim arrays */
    for (int i = 0; i < SMC_NUM_PADS; i++) sim_pad_levels[i] = 0;

    /* --- Stage 1: Pad 0 Strike & Encoder Turn in Bank 0 --- */
    smc_config_set_pad_bank(0);
    smc_config_set_encoder_bank(0);

    /* Simulate pad strike on Pad 0: ADC 500 */
    sim_pad_levels[0] = 500;
    smc_pad_scan_step(&pad_engine, &queue, 100);
    smc_pad_scan_step(&pad_engine, &queue, 101);
    smc_pad_scan_step(&pad_engine, &queue, 102);
    smc_pad_scan_step(&pad_engine, &queue, 103); /* Strike window triggers */

    /* Consume event queue */
    smc_event_t ev;
    while (smc_queue_pop(&queue, &ev)) {
        if (ev.type == EVENT_PAD_STRIKE) {
            smc_rgb_led_set_pad(ev.index, g_sim_cfg.pad_hit_color);
        } else if (ev.type == EVENT_PAD_RELEASE) {
            smc_rgb_led_set_pad(ev.index, g_sim_cfg.pad_idle_color);
        }
        smc_midi_process_event(&dispatcher, &encoder, &ev);
    }
    assert(smc_rgb_led_get_pad(0) == g_sim_cfg.pad_hit_color);

    /* Simulate rotary encoder 1 turn (Clockwise) */
    smc_encoder_process_sample(&encoder, &queue, 1, 1, 104);
    smc_encoder_process_sample(&encoder, &queue, 1, 3, 105);

    /* Simulate release of Pad 0 */
    sim_pad_levels[0] = 10;
    smc_pad_scan_step(&pad_engine, &queue, 106);

    while (smc_queue_pop(&queue, &ev)) {
        if (ev.type == EVENT_PAD_STRIKE) {
            smc_rgb_led_set_pad(ev.index, g_sim_cfg.pad_hit_color);
        } else if (ev.type == EVENT_PAD_RELEASE) {
            smc_rgb_led_set_pad(ev.index, g_sim_cfg.pad_idle_color);
        }
        smc_midi_process_event(&dispatcher, &encoder, &ev);
    }
    assert(smc_rgb_led_get_pad(0) == g_sim_cfg.pad_idle_color);

    /* Verify Dual Transport Outputs */
    assert(captured_usb_count == 3);
    assert(captured_ble_count == 3);

    /* 1st packet: Note-On Bank 0 Pad 0 (Note 36) */
    assert(captured_usb_midi[0].bytes[0] == 0x90);
    assert(captured_usb_midi[0].bytes[1] == 36);
    assert(captured_ble_midi[0].bytes[1] == 36);

    /* 2nd packet: CC Bank 0 Encoder 1 (CC 17) */
    assert(captured_usb_midi[1].bytes[0] == 0xB0);
    assert(captured_usb_midi[1].bytes[1] == 17);
    assert(captured_ble_midi[1].bytes[1] == 17);

    /* 3rd packet: Note-Off Bank 0 Pad 0 */
    assert(captured_usb_midi[2].bytes[0] == 0x80);
    assert(captured_usb_midi[2].bytes[1] == 36);
    assert(captured_ble_midi[2].bytes[1] == 36);

    /* --- Stage 2: Bank Switching to Bank 2 --- */
    smc_config_set_pad_bank(2);
    captured_usb_count = 0;
    captured_ble_count = 0;

    /* Strike Pad 0 in Bank 2 -> Note 68 (36 + 32) */
    sim_pad_levels[0] = 600;
    smc_pad_scan_step(&pad_engine, &queue, 110);
    smc_pad_scan_step(&pad_engine, &queue, 111);
    smc_pad_scan_step(&pad_engine, &queue, 112);
    smc_pad_scan_step(&pad_engine, &queue, 113);

    while (smc_queue_pop(&queue, &ev)) {
        smc_midi_process_event(&dispatcher, &encoder, &ev);
    }
    assert(captured_usb_count == 1);
    assert(captured_usb_midi[0].bytes[1] == 68);
    assert(captured_ble_midi[0].bytes[1] == 68);

    /* --- Stage 3: SysEx Reconfiguration --- */
    /* Reconfigure Bank 2 Pad 0 to Note 108 via SysEx */
    const uint8_t sysex_cmd[] = {
        SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID,
        SMC_CMD_SET_PAD_MAP, 2, 0, 108, 0, 0, SMC_SYSEX_END
    };
    smc_midi_rx_feed_bytes(&dispatcher, sysex_cmd, sizeof(sysex_cmd));

    /* Verify SysEx response ACK */
    assert(captured_sysex_len == 7);
    assert(captured_sysex_tx[3] == SMC_CMD_RESP_ACK);
    assert(captured_sysex_tx[5] == SMC_STATUS_OK);
    assert(g_sim_cfg.pads[2][0].note == 108);

    /* Release and re-strike Pad 0 -> Note 108 */
    sim_pad_levels[0] = 10;
    smc_pad_scan_step(&pad_engine, &queue, 120);
    while (smc_queue_pop(&queue, &ev)) { smc_midi_process_event(&dispatcher, &encoder, &ev); }

    captured_usb_count = 0;
    sim_pad_levels[0] = 650;
    smc_pad_scan_step(&pad_engine, &queue, 125);
    smc_pad_scan_step(&pad_engine, &queue, 126);
    smc_pad_scan_step(&pad_engine, &queue, 127);
    smc_pad_scan_step(&pad_engine, &queue, 128);
    while (smc_queue_pop(&queue, &ev)) { smc_midi_process_event(&dispatcher, &encoder, &ev); }

    assert(captured_usb_midi[0].bytes[1] == 108);

    /* --- Stage 4: Host DAW RGB LED Feedback --- */
    /* Incoming MIDI Note-On for Note 108 with velocity 100 turns Pad 0 LED hit color */
    uint8_t daw_note_on[] = { 0x90, 108, 100 };
    smc_midi_rx_feed_bytes(&dispatcher, daw_note_on, sizeof(daw_note_on));
    assert(smc_rgb_led_get_pad(0) == g_sim_cfg.pad_hit_color);

    /* Incoming MIDI Note-Off restores idle color */
    uint8_t daw_note_off[] = { 0x80, 108, 0 };
    smc_midi_rx_feed_bytes(&dispatcher, daw_note_off, sizeof(daw_note_off));
    assert(smc_rgb_led_get_pad(0) == g_sim_cfg.pad_idle_color);

    printf("Integrated end-to-end pipeline simulation test passed successfully!\n");
    return 0;
}
