/**
 * @file matrix.h
 * @brief Matrix Scanner & Non-Interfering Button LED Multiplexer
 */

#ifndef SMC_MATRIX_H
#define SMC_MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMC_MATRIX_NUM_COLS         16
#define SMC_DEBOUNCE_THRESHOLD      4   /* Number of consistent scans to qualify state change */

/* Phase & segment mapping for each of the 10 button LEDs */
typedef struct {
    uint8_t phase;      /* 0, 1, or 2 */
    uint8_t segment;    /* 0, 1, 2, or 3 */
} smc_led_pinmap_t;

/* HAL callbacks for hardware pin driving / sensing (for hardware & host simulation) */
typedef struct {
    /* 74HC595 driving */
    void (*sr_set_clk)(bool level);
    void (*sr_set_dat)(bool level);
    void (*sr_set_lat)(bool level);
    void (*sr_set_oe)(bool level);

    /* Sense inputs */
    bool (*read_sense0)(void);  /* PB0 (encoders) */
    bool (*read_sense1)(void);  /* PC1 (buttons) */

    /* Button LED driving */
    void (*set_led_phase)(uint8_t phase, bool active);  /* PA3, PC2, PC3 (active low) */
    void (*set_led_segment)(uint8_t seg, bool active);  /* PA5, PA2, PA0, PA1 (active low) */
} smc_matrix_hal_t;

typedef struct {
    /* Button debounce state */
    uint8_t button_raw[SMC_NUM_BUTTONS];
    uint8_t button_integrator[SMC_NUM_BUTTONS];
    bool    button_state[SMC_NUM_BUTTONS];

    /* Encoders raw 2-bit state accumulated from sense 0 */
    uint8_t encoder_phase_bits[SMC_NUM_ENCODERS]; /* Bit 0: col A, Bit 1: col B */

    /* LED states (on/off flags for each of the 10 button LEDs) */
    bool    led_states[LED_COUNT];
    uint8_t current_led_phase;  /* 0, 1, 2 */

    /* HAL interface */
    smc_matrix_hal_t hal;
} smc_matrix_state_t;

/**
 * @brief Initialize matrix scanner and LED multiplexer.
 */
void smc_matrix_init(smc_matrix_state_t *matrix, const smc_matrix_hal_t *hal);

/**
 * @brief Set the on/off state of a button LED.
 */
void smc_matrix_set_led(smc_matrix_state_t *matrix, smc_led_id_t led_id, bool on);

/**
 * @brief Get the on/off state of a button LED.
 */
bool smc_matrix_get_led(const smc_matrix_state_t *matrix, smc_led_id_t led_id);

/**
 * @brief Perform a full 16-column matrix scan.
 * Guarantees all LED drive phases are OFF during the scan to avoid PC3/PC1 crosstalk.
 */
void smc_matrix_scan_step(smc_matrix_state_t *matrix,
                         smc_encoder_state_t *encoder_state,
                         smc_event_queue_t *queue,
                         uint32_t timestamp);

/**
 * @brief Advance button LED multiplexing by one phase.
 * Called in an interleaved time slot when matrix scanning is NOT active.
 */
void smc_matrix_led_step(smc_matrix_state_t *matrix);

#ifdef __cplusplus
}
#endif

#endif /* SMC_MATRIX_H */
