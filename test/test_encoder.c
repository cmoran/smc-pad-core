/**
 * @file test_encoder.c
 * @brief Unit tests for Gray code rotary encoder engine
 */

#include <stdio.h>
#include <assert.h>
#include "encoder.h"

int main(void) {
    printf("Running encoder engine unit tests...\n");

    smc_event_queue_t queue;
    smc_queue_init(&queue);

    smc_encoder_state_t enc;
    /* 2 substeps per click */
    smc_encoder_init(&enc, 2);

    assert(smc_encoder_get_bank(&enc) == 0);
    assert(smc_encoder_get_value(&enc, 0, 0) == 0);

    /* Clockwise full cycle on encoder 0:
     * 00 -> 01 -> 11 -> 10 -> 00
     * Substep 1: 00 -> 01 (+1)
     * Substep 2: 01 -> 11 (+1) -> triggers tick +1!
     */
    int8_t step = smc_encoder_process_sample(&enc, &queue, 0, 1, 100);
    assert(step == 0);
    assert(smc_queue_is_empty(&queue));

    step = smc_encoder_process_sample(&enc, &queue, 0, 3, 101);
    assert(step == +1);
    assert(!smc_queue_is_empty(&queue));

    smc_event_t ev;
    bool ok = smc_queue_pop(&queue, &ev);
    assert(ok);
    assert(ev.type == EVENT_ENCODER_DELTA);
    assert(ev.index == 0);
    assert(ev.value == +1);
    assert(smc_encoder_get_value(&enc, 0, 0) == 1);

    /* Test counter-clockwise rotation:
     * Current state is 11 (3).
     * CCW step 1: 11 -> 01 (-1)
     * CCW step 2: 01 -> 00 (-1) -> triggers tick -1!
     */
    step = smc_encoder_process_sample(&enc, &queue, 0, 1, 200);
    assert(step == 0);

    step = smc_encoder_process_sample(&enc, &queue, 0, 0, 201);
    assert(step == -1);
    assert(smc_encoder_get_value(&enc, 0, 0) == 0);

    /* Test lower bound clamp: value cannot go below 0 */
    step = smc_encoder_process_sample(&enc, &queue, 0, 2, 300); /* 00 -> 10 CCW */
    step = smc_encoder_process_sample(&enc, &queue, 0, 3, 301); /* 10 -> 11 CCW -> trigger -1 */
    assert(step == -1);
    assert(smc_encoder_get_value(&enc, 0, 0) == 0); /* Still 0 */

    /* Test illegal/bounced transition rejection (00 -> 11 is impossible in single transition) */
    step = smc_encoder_process_sample(&enc, &queue, 1, 0, 400); /* enc 1 at 00 */
    step = smc_encoder_process_sample(&enc, &queue, 1, 3, 401); /* illegal 00 -> 11 */
    assert(step == 0);

    /* Test banking isolation */
    smc_encoder_set_value(&enc, 0, 2, 50);
    assert(smc_encoder_get_value(&enc, 0, 2) == 50);
    assert(smc_encoder_get_value(&enc, 1, 2) == 0);

    smc_encoder_set_bank(&enc, 1);
    assert(smc_encoder_get_bank(&enc) == 1);

    /* Rotate encoder 2 in Bank 1 */
    step = smc_encoder_process_sample(&enc, &queue, 2, 1, 500);
    step = smc_encoder_process_sample(&enc, &queue, 2, 3, 501);
    assert(step == +1);
    assert(smc_encoder_get_value(&enc, 1, 2) == 1);
    /* Bank 0 value remains untouched */
    assert(smc_encoder_get_value(&enc, 0, 2) == 50);

    printf("Encoder engine unit tests passed successfully!\n");
    return 0;
}
