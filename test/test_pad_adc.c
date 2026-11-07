/**
 * @file test_pad_adc.c
 * @brief Unit tests for Dual-Mux ADC Pad Scanning and Velocity Curves
 */

#include <stdio.h>
#include <assert.h>
#include "pad_adc.h"

static uint8_t sim_current_mux_ch = 0;
static uint32_t sim_delay_count = 0;
static uint16_t sim_pad_adc_values[SMC_NUM_PADS];

static void hal_set_mux(uint8_t ch) {
    sim_current_mux_ch = ch;
}

static void hal_delay_us(uint32_t us) {
    sim_delay_count += us;
}

static uint16_t hal_read_ch3(void) {
    return sim_pad_adc_values[sim_current_mux_ch];
}

static uint16_t hal_read_ch4(void) {
    return sim_pad_adc_values[sim_current_mux_ch + 8];
}

int main(void) {
    printf("Running Pad ADC & Velocity Engine unit tests...\n");

    /* 1. Test Velocity Curve Engine */
    uint16_t min_t = 50;
    uint16_t max_a = 1000;

    /* Out of bounds checks */
    assert(smc_calculate_velocity(0, min_t, max_a, VELOCITY_CURVE_LINEAR) == 1);
    assert(smc_calculate_velocity(50, min_t, max_a, VELOCITY_CURVE_LINEAR) == 1);
    assert(smc_calculate_velocity(1000, min_t, max_a, VELOCITY_CURVE_LINEAR) == 127);
    assert(smc_calculate_velocity(1500, min_t, max_a, VELOCITY_CURVE_LINEAR) == 127);
    assert(smc_calculate_velocity(300, min_t, max_a, VELOCITY_CURVE_FIXED) == 127);

    /* Mid-range comparison between curves */
    uint16_t mid_strike = 525; /* 50% between min_t and max_a */
    uint8_t vel_lin  = smc_calculate_velocity(mid_strike, min_t, max_a, VELOCITY_CURVE_LINEAR);
    uint8_t vel_soft = smc_calculate_velocity(mid_strike, min_t, max_a, VELOCITY_CURVE_SOFT);
    uint8_t vel_hard = smc_calculate_velocity(mid_strike, min_t, max_a, VELOCITY_CURVE_HARD);

    /* Soft curve should give higher velocity at mid-range than linear */
    assert(vel_soft > vel_lin);
    /* Hard curve should give lower velocity at mid-range than linear */
    assert(vel_hard < vel_lin);

    /* 2. Test Dual-Mux ADC Pad State Machine */
    smc_pad_hal_t hal = {
        .set_mux_channel = hal_set_mux,
        .settling_delay_us = hal_delay_us,
        .read_adc_ch3 = hal_read_ch3,
        .read_adc_ch4 = hal_read_ch4
    };

    smc_pad_matrix_t engine;
    smc_pad_init(&engine, &hal, 50, 35, 1000, 3, VELOCITY_CURVE_LINEAR);

    smc_event_queue_t queue;
    smc_queue_init(&queue);

    for (int i = 0; i < SMC_NUM_PADS; i++) {
        sim_pad_adc_values[i] = 0;
    }

    /* Scan with idle noise (< 50) -> no event */
    sim_pad_adc_values[2] = 20;  /* Pad 2 on Mux 1 */
    sim_pad_adc_values[10] = 30; /* Pad 10 on Mux 2 */
    smc_pad_scan_step(&engine, &queue, 10);
    assert(smc_queue_is_empty(&queue));
    assert(sim_delay_count == 8 * 5); /* 8 channels * 5 us = 40 us settling total */

    /* Pad 2 strike initiated at t = 100 */
    sim_pad_adc_values[2] = 400;
    smc_pad_scan_step(&engine, &queue, 100);
    assert(smc_queue_is_empty(&queue)); /* Inside 3 ms peak sampling window */

    /* Pad 2 reading increases at t = 101 */
    sim_pad_adc_values[2] = 600;
    smc_pad_scan_step(&engine, &queue, 101);
    assert(smc_queue_is_empty(&queue));

    /* Pad 2 reading settles at t = 102 */
    sim_pad_adc_values[2] = 580;
    smc_pad_scan_step(&engine, &queue, 102);
    assert(smc_queue_is_empty(&queue));

    /* Window expires at t = 103 (103 - 100 >= 3 ms window) -> emit strike */
    smc_pad_scan_step(&engine, &queue, 103);
    assert(!smc_queue_is_empty(&queue));

    smc_event_t ev;
    bool ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_PAD_STRIKE);
    assert(ev.index == 2);
    /* Peak captured was 600 */
    uint8_t expected_vel = smc_calculate_velocity(600, 50, 1000, VELOCITY_CURVE_LINEAR);
    assert(ev.value == expected_vel);

    /* Test Hysteresis: Reading drops to 40 (> release threshold 35) -> should remain PRESSED */
    sim_pad_adc_values[2] = 40;
    smc_pad_scan_step(&engine, &queue, 104);
    assert(smc_queue_is_empty(&queue));

    /* Reading drops to 20 (<= release threshold 35) -> triggers EVENT_PAD_RELEASE */
    sim_pad_adc_values[2] = 20;
    smc_pad_scan_step(&engine, &queue, 105);
    assert(!smc_queue_is_empty(&queue));

    ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_PAD_RELEASE);
    assert(ev.index == 2);
    assert(ev.value == 0);

    /* Test Mux 2 simultaneous operation (Pad 11 on Ch 3 of Mux 2) */
    sim_pad_adc_values[11] = 800;
    smc_pad_scan_step(&engine, &queue, 200);
    smc_pad_scan_step(&engine, &queue, 203);
    assert(!smc_queue_is_empty(&queue));
    ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_PAD_STRIKE);
    assert(ev.index == 11);

    printf("Pad ADC & Velocity Engine unit tests passed successfully!\n");
    return 0;
}
