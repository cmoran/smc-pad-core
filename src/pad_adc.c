/**
 * @file pad_adc.c
 * @brief Dual-Mux ADC Sampling with Dynamic Auto-Zero Baseline Implementation
 */

#include "pad_adc.h"

void smc_pad_init(smc_pad_matrix_t *engine,
                 const smc_pad_hal_t *hal,
                 uint16_t delta_strike,
                 uint16_t delta_release,
                 uint16_t max_adc,
                 uint32_t strike_window_ticks,
                 smc_velocity_curve_type_t curve) {
    if (!engine) {
        return;
    }
    if (hal) {
        engine->hal = *hal;
    }

    engine->delta_strike = (delta_strike > 0) ? delta_strike : 50;
    engine->delta_release = (delta_release > 0) ? delta_release : 25;
    engine->max_adc = (max_adc > 0) ? max_adc : 1023;
    engine->strike_window_ticks = (strike_window_ticks > 0) ? strike_window_ticks : 3;
    engine->curve = curve;

    for (uint8_t i = 0; i < SMC_NUM_PADS; i++) {
        engine->pads[i].state = PAD_IDLE;
        engine->pads[i].baseline_adc = 0;
        engine->pads[i].peak_adc = 0;
        engine->pads[i].strike_start_ticks = 0;
    }
}

void smc_pad_set_curve(smc_pad_matrix_t *engine, smc_velocity_curve_type_t curve) {
    if (engine) {
        engine->curve = curve;
    }
}

void smc_pad_calibrate_baseline(smc_pad_matrix_t *engine) {
    if (!engine) {
        return;
    }
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (engine->hal.set_mux_channel) {
            engine->hal.set_mux_channel(ch);
        }
        if (engine->hal.settling_delay_us) {
            engine->hal.settling_delay_us(5);
        }
        if (engine->hal.read_adc_ch3) {
            engine->pads[ch].baseline_adc = engine->hal.read_adc_ch3();
        }
        if (engine->hal.read_adc_ch4) {
            engine->pads[ch + 8].baseline_adc = engine->hal.read_adc_ch4();
        }
    }
}

static void process_single_pad(smc_pad_matrix_t *engine,
                              smc_event_queue_t *queue,
                              uint8_t pad_idx,
                              uint16_t sample,
                              uint32_t current_ticks) {
    smc_pad_channel_state_t *pad = &engine->pads[pad_idx];
    uint16_t strike_threshold = pad->baseline_adc + engine->delta_strike;

    switch (pad->state) {
        case PAD_IDLE:
            if (sample >= strike_threshold) {
                /* Strike detected! Freeze baseline and enter peak sampling */
                pad->state = PAD_STRIKING;
                pad->peak_adc = sample;
                pad->strike_start_ticks = current_ticks;
            } else {
                /*
                 * Dynamic Auto-Zero Baseline Tracking:
                 * Leaky integrator filter (alpha = 1/32) smoothly tracks resting sensor drift/relaxation
                 */
                pad->baseline_adc = (uint16_t)(((uint32_t)pad->baseline_adc * 31u + sample) / 32u);
            }
            break;

        case PAD_STRIKING:
            if (sample > pad->peak_adc) {
                pad->peak_adc = sample;
            }

            /* Peak sampling window evaluation */
            if ((current_ticks - pad->strike_start_ticks) >= engine->strike_window_ticks) {
                uint8_t velocity = smc_calculate_velocity(pad->peak_adc,
                                                         strike_threshold,
                                                         engine->max_adc,
                                                         engine->curve);

                pad->state = PAD_PRESSED;

                if (queue) {
                    smc_event_t ev = {
                        .type = EVENT_PAD_STRIKE,
                        .index = pad_idx,
                        .value = (int16_t)velocity,
                        .timestamp = current_ticks
                    };
                    smc_queue_push(queue, &ev);
                }
            }
            break;

        case PAD_PRESSED:
            {
                /* Relative release threshold with hysteresis */
                uint16_t release_threshold = pad->baseline_adc + engine->delta_release;
                if (sample <= release_threshold) {
                    pad->state = PAD_IDLE;
                    pad->peak_adc = 0;

                    if (queue) {
                        smc_event_t ev = {
                            .type = EVENT_PAD_RELEASE,
                            .index = pad_idx,
                            .value = 0,
                            .timestamp = current_ticks
                        };
                        smc_queue_push(queue, &ev);
                    }
                }
            }
            break;
    }
}

void smc_pad_scan_step(smc_pad_matrix_t *engine,
                      smc_event_queue_t *queue,
                      uint32_t current_timestamp_ticks) {
    if (!engine) {
        return;
    }

    for (uint8_t ch = 0; ch < 8; ch++) {
        if (engine->hal.set_mux_channel) {
            engine->hal.set_mux_channel(ch);
        }

        if (engine->hal.settling_delay_us) {
            engine->hal.settling_delay_us(5);
        }

        uint16_t sample_ch3 = 0;
        if (engine->hal.read_adc_ch3) {
            sample_ch3 = engine->hal.read_adc_ch3();
        }

        uint16_t sample_ch4 = 0;
        if (engine->hal.read_adc_ch4) {
            sample_ch4 = engine->hal.read_adc_ch4();
        }

        process_single_pad(engine, queue, ch, sample_ch3, current_timestamp_ticks);
        process_single_pad(engine, queue, (uint8_t)(ch + 8), sample_ch4, current_timestamp_ticks);
    }
}
