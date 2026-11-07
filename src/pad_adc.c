/**
 * @file pad_adc.c
 * @brief Dual-Mux ADC Sampling & Peak Detection Pad Engine Implementation
 */

#include "pad_adc.h"

void smc_pad_init(smc_pad_matrix_t *engine,
                 const smc_pad_hal_t *hal,
                 uint16_t threshold_strike,
                 uint16_t threshold_release,
                 uint16_t max_adc,
                 uint32_t strike_window_ticks,
                 smc_velocity_curve_type_t curve) {
    if (!engine) {
        return;
    }
    if (hal) {
        engine->hal = *hal;
    }

    engine->threshold_strike = (threshold_strike > 0) ? threshold_strike : 50;
    engine->threshold_release = (threshold_release > 0) ? threshold_release : 35;
    engine->max_adc = (max_adc > 0) ? max_adc : 1023;
    engine->strike_window_ticks = (strike_window_ticks > 0) ? strike_window_ticks : 3;
    engine->curve = curve;

    for (uint8_t i = 0; i < SMC_NUM_PADS; i++) {
        engine->pads[i].state = PAD_IDLE;
        engine->pads[i].peak_adc = 0;
        engine->pads[i].strike_start_ticks = 0;
    }
}

void smc_pad_set_curve(smc_pad_matrix_t *engine, smc_velocity_curve_type_t curve) {
    if (engine) {
        engine->curve = curve;
    }
}

static void process_single_pad(smc_pad_matrix_t *engine,
                              smc_event_queue_t *queue,
                              uint8_t pad_idx,
                              uint16_t sample,
                              uint32_t current_ticks) {
    smc_pad_channel_state_t *pad = &engine->pads[pad_idx];

    switch (pad->state) {
        case PAD_IDLE:
            if (sample >= engine->threshold_strike) {
                pad->state = PAD_STRIKING;
                pad->peak_adc = sample;
                pad->strike_start_ticks = current_ticks;
            }
            break;

        case PAD_STRIKING:
            if (sample > pad->peak_adc) {
                pad->peak_adc = sample;
            }

            /* Has the peak sampling window elapsed? */
            if ((current_ticks - pad->strike_start_ticks) >= engine->strike_window_ticks) {
                uint8_t velocity = smc_calculate_velocity(pad->peak_adc,
                                                         engine->threshold_strike,
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
            /* Check release with hysteresis */
            if (sample <= engine->threshold_release) {
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
            break;
    }
}

void smc_pad_scan_step(smc_pad_matrix_t *engine,
                      smc_event_queue_t *queue,
                      uint32_t current_timestamp_ticks) {
    if (!engine) {
        return;
    }

    /* 8 multiplexer channels: 0..7 */
    for (uint8_t ch = 0; ch < 8; ch++) {
        /* Set channel selection lines S0, S1, S2 */
        if (engine->hal.set_mux_channel) {
            engine->hal.set_mux_channel(ch);
        }

        /* Wait 5 us settling delay for analog multiplexer capacitance */
        if (engine->hal.settling_delay_us) {
            engine->hal.settling_delay_us(5);
        }

        /* Read ADC Ch 3 (4051 #1: pads 0..7) */
        uint16_t sample_ch3 = 0;
        if (engine->hal.read_adc_ch3) {
            sample_ch3 = engine->hal.read_adc_ch3();
        }

        /* Read ADC Ch 4 (4051 #2: pads 8..15) */
        uint16_t sample_ch4 = 0;
        if (engine->hal.read_adc_ch4) {
            sample_ch4 = engine->hal.read_adc_ch4();
        }

        /* Process pad on Mux 1 (pad = ch) */
        process_single_pad(engine, queue, ch, sample_ch3, current_timestamp_ticks);

        /* Process pad on Mux 2 (pad = ch + 8) */
        process_single_pad(engine, queue, (uint8_t)(ch + 8), sample_ch4, current_timestamp_ticks);
    }
}
