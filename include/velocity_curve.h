/**
 * @file velocity_curve.h
 * @brief Precomputed Velocity Curves for SMC-Pad
 */

#ifndef SMC_VELOCITY_CURVE_H
#define SMC_VELOCITY_CURVE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VELOCITY_CURVE_LINEAR = 0,
    VELOCITY_CURVE_SOFT   = 1,   /* Logarithmic: easier to reach higher velocity */
    VELOCITY_CURVE_HARD   = 2,   /* Exponential: requires firmer strike */
    VELOCITY_CURVE_FIXED  = 3    /* Always 127 */
} smc_velocity_curve_type_t;

/**
 * @brief Map a calibrated peak ADC reading to 7-bit MIDI velocity (1..127).
 *
 * @param peak_adc Peak reading captured during strike window
 * @param min_thresh Noise floor / strike threshold
 * @param max_adc Maximum expected ADC reading (e.g. 1023 or 4095)
 * @param curve_type Selected velocity curve
 * @return uint8_t Velocity in range 1..127
 */
uint8_t smc_calculate_velocity(uint16_t peak_adc,
                              uint16_t min_thresh,
                              uint16_t max_adc,
                              smc_velocity_curve_type_t curve_type);

#ifdef __cplusplus
}
#endif

#endif /* SMC_VELOCITY_CURVE_H */
