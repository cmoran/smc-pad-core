/**
 * @file ring_buffer.c
 * @brief Lock-Free SPSC Ring Buffer Implementation
 */

#include "ring_buffer.h"

void smc_queue_init(smc_event_queue_t *queue) {
    if (!queue) {
        return;
    }
    queue->head = 0;
    queue->tail = 0;
    queue->dropped = 0;
}

bool smc_queue_push(smc_event_queue_t *queue, const smc_event_t *event) {
    if (!queue || !event) {
        return false;
    }

    uint32_t head = queue->head;
    uint32_t tail = queue->tail;

    /* Check if full: (tail + 1) == head */
    if ((tail - head) >= SMC_EVENT_QUEUE_CAPACITY) {
        queue->dropped++;
        return false;
    }

    queue->buffer[tail & SMC_EVENT_QUEUE_MASK] = *event;
    
    /* Memory barrier / compiler barrier before committing the write index */
    __asm__ __volatile__("" ::: "memory");
    queue->tail = tail + 1;

    return true;
}

bool smc_queue_pop(smc_event_queue_t *queue, smc_event_t *out_event) {
    if (!queue || !out_event) {
        return false;
    }

    uint32_t head = queue->head;
    uint32_t tail = queue->tail;

    /* Check if empty */
    if (head == tail) {
        return false;
    }

    *out_event = queue->buffer[head & SMC_EVENT_QUEUE_MASK];

    /* Memory barrier before updating head */
    __asm__ __volatile__("" ::: "memory");
    queue->head = head + 1;

    return true;
}

bool smc_queue_is_empty(const smc_event_queue_t *queue) {
    if (!queue) {
        return true;
    }
    return queue->head == queue->tail;
}

uint32_t smc_queue_count(const smc_event_queue_t *queue) {
    if (!queue) {
        return 0;
    }
    return queue->tail - queue->head;
}
