/**
 * @file ble_midi.h
 * @brief Standard Apple/MMA BLE-MIDI GATT Peripheral Protocol Driver
 *
 * Implements BLE-MIDI packetization with 13-bit timestamp framing,
 * multi-packet SysEx transport, and notification transmission.
 */

#ifndef SMC_BLE_MIDI_H
#define SMC_BLE_MIDI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMC_BLE_MIDI_SERVICE_UUID       "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define SMC_BLE_MIDI_CHAR_UUID          "7772e5db-3868-4112-a1a9-f2669d106bf3"

#define SMC_BLE_MIDI_DEFAULT_MTU        23
#define SMC_BLE_MIDI_MAX_PACKET_LEN     128

typedef bool (*smc_ble_send_fn)(const uint8_t *data, uint16_t len);

typedef struct {
    bool             connected;
    bool             subscribed;
    uint16_t         mtu;
    smc_ble_send_fn  send_fn;
    uint8_t          tx_buf[SMC_BLE_MIDI_MAX_PACKET_LEN];
} smc_ble_midi_t;

/**
 * @brief Initialize BLE-MIDI driver instance.
 *
 * @param ble Pointer to BLE-MIDI state
 * @param send_fn Notification transmit callback
 */
void smc_ble_midi_init(smc_ble_midi_t *ble, smc_ble_send_fn send_fn);

/**
 * @brief Update connection status.
 */
void smc_ble_midi_set_connected(smc_ble_midi_t *ble, bool connected);

/**
 * @brief Update GATT Client Characteristic Configuration (CCC 0x2902) notification state.
 */
void smc_ble_midi_set_subscribed(smc_ble_midi_t *ble, bool subscribed);

/**
 * @brief Set negotiated ATT MTU size.
 */
void smc_ble_midi_set_mtu(smc_ble_midi_t *ble, uint16_t mtu);

/**
 * @brief Format and transmit a standard MIDI channel voice message over BLE-MIDI.
 *
 * @param ble BLE-MIDI state
 * @param midi_msg 1 to 3 bytes of raw MIDI (e.g. Note-On, CC)
 * @param len Length of MIDI message (1..3)
 * @param timestamp_ms Millisecond clock for 13-bit BLE-MIDI timestamp header
 * @return true if transmitted, false if not subscribed, disconnected, or error
 */
bool smc_ble_midi_send_message(smc_ble_midi_t *ble,
                               const uint8_t *midi_msg,
                               uint8_t len,
                               uint32_t timestamp_ms);

/**
 * @brief Format and transmit a SysEx byte stream over BLE-MIDI notifications.
 *
 * @param ble BLE-MIDI state
 * @param sysex_data Full SysEx frame (0xF0 ... 0xF7)
 * @param len Length of SysEx data
 * @param timestamp_ms Millisecond timestamp
 * @return true on complete transmission, false on error
 */
bool smc_ble_midi_send_sysex(smc_ble_midi_t *ble,
                             const uint8_t *sysex_data,
                             uint16_t len,
                             uint32_t timestamp_ms);

/**
 * @brief Parse an incoming BLE-MIDI notification or write packet into standard MIDI bytes.
 *
 * Strips BLE-MIDI header and timestamp bytes, emitting naked MIDI stream bytes to callback.
 *
 * @param data Raw BLE packet
 * @param len Packet length
 * @param on_byte Callback invoked for each decoded MIDI byte
 * @param user_ctx User context pointer
 */
void smc_ble_midi_parse_rx(const uint8_t *data,
                           uint16_t len,
                           void (*on_byte)(uint8_t byte, void *ctx),
                           void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif /* SMC_BLE_MIDI_H */
