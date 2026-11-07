/**
 * @file encoder.c
 * @brief Deterministic Gray Code Rotary Encoder Engine Implementation
 */

#include "encoder.h"

/*
 * Transition lookup table indexed by (old_state << 2) | new_state
 * Valid states: 00(0), 01(1), 11(3), 10(2)
 *
 * Sequence CW:  0 -> 1 -> 3 -> 2 -> 0
 * Sequence CCW: 0 -> 2 -> 3 -> 1 -> 0
 */
static const int8_t GRAY_TRANSITION_TABLE[16] = {
    /* old = 00 (0) */
    0,   /* new = 00 -> 0 */
    +1,  /* new = 01 -> CW */
    -1,  /* new = 10 -> CCW */
    0,   /* new = 11 -> invalid */

    /* old = 01 (1) */
    -1,  /* new = 00 -> CCW */
    0,   /* new = 01 -> 0 */
    0,   /* new = 10 -> invalid */
    +1,  /* new = 11 -> CW */

    /* old = 10 (2) */
    +1,  /* new = 00 -> CW */
    0,   /* new = 01 -> invalid */
    0,   /* new = 10 -> 0 */
    -1,  /* new = 11 -> CCW */

    /* old = 11 (3) */
    0,   /* new = 00 -> invalid */
    -1,  /* new = 01 -> CCW */
    +1,  /* new = 10 -> CW */
    0    /* new = 11 -> 0 */
};

void smc_encoder_init(smc_encoder_state_t *state, uint8_t substeps_per_tick) {
    if (!state) {
        return;
    }
    state->active_bank = 0;
    state->substeps_per_tick = (substeps_per_tick > 0) ? substeps_per_tick : 2;

    for (uint8_t i = 0; i < SMC_NUM_ENCODERS; i++) {
        state->last_gray_state[i] = 0;
        state->substep_accum[i] = 0;
    }

    /* Initialize default CCs: Bank 0: CC 20..27, Bank 1: CC 28..35, etc. */
    for (uint8_t b = 0; b < SMC_NUM_BANKS; b++) {
        for (uint8_t i = 0; i < SMC_NUM_ENCODERS; i++) {
            state->configs[b][i].midi_channel = 0;
            state->configs[b][i].cc_number = (uint8_t)(20 + (b * SMC_NUM_ENCODERS) + i);
            state->configs[b][i].value = 0;
        }
    }
}

bool smc_encoder_set_bank(smc_encoder_state_t *state, uint8_t bank) {
    if (!state || bank >= SMC_NUM_BANKS) {
        return false;
    }
    state->active_bank = bank;
    return true;
}

uint8_t smc_encoder_get_bank(const smc_encoder_state_t *state) {
    return state ? state->active_bank : 0;
}

int8_t smc_encoder_process_sample(smc_encoder_state_t *state,
                                 smc_event_queue_t *queue,
                                 uint8_t enc_idx,
                                 uint8_t curr_gray,
                                 uint32_t timestamp) {
    if (!state || enc_idx >= SMC_NUM_ENCODERS) {
        return 0;
    }

    uint8_t prev_gray = state->last_gray_state[enc_idx] & 0x03;
    curr_gray &= 0x03;

    if (prev_gray == curr_gray) {
        return 0; /* No change */
    }

    uint8_t table_idx = (uint8_t)((prev_gray << 2) | curr_gray);
    int8_t delta = GRAY_TRANSITION_TABLE[table_idx];

    state->last_gray_state[enc_idx] = curr_gray;

    if (delta == 0) {
        /* Skipped / illegal transition, reject */
        return 0;
    }

    state->substep_accum[enc_idx] += delta;

    int8_t tick_step = 0;
    if (state->substep_accum[enc_idx] >= (int8_t)state->substeps_per_tick) {
        tick_step = +1;
        state->substep_accum[enc_idx] = 0;
    } else if (state->substep_accum[enc_idx] <= -((int8_t)state->substeps_per_tick)) {
        tick_step = -1;
        state->substep_accum[enc_idx] = 0;
    }

    if (tick_step != 0) {
        uint8_t bank = state->active_bank;
        int16_t cur_val = state->configs[bank][enc_idx].value;
        int16_t new_val = cur_val + tick_step;

        /* Clamp to 0..127 */
        if (new_val < 0) {
            new_val = 0;
        } else if (new_val > 127) {
            new_val = 127;
        }

        state->configs[bank][enc_idx].value = (uint8_t)new_val;

        if (queue) {
            smc_event_t ev = {
                .type = EVENT_ENCODER_DELTA,
                .index = enc_idx,
                .value = tick_step,
                .timestamp = timestamp
            };
            smc_queue_push(queue, &ev);
        }
    }

    return tick_step;
}

uint8_t smc_encoder_get_value(const smc_encoder_state_t *state, uint8_t bank, uint8_t enc_idx) {
    if (!state || bank >= SMC_NUM_BANKS || enc_idx >= SMC_NUM_ENCODERS) {
        return 0;
    }
    return state->configs[bank][enc_idx].value;
}

void smc_encoder_set_value(smc_encoder_state_t *state, uint8_t bank, uint8_t enc_idx, uint8_t val) {
    if (!state || bank >= SMC_NUM_BANKS || enc_idx >= SMC_NUM_ENCODERS) {
        return;
    }
    state->configs[bank][enc_idx].value = (val > 127) ? 127 : val;
}
