/**
 * @file test_e2e_pipeline.c
 * @brief End-to-End Simulation of the SMC-Pad Reactive MIDI Pipeline
 */

#include <stdio.h>
#include <assert.h>
#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"
#include "matrix.h"
#include "pad_adc.h"

/* Captured MIDI packets */
typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
} midi_packet_t;

#define MAX_CAPTURED_MIDI 64
static midi_packet_t captured_midi[MAX_CAPTURED_MIDI];
static uint32_t captured_midi_count = 0;

static void midi_dispatch(const smc_event_t *ev, smc_encoder_state_t *enc) {
    if (captured_midi_count >= MAX_CAPTURED_MIDI) {
        return;
    }

    midi_packet_t pkt = {0, 0, 0};

    switch (ev->type) {
        case EVENT_PAD_STRIKE:
            /* Note On: Channel 1 (0x90), Note = 36 + index, Velocity = ev->value */
            pkt.status = 0x90;
            pkt.data1 = (uint8_t)(36 + ev->index);
            pkt.data2 = (uint8_t)ev->value;
            captured_midi[captured_midi_count++] = pkt;
            break;

        case EVENT_PAD_RELEASE:
            /* Note Off: Channel 1 (0x80), Note = 36 + index, Velocity = 0 */
            pkt.status = 0x80;
            pkt.data1 = (uint8_t)(36 + ev->index);
            pkt.data2 = 0;
            captured_midi[captured_midi_count++] = pkt;
            break;

        case EVENT_ENCODER_DELTA:
            /* Control Change: Channel 1 (0xB0), CC = config cc_number, Val = new val */
            {
                uint8_t bank = smc_encoder_get_bank(enc);
                uint8_t val = smc_encoder_get_value(enc, bank, ev->index);
                pkt.status = 0xB0;
                pkt.data1 = enc->configs[bank][ev->index].cc_number;
                pkt.data2 = val;
                captured_midi[captured_midi_count++] = pkt;
            }
            break;

        case EVENT_BUTTON_DOWN:
            /* Shift + Pad Bank etc. could switch banks */
            if (ev->index == BTN_PAD_BANK) {
                /* Toggle bank */
                uint8_t b = (smc_encoder_get_bank(enc) + 1) % SMC_NUM_BANKS;
                smc_encoder_set_bank(enc, b);
            }
            break;

        case EVENT_BUTTON_UP:
        default:
            break;
    }
}

/* Simulation state hooks */
static uint16_t sim_sr_col = 0;
static uint16_t sim_pad_levels[SMC_NUM_PADS];
static bool sim_btn_levels[SMC_NUM_BUTTONS];
static uint8_t sim_mux_ch = 0;

static void hal_sr_clk(bool l) { (void)l; }
static void hal_sr_dat(bool l) { (void)l; }
static void hal_sr_lat(bool l) { (void)l; }
static void hal_sr_oe(bool l)  { (void)l; }

static bool hal_read_s0(void) { return false; }
static bool hal_read_s1(void) {
    if (sim_sr_col < SMC_NUM_BUTTONS) {
        return sim_btn_levels[sim_sr_col];
    }
    return false;
}

static void hal_set_phase(uint8_t p, bool a) { (void)p; (void)a; }
static void hal_set_seg(uint8_t s, bool a)   { (void)s; (void)a; }

static void hal_set_mux(uint8_t ch) { sim_mux_ch = ch; }
static void hal_delay_us(uint32_t us) { (void)us; }
static uint16_t hal_read_ch3(void) { return sim_pad_levels[sim_mux_ch]; }
static uint16_t hal_read_ch4(void) { return sim_pad_levels[sim_mux_ch + 8]; }

int main(void) {
    printf("Running end-to-end pipeline simulation test...\n");

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

    /* 4. Initialize pad HAL & engine */
    smc_pad_hal_t p_hal = {
        .set_mux_channel = hal_set_mux,
        .settling_delay_us = hal_delay_us,
        .read_adc_ch3 = hal_read_ch3,
        .read_adc_ch4 = hal_read_ch4
    };
    smc_pad_matrix_t pad_engine;
    smc_pad_init(&pad_engine, &p_hal, 50, 35, 1000, 3, VELOCITY_CURVE_LINEAR);

    /* Clear sim arrays */
    for (int i = 0; i < SMC_NUM_PADS; i++) sim_pad_levels[i] = 0;
    for (int i = 0; i < SMC_NUM_BUTTONS; i++) sim_btn_levels[i] = false;

    /* Simulate pad strike on Pad 0: ADC 500 */
    sim_pad_levels[0] = 500;
    smc_pad_scan_step(&pad_engine, &queue, 100);
    smc_pad_scan_step(&pad_engine, &queue, 101);
    smc_pad_scan_step(&pad_engine, &queue, 102);
    smc_pad_scan_step(&pad_engine, &queue, 103); /* Strike window triggers */

    /* Simulate rotary encoder 1 turn (Clockwise) */
    smc_encoder_process_sample(&encoder, &queue, 1, 1, 104);
    smc_encoder_process_sample(&encoder, &queue, 1, 3, 105);

    /* Simulate release of Pad 0 */
    sim_pad_levels[0] = 10;
    smc_pad_scan_step(&pad_engine, &queue, 106);

    /* Consume queue in main loop and dispatch to MIDI */
    smc_event_t ev;
    while (smc_queue_pop(&queue, &ev)) {
        midi_dispatch(&ev, &encoder);
    }

    /* Verify captured MIDI packets */
    assert(captured_midi_count == 3);

    /* 1st packet: Note On for pad 0 (Note 36) */
    assert(captured_midi[0].status == 0x90);
    assert(captured_midi[0].data1 == 36);
    assert(captured_midi[0].data2 > 0);

    /* 2nd packet: CC for encoder 1 (CC 21, Val 1) */
    assert(captured_midi[1].status == 0xB0);
    assert(captured_midi[1].data1 == 21);
    assert(captured_midi[1].data2 == 1);

    /* 3rd packet: Note Off for pad 0 (Note 36) */
    assert(captured_midi[2].status == 0x80);
    assert(captured_midi[2].data1 == 36);
    assert(captured_midi[2].data2 == 0);

    printf("End-to-end pipeline simulation test passed successfully!\n");
    return 0;
}
