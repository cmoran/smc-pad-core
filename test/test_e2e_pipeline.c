/**
 * @file test_e2e_pipeline.c
 * @brief End-to-End Simulation of the SMC-Pad Reactive MIDI Pipeline with Dispatcher
 */

#include <stdio.h>
#include <assert.h>
#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"
#include "matrix.h"
#include "pad_adc.h"
#include "midi_dispatcher.h"

#define MAX_CAPTURED_MIDI 64
static smc_midi_raw_msg_t captured_midi[MAX_CAPTURED_MIDI];
static uint32_t captured_midi_count = 0;

static void sim_send_midi(const smc_midi_raw_msg_t *msg) {
    if (captured_midi_count < MAX_CAPTURED_MIDI) {
        captured_midi[captured_midi_count++] = *msg;
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

    /* 4. Initialize pad HAL & engine with auto-zero baseline */
    smc_pad_hal_t p_hal = {
        .set_mux_channel = hal_set_mux,
        .settling_delay_us = hal_delay_us,
        .read_adc_ch3 = hal_read_ch3,
        .read_adc_ch4 = hal_read_ch4
    };
    smc_pad_matrix_t pad_engine;
    smc_pad_init(&pad_engine, &p_hal, 50, 25, 1000, 3, VELOCITY_CURVE_LINEAR);

    /* 5. Initialize MIDI Dispatcher */
    smc_midi_dispatcher_t dispatcher;
    smc_midi_init(&dispatcher, sim_send_midi);

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
        smc_midi_process_event(&dispatcher, &encoder, &ev);
    }

    /* Verify captured MIDI packets */
    assert(captured_midi_count == 3);

    /* 1st packet: Note On for pad 0 (Note 36) */
    assert(captured_midi[0].bytes[0] == 0x90);
    assert(captured_midi[0].bytes[1] == 36);
    assert(captured_midi[0].bytes[2] > 0);

    /* 2nd packet: CC for encoder 1 (CC 21, Val 1) */
    assert(captured_midi[1].bytes[0] == 0xB0);
    assert(captured_midi[1].bytes[1] == 21);
    assert(captured_midi[1].bytes[2] == 1);

    /* 3rd packet: Note Off for pad 0 (Note 36) */
    assert(captured_midi[2].bytes[0] == 0x80);
    assert(captured_midi[2].bytes[1] == 36);
    assert(captured_midi[2].bytes[2] == 0);

    /* Verify no lingering active notes */
    assert(!smc_midi_is_pad_active(&dispatcher, 0));
    assert(dispatcher.active_pads_mask == 0);

    printf("Integrated end-to-end pipeline simulation test passed successfully!\n");
    return 0;
}
