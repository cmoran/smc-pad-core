/**
 * @file midi_dispatcher.h
 * @brief MIDI 1.0 Event Dispatcher with Active Note Tracking and Panic Protocol
 */

#ifndef SMC_MIDI_DISPATCHER_H
#define SMC_MIDI_DISPATCHER_H

#include <stdint.h>
#include <stdbool.h>
#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t bytes[3];
    uint8_t len;
} smc_midi_raw_msg_t;

typedef struct {
    uint8_t note_number;
    uint8_t midi_channel;   /* 0..15 */
} smc_pad_config_t;

typedef struct {
    /* Per-pad mapping */
    smc_pad_config_t pad_configs[SMC_NUM_PADS];

    /* Active note tracking bitmask (bit i = pad i is currently sounding) */
    uint16_t active_pads_mask;

    /* Shift state tracker */
    bool shift_active;

    /* Output callback */
    void (*send_midi_msg)(const smc_midi_raw_msg_t *msg);
} smc_midi_dispatcher_t;

/**
 * @brief Initialize MIDI dispatcher with default GM Drum map (Notes 36..51 on Channel 10/1).
 */
void smc_midi_init(smc_midi_dispatcher_t *dispatcher,
                  void (*send_cb)(const smc_midi_raw_msg_t *msg));

/**
 * @brief Process an incoming event from the ring buffer.
 */
void smc_midi_process_event(smc_midi_dispatcher_t *dispatcher,
                           smc_encoder_state_t *encoder_state,
                           const smc_event_t *event);

/**
 * @brief Trigger an immediate MIDI Panic (All Sound Off CC 120, All Notes Off CC 123, Note-Off for active notes).
 */
void smc_midi_panic(smc_midi_dispatcher_t *dispatcher);

/**
 * @brief Check if a pad is currently sounding (active Note-On without Note-Off).
 */
bool smc_midi_is_pad_active(const smc_midi_dispatcher_t *dispatcher, uint8_t pad_idx);

#ifdef __cplusplus
}
#endif

#endif /* SMC_MIDI_DISPATCHER_H */
