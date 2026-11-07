/**
 * @file ring_buffer.h
 * @brief Lock-Free Single-Producer Single-Consumer (SPSC) Ring Buffer for SMC-Pad
 */

#ifndef SMC_RING_BUFFER_H
#define SMC_RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_NONE = 0,
    EVENT_PAD_STRIKE,      /* Pad struck: index = 0..15, value = velocity (1..127) */
    EVENT_PAD_RELEASE,     /* Pad released: index = 0..15, value = 0 */
    EVENT_ENCODER_DELTA,   /* Encoder moved: index = 0..7, value = signed delta (+1, -1) */
    EVENT_BUTTON_DOWN,     /* Button pressed: index = smc_button_id_t */
    EVENT_BUTTON_UP,       /* Button released: index = smc_button_id_t */
    EVENT_SYSEX_RAW        /* SysEx packet chunk */
} smc_event_type_t;

typedef struct {
    smc_event_type_t type;
    uint8_t index;
    int16_t value;
    uint32_t timestamp;
} smc_event_t;

/* Buffer capacity must be a power of two */
#define SMC_EVENT_QUEUE_CAPACITY 128u
#define SMC_EVENT_QUEUE_MASK     (SMC_EVENT_QUEUE_CAPACITY - 1u)

typedef struct {
    smc_event_t buffer[SMC_EVENT_QUEUE_CAPACITY];
    volatile uint32_t head;       /* Read pointer (consumed by main loop) */
    volatile uint32_t tail;       /* Write pointer (produced by ISRs) */
    volatile uint32_t dropped;    /* Overflow counter */
} smc_event_queue_t;

/**
 * @brief Initialize an event queue.
 */
void smc_queue_init(smc_event_queue_t *queue);

/**
 * @brief Push an event onto the queue (called from ISR).
 * @return true if enqueued, false if queue is full.
 */
bool smc_queue_push(smc_event_queue_t *queue, const smc_event_t *event);

/**
 * @brief Pop an event from the queue (called from main loop).
 * @return true if an event was popped, false if queue is empty.
 */
bool smc_queue_pop(smc_event_queue_t *queue, smc_event_t *out_event);

/**
 * @brief Check if queue is empty.
 */
bool smc_queue_is_empty(const smc_event_queue_t *queue);

/**
 * @brief Get count of pending items in the queue.
 */
uint32_t smc_queue_count(const smc_event_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* SMC_RING_BUFFER_H */
