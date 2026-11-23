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

    /* Per Apple/MMA BLE-MIDI spec, 0xF7 must be preceded by a timestamp byte */
    bool has_end = (sysex_data[len - 1] == 0xF7u);
    uint16_t body_len = has_end ? (uint16_t)(len - 1) : len;

    uint16_t offset = 0;
    bool is_first = true;
    bool end_sent = !has_end;

    while (offset < body_len) {
        uint16_t hdr_len = is_first ? 2u : 1u;
        uint16_t room = (max_payload > hdr_len) ? (uint16_t)(max_payload - hdr_len) : 1u;

        ble->tx_buf[0] = header;
        if (is_first) {
            ble->tx_buf[1] = ts_low;
        }

        uint16_t chunk = (uint16_t)(body_len - offset);
        if (chunk > room) {
            chunk = room;
        }

        memcpy(&ble->tx_buf[hdr_len], &sysex_data[offset], chunk);
        uint16_t pkt_len = (uint16_t)(hdr_len + chunk);
        offset = (uint16_t)(offset + chunk);
        is_first = false;

        /* If we finished the body and 2 bytes remain in this packet, append [ts_low, 0xF7] */
        if (offset == body_len && has_end && (uint16_t)(max_payload - pkt_len) >= 2u) {
            ble->tx_buf[pkt_len++] = ts_low;
            ble->tx_buf[pkt_len++] = 0xF7u;
            end_sent = true;
        }

        if (!ble->send_fn(ble->tx_buf, pkt_len)) {
            return false;
        }
    }

    if (!end_sent) {
        ble->tx_buf[0] = header;
        ble->tx_buf[1] = ts_low;
        ble->tx_buf[2] = 0xF7u;
        if (!ble->send_fn(ble->tx_buf, 3u)) {
            return false;
        }
    }

    return true;
}

void smc_ble_midi_parse_rx(const uint8_t *data,
                           uint16_t len,
                           void (*on_byte)(uint8_t byte, void *ctx),
                           void *user_ctx) {
    if (!data || len < 2 || !on_byte) return;

    /* First byte must be header (bit 7 set) */
    if (!(data[0] & 0x80u)) return;

    /*
     * Parse BLE-MIDI stream:
     * data[0] is Header (0x80 | ts_high).
     * data[1] is Timestamp Low (bit 7 set) for standard/SysEx-start packets,
     *         OR SysEx continuation payload (bit 7 clear) for multi-packet SysEx.
     */
    uint16_t i = (data[1] & 0x80u) ? 2u : 1u;
    while (i < len) {
        uint8_t b = data[i];

        if ((b & 0x80u) && b != 0xF0u && b != 0xF7u) {
            /* Inline timestamp byte preceding any status byte (including 0xF7) */
            if ((i + 1 < len) && (data[i + 1] & 0x80u)) {
                i++;
                continue;
            }
        }

        on_byte(b, user_ctx);
        i++;
    }
}
