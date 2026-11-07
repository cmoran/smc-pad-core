/**
 * @file encoder.h
 * @brief Deterministic Gray Code Rotary Encoder Engine & Virtual Banking
 */

#ifndef SMC_ENCODER_H
#define SMC_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include "ring_buffer.h"
#include "smc_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t cc_number;
    uint8_t midi_channel;   /* 0..15 */
    uint8_t value;          /* 0..127 */
} smc_encoder_config_t;

typedef struct {
    uint8_t last_gray_state[SMC_NUM_ENCODERS]; /* 2-bit state (0..3) */
    int8_t  substep_accum[SMC_NUM_ENCODERS];   /* Substep accumulator */
    uint8_t substeps_per_tick;                 /* Configurable resolution (1, 2, or 4) */
    uint8_t active_bank;                       /* 0..3 */
    smc_encoder_config_t configs[SMC_NUM_BANKS][SMC_NUM_ENCODERS];
} smc_encoder_state_t;

/**
 * @brief Initialize encoder engine with default CC maps and banks.
 */
void smc_encoder_init(smc_encoder_state_t *state, uint8_t substeps_per_tick);

/**
 * @brief Switch active encoder bank (0..3).
 */
bool smc_encoder_set_bank(smc_encoder_state_t *state, uint8_t bank);

/**
 * @brief Get current active bank.
 */
uint8_t smc_encoder_get_bank(const smc_encoder_state_t *state);

/**
 * @brief Process a raw 2-bit quadrature sample for a physical encoder.
 *
 * @param state Pointer to encoder engine state
 * @param queue Pointer to event queue where movement events are pushed
 * @param enc_idx Physical encoder index (0..7)
 * @param curr_gray Current 2-bit Gray code: (pinA << 1) | pinB
 * @param timestamp Tick timestamp
 * @return int8_t Direction step (-1, 0, +1) if a full tick was triggered
 */
int8_t smc_encoder_process_sample(smc_encoder_state_t *state,
                                 smc_event_queue_t *queue,
                                 uint8_t enc_idx,
                                 uint8_t curr_gray,
                                 uint32_t timestamp);

/**
 * @brief Get the current CC value for a given bank and encoder.
 */
uint8_t smc_encoder_get_value(const smc_encoder_state_t *state, uint8_t bank, uint8_t enc_idx);

/**
 * @brief Directly set the value of an encoder in a bank (e.g. from DAW feedback).
 */
void smc_encoder_set_value(smc_encoder_state_t *state, uint8_t bank, uint8_t enc_idx, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* SMC_ENCODER_H */
