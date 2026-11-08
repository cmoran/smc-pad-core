/**
 * @file midi_dispatcher.h
 * @brief MIDI 1.0 Event Dispatcher with Dual-Transport Output, Virtual Banking, and Host RX
 */

#ifndef SMC_MIDI_DISPATCHER_H
#define SMC_MIDI_DISPATCHER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "smc_hw.h"
#include "ring_buffer.h"
#include "encoder.h"
#include "smc_config.h"

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

typedef void (*smc_midi_host_note_cb)(uint8_t channel, uint8_t note, uint8_t velocity, void *ctx);
typedef void (*smc_midi_sysex_byte_cb)(uint8_t byte, void *ctx);

typedef struct {
    /* Legacy fallback per-pad mapping */
    smc_pad_config_t pad_configs[SMC_NUM_PADS];

    /* Active configuration pointer */
    const smc_config_t *config;

    /* Active note tracking bitmask (bit i = pad i is currently sounding) */
    uint16_t active_pads_mask;

    /* Shift state tracker */
    bool shift_active;

    /* Octave & Channel Transpose Offsets */
    int8_t  octave_offset;      /* Semitones offset (-48..+48), multiple of 12 */
    int8_t  channel_override;   /* -1 = use per-pad channel, 0..15 = global override */

    /* Dual Output Transport Callbacks */
    void (*send_midi_msg)(const smc_midi_raw_msg_t *msg);     /* Primary / USB */
    void (*send_ble_msg)(const smc_midi_raw_msg_t *msg);      /* Wireless BLE-MIDI */

    /* Incoming MIDI Stream Parsing State */
    uint8_t                 rx_running_status;
    uint8_t                 rx_expected_len;
    uint8_t                 rx_idx;
    uint8_t                 rx_bytes[3];
    bool                    rx_in_sysex;
    smc_midi_host_note_cb   host_note_cb;
    smc_midi_sysex_byte_cb  sysex_cb;
    void                   *host_cb_ctx;
} smc_midi_dispatcher_t;

/**
 * @brief Initialize MIDI dispatcher with default GM Drum map.
 */
void smc_midi_init(smc_midi_dispatcher_t *dispatcher,
                  void (*send_cb)(const smc_midi_raw_msg_t *msg));

/**
 * @brief Associate active configuration engine with dispatcher.
 */
void smc_midi_set_config(smc_midi_dispatcher_t *dispatcher, const smc_config_t *config);

/**
 * @brief Register BLE-MIDI output sink callback for concurrent dual dispatch.
 */
void smc_midi_set_ble_sink(smc_midi_dispatcher_t *dispatcher,
                           void (*ble_send_cb)(const smc_midi_raw_msg_t *msg));

/**
 * @brief Register incoming MIDI stream callbacks (Host DAW LED note feedback & SysEx).
 */
void smc_midi_set_host_callbacks(smc_midi_dispatcher_t *dispatcher,
                                smc_midi_host_note_cb note_cb,
                                smc_midi_sysex_byte_cb sysex_cb,
                                void *ctx);

/**
 * @brief Feed a single byte from incoming USB or BLE MIDI stream.
 */
void smc_midi_rx_feed_byte(smc_midi_dispatcher_t *dispatcher, uint8_t byte);

/**
 * @brief Feed a buffer of incoming MIDI stream bytes.
 */
void smc_midi_rx_feed_bytes(smc_midi_dispatcher_t *dispatcher, const uint8_t *data, size_t len);

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
 * @brief Set global octave offset in semitones (-48..+48).
 */
void smc_midi_set_octave_offset(smc_midi_dispatcher_t *dispatcher, int8_t semitones);

/**
 * @brief Get current octave offset in semitones.
 */
int8_t smc_midi_get_octave_offset(const smc_midi_dispatcher_t *dispatcher);

/**
 * @brief Set global MIDI channel override (-1 for per-pad config, 0..15 to override).
 */
void smc_midi_set_channel_override(smc_midi_dispatcher_t *dispatcher, int8_t channel);

/**
 * @brief Get current global MIDI channel override.
 */
int8_t smc_midi_get_channel_override(const smc_midi_dispatcher_t *dispatcher);

/**
 * @brief Check if a pad is currently sounding (active Note-On without Note-Off).
 */
bool smc_midi_is_pad_active(const smc_midi_dispatcher_t *dispatcher, uint8_t pad_idx);

#ifdef __cplusplus
}
#endif

#endif /* SMC_MIDI_DISPATCHER_H */
