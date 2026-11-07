/**
 * @file velocity_curve.c
 * @brief Velocity Curve Calculations using Deterministic Fixed-Point / Integer Math
 */

#include "velocity_curve.h"

uint8_t smc_calculate_velocity(uint16_t peak_adc,
                              uint16_t min_thresh,
                              uint16_t max_adc,
                              smc_velocity_curve_type_t curve_type) {
    if (curve_type == VELOCITY_CURVE_FIXED) {
        return 127;
    }

    if (peak_adc <= min_thresh) {
        return 1;
    }
    if (peak_adc >= max_adc) {
        return 127;
    }

    /* Normalize input to range 0..1023 (10-bit fixed range) */
    uint32_t span = (max_adc > min_thresh) ? (max_adc - min_thresh) : 1;
    uint32_t norm = ((uint32_t)(peak_adc - min_thresh) * 1023u) / span;

    uint32_t result = 0;

    switch (curve_type) {
        case VELOCITY_CURVE_LINEAR:
            /* norm: 0..1023 -> 1..127 */
            result = 1 + ((norm * 126u) / 1023u);
            break;

        case VELOCITY_CURVE_SOFT:
            /*
             * Logarithmic-style curve: concave downward.
             * Formula: (2 * norm - (norm^2 / 1023)) maps 0..1023 with rapid initial rise
             */
            {
                uint32_t term = (norm * norm) / 1023u;
                uint32_t shaped = (2u * norm > term) ? (2u * norm - term) : norm;
                result = 1 + ((shaped * 126u) / 1023u);
            }
            break;

        case VELOCITY_CURVE_HARD:
            /*
             * Exponential-style curve: concave upward.
             * Formula: (norm^2 / 1023) maps 0..1023 with slow initial rise requiring hard strikes
             */
            {
                uint32_t shaped = (norm * norm) / 1023u;
                result = 1 + ((shaped * 126u) / 1023u);
            }
            break;

        case VELOCITY_CURVE_FIXED:
        default:
            return 127;
    }

    if (result < 1) {
        result = 1;
    } else if (result > 127) {
        result = 127;
    }

    return (uint8_t)result;
}
