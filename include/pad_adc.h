/**
 * @file pad_adc.h
 * @brief Dual-Mux ADC Sampling & Peak Detection Pad Engine with Dynamic Auto-Zero Baseline
 */

#ifndef SMC_PAD_ADC_H
#define SMC_PAD_ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "smc_hw.h"
#include "ring_buffer.h"
#include "velocity_curve.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PAD_IDLE = 0,
    PAD_STRIKING,
    PAD_PRESSED
} smc_pad_fsm_state_t;

typedef struct {
    smc_pad_fsm_state_t state;
    uint16_t baseline_adc;          /* Adaptive resting baseline */
    uint16_t peak_adc;              /* Peak pressure captured during window */
    uint32_t strike_start_ticks;
} smc_pad_channel_state_t;

typedef struct {
    /* HAL callbacks */
    void (*set_mux_channel)(uint8_t ch);    /* Shared S0 (PB2), S1 (PB3), S2 (PC4) */
    void (*settling_delay_us)(uint32_t us);  /* 5 us settling delay */
    uint16_t (*read_adc_ch3)(void);          /* PA4 / 4051 #1 COM (Pads 0..7) */
    uint16_t (*read_adc_ch4)(void);          /* PA6 / 4051 #2 COM (Pads 8..15) */
} smc_pad_hal_t;

typedef struct {
    smc_pad_channel_state_t pads[SMC_NUM_PADS];
    uint16_t delta_strike;          /* Offset above baseline required to trigger strike (e.g. 50) */
    uint16_t delta_release;         /* Offset above baseline required to release (e.g. 25) */
    uint16_t max_adc;               /* Max ADC reading (e.g. 1023) */
    uint32_t strike_window_ticks;   /* Peak sampling window (e.g. 3 ms) */
    smc_velocity_curve_type_t curve;/* Active velocity curve */
    smc_pad_hal_t hal;
} smc_pad_matrix_t;

/**
 * @brief Initialize pad matrix engine with calibrated relative thresholds and HAL callbacks.
 */
void smc_pad_init(smc_pad_matrix_t *engine,
                 const smc_pad_hal_t *hal,
                 uint16_t delta_strike,
                 uint16_t delta_release,
                 uint16_t max_adc,
                 uint32_t strike_window_ticks,
                 smc_velocity_curve_type_t curve);

/**
 * @brief Set the active velocity curve for all pads.
 */
void smc_pad_set_curve(smc_pad_matrix_t *engine, smc_velocity_curve_type_t curve);

/**
 * @brief Calibrate initial baseline for all pads from current readings.
 */
void smc_pad_calibrate_baseline(smc_pad_matrix_t *engine);

/**
 * @brief Perform one complete scan pass over the 16 pads (8 mux cycles).
 */
void smc_pad_scan_step(smc_pad_matrix_t *engine,
                      smc_event_queue_t *queue,
                      uint32_t current_timestamp_ticks);

#ifdef __cplusplus
}
#endif

#endif /* SMC_PAD_ADC_H */
