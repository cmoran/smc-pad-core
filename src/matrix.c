/**
 * @file matrix.c
 * @brief Matrix Scanner & Non-Interfering Button LED Multiplexer Implementation
 */

#include "matrix.h"

/* Physical button LED hardware map: flash 0x1E1659A */
static const smc_led_pinmap_t LED_MAP[LED_COUNT] = {
    [LED_LEFT_ARROW]  = { .phase = 1, .segment = 0 },
    [LED_RIGHT_ARROW] = { .phase = 0, .segment = 3 },
    [LED_PLAY]        = { .phase = 1, .segment = 2 },
    [LED_PAUSE]       = { .phase = 0, .segment = 1 },
    [LED_REC]         = { .phase = 1, .segment = 1 },
    [LED_BLUETOOTH]   = { .phase = 2, .segment = 2 },
    [LED_PAD_BANK]    = { .phase = 2, .segment = 1 },
    [LED_KNOB_BANK]   = { .phase = 2, .segment = 0 },
    [LED_SHIFT]       = { .phase = 0, .segment = 2 },
    [LED_NOTE_REPEAT] = { .phase = 0, .segment = 0 }
};

void smc_matrix_init(smc_matrix_state_t *matrix, const smc_matrix_hal_t *hal) {
    if (!matrix) {
        return;
    }
    if (hal) {
        matrix->hal = *hal;
    }

    for (uint8_t i = 0; i < SMC_NUM_BUTTONS; i++) {
        matrix->button_raw[i] = 0;
        matrix->button_integrator[i] = 0;
        matrix->button_state[i] = false;
    }

    for (uint8_t i = 0; i < SMC_NUM_ENCODERS; i++) {
        matrix->encoder_phase_bits[i] = 0;
    }

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        matrix->led_states[i] = false;
    }

    matrix->current_led_phase = 0;

    /* Initialize hardware to safe idle state: all LED phases and segments inactive */
    if (matrix->hal.set_led_phase) {
        for (uint8_t p = 0; p < 3; p++) {
            matrix->hal.set_led_phase(p, false);
        }
    }
    if (matrix->hal.set_led_segment) {
        for (uint8_t s = 0; s < 4; s++) {
            matrix->hal.set_led_segment(s, false);
        }
    }
    if (matrix->hal.sr_set_oe) {
        matrix->hal.sr_set_oe(true); /* Enable shift register outputs */
    }
}

void smc_matrix_set_led(smc_matrix_state_t *matrix, smc_led_id_t led_id, bool on) {
    if (!matrix || led_id >= LED_COUNT) {
        return;
    }
    matrix->led_states[led_id] = on;
}

bool smc_matrix_get_led(const smc_matrix_state_t *matrix, smc_led_id_t led_id) {
    if (!matrix || led_id >= LED_COUNT) {
        return false;
    }
    return matrix->led_states[led_id];
}

/*
 * Push 16 bits through the cascaded 74HC595 shift registers.
 * A bit value of 1 selects/activates that column.
 */
static void sr_write_16(const smc_matrix_hal_t *hal, uint16_t value) {
    if (!hal->sr_set_clk || !hal->sr_set_dat || !hal->sr_set_lat) {
        return;
    }

    /* Shift MSB first (bit 15 down to 0) */
    for (int8_t i = 15; i >= 0; i--) {
        bool bit = (value >> i) & 1;
        hal->sr_set_dat(bit);
        hal->sr_set_clk(true);
        hal->sr_set_clk(false);
    }

    /* Latch pulse */
    hal->sr_set_lat(true);
    hal->sr_set_lat(false);
}

void smc_matrix_scan_step(smc_matrix_state_t *matrix,
                         smc_encoder_state_t *encoder_state,
                         smc_event_queue_t *queue,
                         uint32_t timestamp) {
    if (!matrix) {
        return;
    }

    /* CRITICAL: Turn OFF all LED phases before scanning to eliminate PC3/PC1 crosstalk */
    if (matrix->hal.set_led_phase) {
        matrix->hal.set_led_phase(0, false);
        matrix->hal.set_led_phase(1, false);
        matrix->hal.set_led_phase(2, false);
    }
    if (matrix->hal.set_led_segment) {
        matrix->hal.set_led_segment(0, false);
        matrix->hal.set_led_segment(1, false);
        matrix->hal.set_led_segment(2, false);
        matrix->hal.set_led_segment(3, false);
    }

    /* Clock through each of the 16 columns */
    for (uint8_t col = 0; col < SMC_MATRIX_NUM_COLS; col++) {
        uint16_t mask = (uint16_t)(1u << col);
        sr_write_16(&matrix->hal, mask);

        /* Read Sense 0 (Encoders on PB0) */
        if (matrix->hal.read_sense0) {
            bool s0 = matrix->hal.read_sense0();
            uint8_t enc_idx = col / 2;
            uint8_t phase_bit = col % 2; /* 0 = A, 1 = B */

            if (phase_bit == 0) {
                matrix->encoder_phase_bits[enc_idx] = (s0 ? 1 : 0);
            } else {
                uint8_t gray = (uint8_t)(matrix->encoder_phase_bits[enc_idx] | ((s0 ? 1 : 0) << 1));
                if (encoder_state) {
                    smc_encoder_process_sample(encoder_state, queue, enc_idx, gray, timestamp);
                }
            }
        }

        /* Read Sense 1 (Buttons on PC1, columns 0..9) */
        if (col < SMC_NUM_BUTTONS && matrix->hal.read_sense1) {
            bool s1 = matrix->hal.read_sense1();
            matrix->button_raw[col] = s1 ? 1 : 0;

            /* Debounce integrator */
            if (matrix->button_raw[col]) {
                if (matrix->button_integrator[col] < SMC_DEBOUNCE_THRESHOLD) {
                    matrix->button_integrator[col]++;
                    if (matrix->button_integrator[col] == SMC_DEBOUNCE_THRESHOLD && !matrix->button_state[col]) {
                        matrix->button_state[col] = true;
                        if (queue) {
                            smc_event_t ev = {
                                .type = EVENT_BUTTON_DOWN,
                                .index = col,
                                .value = 1,
                                .timestamp = timestamp
                            };
                            smc_queue_push(queue, &ev);
                        }
                    }
                }
            } else {
                if (matrix->button_integrator[col] > 0) {
                    matrix->button_integrator[col]--;
                    if (matrix->button_integrator[col] == 0 && matrix->button_state[col]) {
                        matrix->button_state[col] = false;
                        if (queue) {
                            smc_event_t ev = {
                                .type = EVENT_BUTTON_UP,
                                .index = col,
                                .value = 0,
                                .timestamp = timestamp
                            };
                            smc_queue_push(queue, &ev);
                        }
                    }
                }
            }
        }
    }
}

void smc_matrix_led_step(smc_matrix_state_t *matrix) {
    if (!matrix || !matrix->hal.set_led_phase || !matrix->hal.set_led_segment) {
        return;
    }

    /* Turn off all phases first */
    for (uint8_t p = 0; p < 3; p++) {
        matrix->hal.set_led_phase(p, false);
    }
    for (uint8_t s = 0; s < 4; s++) {
        matrix->hal.set_led_segment(s, false);
    }

    uint8_t phase = matrix->current_led_phase;

    /* Compute segment mask for this phase */
    bool seg_active[4] = { false, false, false, false };
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        if (matrix->led_states[i] && LED_MAP[i].phase == phase) {
            seg_active[LED_MAP[i].segment] = true;
        }
    }

    /* Apply segment sinks */
    for (uint8_t s = 0; s < 4; s++) {
        if (seg_active[s]) {
            matrix->hal.set_led_segment(s, true);
        }
    }

    /* Enable high-side phase driver */
    matrix->hal.set_led_phase(phase, true);

    /* Advance to next phase for next call */
    matrix->current_led_phase = (phase + 1) % 3;
}
