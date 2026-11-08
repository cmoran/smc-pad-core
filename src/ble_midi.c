/**
 * @file ble_midi.c
 * @brief Standard Apple/MMA BLE-MIDI GATT Peripheral Protocol Driver Implementation
 */

#include "ble_midi.h"
#include <string.h>

void smc_ble_midi_init(smc_ble_midi_t *ble, smc_ble_send_fn send_fn) {
    if (!ble) return;
    memset(ble, 0, sizeof(smc_ble_midi_t));
    ble->mtu = SMC_BLE_MIDI_DEFAULT_MTU;
    ble->send_fn = send_fn;
}

void smc_ble_midi_set_connected(smc_ble_midi_t *ble, bool connected) {
    if (!ble) return;
    ble->connected = connected;
    if (!connected) {
        ble->subscribed = false;
    }
}

void smc_ble_midi_set_subscribed(smc_ble_midi_t *ble, bool subscribed) {
    if (!ble) return;
    ble->subscribed = subscribed;
}

void smc_ble_midi_set_mtu(smc_ble_midi_t *ble, uint16_t mtu) {
    if (!ble) return;
    if (mtu >= SMC_BLE_MIDI_DEFAULT_MTU) {
        ble->mtu = mtu;
    }
}

bool smc_ble_midi_send_message(smc_ble_midi_t *ble,
                               const uint8_t *midi_msg,
                               uint8_t len,
                               uint32_t timestamp_ms) {
    if (!ble || !ble->connected || !ble->subscribed || !ble->send_fn || !midi_msg || len == 0 || len > 3) {
        return false;
    }

    uint16_t ts = (uint16_t)(timestamp_ms & 0x1FFFu);
    uint8_t header = (uint8_t)(0x80u | ((ts >> 7) & 0x3Fu));
    uint8_t ts_low = (uint8_t)(0x80u | (ts & 0x7Fu));

    ble->tx_buf[0] = header;
    ble->tx_buf[1] = ts_low;
    memcpy(&ble->tx_buf[2], midi_msg, len);

    return ble->send_fn(ble->tx_buf, (uint16_t)(len + 2));
}

bool smc_ble_midi_send_sysex(smc_ble_midi_t *ble,
                             const uint8_t *sysex_data,
                             uint16_t len,
                             uint32_t timestamp_ms) {
    if (!ble || !ble->connected || !ble->subscribed || !ble->send_fn || !sysex_data || len == 0) {
        return false;
    }

    uint16_t max_payload = (ble->mtu > 3) ? (uint16_t)(ble->mtu - 3) : 20;
    if (max_payload > SMC_BLE_MIDI_MAX_PACKET_LEN) {
        max_payload = SMC_BLE_MIDI_MAX_PACKET_LEN;
    }

    uint16_t ts = (uint16_t)(timestamp_ms & 0x1FFFu);
    uint8_t header = (uint8_t)(0x80u | ((ts >> 7) & 0x3Fu));
    uint8_t ts_low = (uint8_t)(0x80u | (ts & 0x7Fu));

    uint16_t offset = 0;
    bool is_first = true;

    while (offset < len) {
        uint16_t room;
        if (is_first) {
            ble->tx_buf[0] = header;
            ble->tx_buf[1] = ts_low;
            room = (max_payload > 2) ? (uint16_t)(max_payload - 2) : 1;
        } else {
            ble->tx_buf[0] = header;
            room = (max_payload > 1) ? (uint16_t)(max_payload - 1) : 1;
        }

        uint16_t chunk = (uint16_t)(len - offset);
        if (chunk > room) {
            chunk = room;
        }

        uint16_t pkt_len;
        if (is_first) {
            memcpy(&ble->tx_buf[2], &sysex_data[offset], chunk);
            pkt_len = (uint16_t)(chunk + 2);
            is_first = false;
        } else {
            memcpy(&ble->tx_buf[1], &sysex_data[offset], chunk);
            pkt_len = (uint16_t)(chunk + 1);
        }

        if (!ble->send_fn(ble->tx_buf, pkt_len)) {
            return false;
        }

        offset = (uint16_t)(offset + chunk);
    }

    return true;
}

void smc_ble_midi_parse_rx(const uint8_t *data,
                           uint16_t len,
                           void (*on_byte)(uint8_t byte, void *ctx),
                           void *user_ctx) {
    if (!data || len < 3 || !on_byte) return;

    /* First byte must be header (bit 7 set) */
    if (!(data[0] & 0x80u)) return;

    /*
     * Parse BLE-MIDI stream:
     * data[0] is Header (0x80 | ts_high).
     * data[1] is Timestamp Low (0x80 | ts_low).
     * Subsequent bytes are MIDI status or data bytes.
     * If a byte has bit 7 set and is followed by another byte with bit 7 set (or is in timestamp position),
     * it represents a sub-packet timestamp.
     */
    uint16_t i = 2; /* Skip header and initial timestamp */
    while (i < len) {
        uint8_t b = data[i];

        if (b & 0x80u) {
            /* Check if this is an inline timestamp byte preceding another status byte */
            if ((i + 1 < len) && (data[i + 1] & 0x80u) && (data[i + 1] != 0xF7u)) {
                /* data[i] is an inline timestamp; skip it */
                i++;
                continue;
            }
        }

        on_byte(b, user_ctx);
        i++;
    }
}
