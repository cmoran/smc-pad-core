/**
 * @file test_ring_buffer.c
 * @brief Unit tests for SPSC Ring Buffer
 */

#include <stdio.h>
#include <assert.h>
#include "ring_buffer.h"

int main(void) {
    printf("Running ring buffer unit tests...\n");

    smc_event_queue_t queue;
    smc_queue_init(&queue);

    assert(smc_queue_is_empty(&queue));
    assert(smc_queue_count(&queue) == 0);

    /* Test single push / pop */
    smc_event_t ev = {
        .type = EVENT_PAD_STRIKE,
        .index = 3,
        .value = 100,
        .timestamp = 1000
    };
    bool ok = smc_queue_push(&queue, &ev);
    assert(ok);
    assert(!smc_queue_is_empty(&queue));
    assert(smc_queue_count(&queue) == 1);

    smc_event_t popped;
    ok = smc_queue_pop(&queue, &popped);
    assert(ok);
    assert(popped.type == EVENT_PAD_STRIKE);
    assert(popped.index == 3);
    assert(popped.value == 100);
    assert(popped.timestamp == 1000);
    assert(smc_queue_is_empty(&queue));

    /* Test pop on empty queue */
    ok = smc_queue_pop(&queue, &popped);
    assert(!ok);

    /* Test filling queue to capacity */
    for (uint32_t i = 0; i < SMC_EVENT_QUEUE_CAPACITY; i++) {
        smc_event_t item = {
            .type = EVENT_ENCODER_DELTA,
            .index = (uint8_t)(i % 8),
            .value = 1,
            .timestamp = i
        };
        ok = smc_queue_push(&queue, &item);
        assert(ok);
    }
    assert(smc_queue_count(&queue) == SMC_EVENT_QUEUE_CAPACITY);

    /* Overflow attempt */
    smc_event_t overflow_item = { .type = EVENT_BUTTON_DOWN, .index = 0, .value = 1, .timestamp = 9999 };
    ok = smc_queue_push(&queue, &overflow_item);
    assert(!ok);
    assert(queue.dropped == 1);

    /* Verify items order (FIFO) */
    for (uint32_t i = 0; i < SMC_EVENT_QUEUE_CAPACITY; i++) {
        ok = smc_queue_pop(&queue, &popped);
        assert(ok);
        assert(popped.type == EVENT_ENCODER_DELTA);
        assert(popped.index == (uint8_t)(i % 8));
        assert(popped.timestamp == i);
    }

    assert(smc_queue_is_empty(&queue));
    printf("Ring buffer tests passed successfully!\n");
    return 0;
}
