/**
 * @file test_pad_adc.c
 * @brief Unit tests for Dual-Mux ADC Pad Scanning, Auto-Zero Baseline, and Velocity Curves
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

    assert(smc_calculate_velocity(0, min_t, max_a, VELOCITY_CURVE_LINEAR) == 1);
    assert(smc_calculate_velocity(50, min_t, max_a, VELOCITY_CURVE_LINEAR) == 1);
    assert(smc_calculate_velocity(1000, min_t, max_a, VELOCITY_CURVE_LINEAR) == 127);
    assert(smc_calculate_velocity(1500, min_t, max_a, VELOCITY_CURVE_LINEAR) == 127);
    assert(smc_calculate_velocity(300, min_t, max_a, VELOCITY_CURVE_FIXED) == 127);

    uint16_t mid_strike = 525;
    uint8_t vel_lin  = smc_calculate_velocity(mid_strike, min_t, max_a, VELOCITY_CURVE_LINEAR);
    uint8_t vel_soft = smc_calculate_velocity(mid_strike, min_t, max_a, VELOCITY_CURVE_SOFT);
    uint8_t vel_hard = smc_calculate_velocity(mid_strike, min_t, max_a, VELOCITY_CURVE_HARD);

    assert(vel_soft > vel_lin);
    assert(vel_hard < vel_lin);

    /* 2. Test Dual-Mux ADC Pad State Machine with Dynamic Baseline */
    smc_pad_hal_t hal = {
        .set_mux_channel = hal_set_mux,
        .settling_delay_us = hal_delay_us,
        .read_adc_ch3 = hal_read_ch3,
        .read_adc_ch4 = hal_read_ch4
    };

    smc_pad_matrix_t engine;
    smc_pad_init(&engine, &hal, 50, 25, 1000, 3, VELOCITY_CURVE_LINEAR);

    smc_event_queue_t queue;
    smc_queue_init(&queue);

    for (int i = 0; i < SMC_NUM_PADS; i++) {
        sim_pad_adc_values[i] = 0;
    }

    /* Calibrate initial baseline (all 0) */
    smc_pad_calibrate_baseline(&engine);
    for (int i = 0; i < SMC_NUM_PADS; i++) {
        assert(engine.pads[i].baseline_adc == 0);
    }

    /* Scan with idle noise (< 50) */
    sim_pad_adc_values[2] = 20;
    sim_pad_adc_values[10] = 30;
    smc_pad_scan_step(&engine, &queue, 10);
    assert(smc_queue_is_empty(&queue));

    /* Pad 2 strike initiated at t = 100 */
    sim_pad_adc_values[2] = 400;
    smc_pad_scan_step(&engine, &queue, 100);
    assert(smc_queue_is_empty(&queue));

    sim_pad_adc_values[2] = 600;
    smc_pad_scan_step(&engine, &queue, 101);
    assert(smc_queue_is_empty(&queue));

    sim_pad_adc_values[2] = 580;
    smc_pad_scan_step(&engine, &queue, 102);
    assert(smc_queue_is_empty(&queue));

    /* Window expires at t = 103 */
    smc_pad_scan_step(&engine, &queue, 103);
    assert(!smc_queue_is_empty(&queue));

    smc_event_t ev;
    bool ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_PAD_STRIKE);
    assert(ev.index == 2);
    assert(ev.value > 0);

    /* Hysteresis: Reading drops to 30 (> baseline 0 + delta_release 25) -> remain PRESSED */
    sim_pad_adc_values[2] = 30;
    smc_pad_scan_step(&engine, &queue, 104);
    assert(smc_queue_is_empty(&queue));

    /* Reading drops to 20 (<= baseline 0 + delta_release 25) -> triggers EVENT_PAD_RELEASE */
    sim_pad_adc_values[2] = 20;
    smc_pad_scan_step(&engine, &queue, 105);
    assert(!smc_queue_is_empty(&queue));

    ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_PAD_RELEASE);
    assert(ev.index == 2);
    assert(ev.value == 0);

    /* 3. CRITICAL ANTI-STUCK TEST: Mechanical Relaxation Sensor Drift
     * Suppose Pad 5 has a resting mechanical pre-load of 80 ADC counts on startup.
     */
    sim_pad_adc_values[5] = 80;
    smc_pad_calibrate_baseline(&engine);
    assert(engine.pads[5].baseline_adc == 80);

    /* Idle auto-zero runs with slight fluctuation around 80 */
    for (int p = 0; p < 50; p++) {
        smc_pad_scan_step(&engine, &queue, 1000 + p);
    }
    assert(smc_queue_is_empty(&queue));
    assert(engine.pads[5].baseline_adc >= 78 && engine.pads[5].baseline_adc <= 80);
    uint16_t adapted_baseline = engine.pads[5].baseline_adc;

    /* User strikes Pad 5 hard -> ADC goes to 700 */
    sim_pad_adc_values[5] = 700;
    smc_pad_scan_step(&engine, &queue, 2000);
    smc_pad_scan_step(&engine, &queue, 2001);
    smc_pad_scan_step(&engine, &queue, 2002);
    smc_pad_scan_step(&engine, &queue, 2003); /* Window expires */

    assert(!smc_queue_is_empty(&queue));
    ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_PAD_STRIKE);
    assert(ev.index == 5);

    /* Baseline was strictly frozen during strike */
    assert(engine.pads[5].baseline_adc == adapted_baseline);

    /* Sensor relaxation drift: User lets go completely, but sensor rests at 95 (elevated drift!)
     * In stock firmware with static cutoff < 50, this would hang as a stuck note forever!
     * In our relative architecture: release point is baseline (80) + delta_release (25) = 105.
     * 95 <= 105 -> Note-Off MUST fire cleanly!
     */
    sim_pad_adc_values[5] = 95;
    smc_pad_scan_step(&engine, &queue, 2004);
    assert(!smc_queue_is_empty(&queue));

    ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_PAD_RELEASE);
    assert(ev.index == 5);
    assert(ev.value == 0);

    printf("Pad ADC & Velocity Engine unit tests (including Anti-Stuck drift verification) passed!\n");
    return 0;
}
