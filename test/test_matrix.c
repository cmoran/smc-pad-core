/**
 * @file test_matrix.c
 * @brief Unit tests for Matrix scanning, debouncing, and LED multiplexing
 */

#include <stdio.h>
#include <assert.h>
#include "matrix.h"

static uint16_t sim_sr_shift_reg = 0;
static uint16_t sim_sr_latched = 0;
static bool sim_sr_oe = false;

static bool sim_s0_level = false;
static bool sim_btn0_pressed = false;

static bool sim_phase_active[3] = { false, false, false };
static bool sim_seg_active[4] = { false, false, false, false };
static bool sim_phase_active_during_scan = false;

static void hal_sr_clk(bool level) {
    (void)level;
}
static void hal_sr_dat(bool level) {
    sim_sr_shift_reg = (sim_sr_shift_reg << 1) | (level ? 1 : 0);
}
static void hal_sr_lat(bool level) {
    if (level) {
        sim_sr_latched = sim_sr_shift_reg;
    }
}
static void hal_sr_oe(bool level) {
    sim_sr_oe = level;
}

static bool hal_read_s0(void) {
    return sim_s0_level;
}
static bool hal_read_s1(void) {
    /* If any LED phase is active during sense line read, flag crosstalk error */
    if (sim_phase_active[0] || sim_phase_active[1] || sim_phase_active[2]) {
        sim_phase_active_during_scan = true;
    }
    /* Only return pressed if button 0 (Shift) column is active */
    if (sim_btn0_pressed && (sim_sr_latched == (1u << BTN_SHIFT))) {
        return true;
    }
    return false;
}

static void hal_set_phase(uint8_t phase, bool active) {
    if (phase < 3) {
        sim_phase_active[phase] = active;
    }
}
static void hal_set_seg(uint8_t seg, bool active) {
    if (seg < 4) {
        sim_seg_active[seg] = active;
    }
}

int main(void) {
    printf("Running matrix & LED multiplexing unit tests...\n");

    smc_matrix_hal_t hal = {
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
    smc_matrix_init(&matrix, &hal);

    smc_event_queue_t queue;
    smc_queue_init(&queue);

    smc_encoder_state_t encoder;
    smc_encoder_init(&encoder, 2);

    /* 1. Verify safe idle state: all LED phases and segments must be inactive */
    assert(!sim_phase_active[0] && !sim_phase_active[1] && !sim_phase_active[2]);
    assert(!sim_seg_active[0] && !sim_seg_active[1] && !sim_seg_active[2] && !sim_seg_active[3]);

    /* 2. Verify debouncing: simulate button 0 (Shift) held for fewer than threshold cycles */
    sim_btn0_pressed = true;
    for (int i = 0; i < SMC_DEBOUNCE_THRESHOLD - 1; i++) {
        smc_matrix_scan_step(&matrix, &encoder, &queue, 100 + i);
        assert(smc_queue_is_empty(&queue)); /* Should NOT have triggered yet */
    }

    /* Next scan reaches threshold -> triggers EVENT_BUTTON_DOWN */
    smc_matrix_scan_step(&matrix, &encoder, &queue, 200);
    assert(!smc_queue_is_empty(&queue));
    smc_event_t ev;
    bool ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_BUTTON_DOWN);
    assert(ev.index == BTN_SHIFT);
    assert(smc_queue_is_empty(&queue));

    /* Verify no crosstalk: phases were strictly inactive during reads */
    assert(!sim_phase_active_during_scan);

    /* 3. Release button 0: requires threshold cycles to emit EVENT_BUTTON_UP */
    sim_btn0_pressed = false;
    for (int i = 0; i < SMC_DEBOUNCE_THRESHOLD - 1; i++) {
        smc_matrix_scan_step(&matrix, &encoder, &queue, 300 + i);
        assert(smc_queue_is_empty(&queue));
    }
    smc_matrix_scan_step(&matrix, &encoder, &queue, 400);
    assert(!smc_queue_is_empty(&queue));
    ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_BUTTON_UP);
    assert(ev.index == BTN_SHIFT);

    /* 4. Test Button LED Multiplexing */
    /* Turn on Play LED (Phase 1, Segment 2) and Bluetooth LED (Phase 2, Segment 2) */
    smc_matrix_set_led(&matrix, LED_PLAY, true);
    smc_matrix_set_led(&matrix, LED_BLUETOOTH, true);

    /* Step 0: Phase 0 (should have no active segments for these LEDs) */
    smc_matrix_led_step(&matrix);
    assert(sim_phase_active[0] && !sim_phase_active[1] && !sim_phase_active[2]);
    assert(!sim_seg_active[2]);

    /* Step 1: Phase 1 (Play LED on Segment 2 should be active) */
    smc_matrix_led_step(&matrix);
    assert(!sim_phase_active[0] && sim_phase_active[1] && !sim_phase_active[2]);
    assert(sim_seg_active[2]);

    /* Step 2: Phase 2 (Bluetooth LED on Segment 2 should be active) */
    smc_matrix_led_step(&matrix);
    assert(!sim_phase_active[0] && !sim_phase_active[1] && sim_phase_active[2]);
    assert(sim_seg_active[2]);

    printf("Matrix & LED multiplexing unit tests passed successfully!\n");
    return 0;
}
