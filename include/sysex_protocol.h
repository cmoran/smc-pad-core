/**
 * @file sysex_protocol.h
 * @brief Open SysEx Configuration Protocol for SMC-Pad (Manufacturer ID 0x7D)
 *
 * Provides bidirectional querying and parameter configuration for WebMIDI
 * and DAW integrations without proprietary host software.
 */

#ifndef SMC_SYSEX_PROTOCOL_H
#define SMC_SYSEX_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "smc_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMC_SYSEX_START             0xF0u
#define SMC_SYSEX_END               0xF7u
#define SMC_SYSEX_MFG_ID            0x7Du /* Educational / Open Standard */
#define SMC_SYSEX_DEV_ID            0x01u /* SMC-Pad Device ID */

#define SMC_SYSEX_MAX_LEN           64

/* Protocol Command Opcodes */
#define SMC_CMD_GET_DEV_INFO        0x01u
#define SMC_CMD_RESP_DEV_INFO       0x02u

#define SMC_CMD_GET_PAD_MAP         0x10u
#define SMC_CMD_SET_PAD_MAP         0x11u
#define SMC_CMD_RESP_PAD_MAP        0x12u

#define SMC_CMD_GET_ENC_MAP         0x14u
#define SMC_CMD_SET_ENC_MAP         0x15u
#define SMC_CMD_RESP_ENC_MAP        0x16u

#define SMC_CMD_GET_CALIBRATION     0x20u
#define SMC_CMD_SET_CALIBRATION     0x21u
#define SMC_CMD_RESP_CALIBRATION    0x22u

#define SMC_CMD_GET_RGB_CFG         0x30u
#define SMC_CMD_SET_RGB_CFG         0x31u
#define SMC_CMD_RESP_RGB_CFG        0x32u

#define SMC_CMD_SAVE_TO_FLASH       0x70u
#define SMC_CMD_RESP_ACK            0x7Eu
#define SMC_CMD_FACTORY_RESET       0x7Fu

/* Status Codes */
#define SMC_STATUS_OK               0x00u
#define SMC_STATUS_ERR_CMD          0x01u
#define SMC_STATUS_ERR_PARAM        0x02u
#define SMC_STATUS_ERR_FLASH        0x03u

/* Transmit callback */
typedef void (*smc_sysex_tx_fn)(const uint8_t *data, size_t len);

/* SysEx Engine State */
typedef struct {
    smc_config_t     *cfg;
    smc_sysex_tx_fn   tx_fn;
    uint8_t           rx_buf[SMC_SYSEX_MAX_LEN];
    uint8_t           rx_idx;
    bool              in_sysex;
} smc_sysex_engine_t;

/**
 * @brief Initialize the SysEx configuration engine.
 *
 * @param engine Pointer to engine instance
 * @param cfg Pointer to active configuration
 * @param tx_fn Function pointer for transmitting response SysEx packets
 */
void smc_sysex_init(smc_sysex_engine_t *engine, smc_config_t *cfg, smc_sysex_tx_fn tx_fn);

/**
 * @brief Feed a single byte from the incoming MIDI stream.
 *
 * @param engine SysEx engine
 * @param byte Incoming byte
 */
void smc_sysex_feed_byte(smc_sysex_engine_t *engine, uint8_t byte);

/**
 * @brief Feed a buffer of incoming MIDI bytes.
 *
 * @param engine SysEx engine
 * @param data Byte buffer
 * @param len Buffer length
 */
void smc_sysex_feed_bytes(smc_sysex_engine_t *engine, const uint8_t *data, size_t len);

/* 7-bit MIDI bit-packing helper functions */
static inline void smc_pack_u16_to_7bit(uint16_t val, uint8_t *lsb, uint8_t *msb) {
    if (lsb) *lsb = (uint8_t)(val & 0x7Fu);
    if (msb) *msb = (uint8_t)((val >> 7) & 0x7Fu);
}

static inline uint16_t smc_unpack_7bit_to_u16(uint8_t lsb, uint8_t msb) {
    return (uint16_t)((lsb & 0x7Fu) | ((uint16_t)(msb & 0x7Fu) << 7));
}

static inline void smc_pack_color_to_7bit(uint32_t rgb, uint8_t out[4]) {
    if (!out) return;
    out[0] = (uint8_t)(rgb & 0x7Fu);
    out[1] = (uint8_t)((rgb >> 7) & 0x7Fu);
    out[2] = (uint8_t)((rgb >> 14) & 0x7Fu);
    out[3] = (uint8_t)((rgb >> 21) & 0x07u);
}

static inline uint32_t smc_unpack_7bit_to_color(const uint8_t in[4]) {
    if (!in) return 0;
    return (uint32_t)(in[0] & 0x7Fu) |
           ((uint32_t)(in[1] & 0x7Fu) << 7) |
           ((uint32_t)(in[2] & 0x7Fu) << 14) |
           ((uint32_t)(in[3] & 0x07u) << 21);
}

#ifdef __cplusplus
}
#endif

#endif /* SMC_SYSEX_PROTOCOL_H */
