/**
 * @file smc_config.h
 * @brief Configuration Engine & Virtual Bank Management for SMC-Pad
 *
 * Implements non-volatile persistent storage and 4 virtual banks for
 * the 16 velocity pads and 8 rotary encoders.
 */

#ifndef SMC_CONFIG_H
#define SMC_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "velocity_curve.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMC_CONFIG_BANKS        4
#define SMC_CONFIG_PADS         16
#define SMC_CONFIG_ENCODERS     8
#define SMC_CONFIG_MAGIC        0x534D4331u /* "SMC1" */
#define SMC_CONFIG_VERSION      1

#define SMC_LED_MODE_LOCAL      0
#define SMC_LED_MODE_HOST       1

typedef struct {
    uint8_t note;            /* MIDI Note Number (0..127) */
    uint8_t channel;         /* MIDI Channel (0..15) */
    uint8_t mode;            /* 0 = Momentary, 1 = Toggle */
    uint8_t reserved;
} smc_pad_cfg_t;

typedef struct {
    uint8_t cc;              /* MIDI CC Number (0..127) */
    uint8_t channel;         /* MIDI Channel (0..15) */
    uint8_t step_size;       /* Increment/Decrement per step (default 1) */
    uint8_t min_val;         /* Clamp min (default 0) */
    uint8_t max_val;         /* Clamp max (default 127) */
    uint8_t reserved[3];
} smc_encoder_cfg_t;

typedef struct {
    uint32_t magic;          /* SMC_CONFIG_MAGIC */
    uint16_t version;        /* SMC_CONFIG_VERSION */
    uint16_t crc16;          /* CRC16 of config payload following header */

    /* Bank Mappings */
    smc_pad_cfg_t     pads[SMC_CONFIG_BANKS][SMC_CONFIG_PADS];
    smc_encoder_cfg_t encoders[SMC_CONFIG_BANKS][SMC_CONFIG_ENCODERS];

    /* Pad Calibration & Velocity Engine */
    uint16_t strike_threshold;
    uint16_t release_threshold;
    uint16_t dynamic_range;
    uint8_t  scan_window_ms;
    uint8_t  velocity_curve; /* smc_velocity_curve_type_t */

    /* RGB LED Colors (0xRRGGBB) */
    uint32_t pad_idle_color;
    uint32_t pad_hit_color;
    uint8_t  led_mode;       /* SMC_LED_MODE_LOCAL / SMC_LED_MODE_HOST */
    uint8_t  reserved[3];
} smc_config_t;

/* HAL Flash Read/Write callbacks */
typedef bool (*smc_flash_read_fn)(uint32_t offset, void *dest, size_t size);
typedef bool (*smc_flash_write_fn)(uint32_t offset, const void *src, size_t size);

/**
 * @brief Initialize configuration engine with storage callbacks.
 *
 * @param cfg Pointer to target configuration structure
 * @param read_fn Flash read function (or NULL for host mock)
 * @param write_fn Flash write function (or NULL for host mock)
 */
void smc_config_init(smc_config_t *cfg, smc_flash_read_fn read_fn, smc_flash_write_fn write_fn);

/**
 * @brief Populate configuration with clean factory defaults.
 *
 * @param cfg Target configuration structure
 */
void smc_config_set_defaults(smc_config_t *cfg);

/**
 * @brief Calculate CCITT-16 CRC of the configuration payload.
 *
 * @param cfg Pointer to configuration structure
 * @return uint16_t CRC16 checksum
 */
uint16_t smc_config_calc_crc(const smc_config_t *cfg);

/**
 * @brief Validate magic, version, CRC, and parameters range.
 *
 * @param cfg Pointer to configuration structure
 * @return true if valid, false if corrupted
 */
bool smc_config_validate(const smc_config_t *cfg);

/**
 * @brief Load configuration from non-volatile storage.
 *
 * Falls back to factory defaults if flash is uninitialized or corrupted.
 *
 * @param cfg Target configuration structure
 * @return true if loaded from flash, false if factory defaults loaded
 */
bool smc_config_load(smc_config_t *cfg);

/**
 * @brief Commit configuration to non-volatile storage with updated CRC.
 *
 * @param cfg Configuration structure to persist
 * @return true on success, false on write error
 */
bool smc_config_save(smc_config_t *cfg);

/**
 * @brief Restore factory defaults and immediately commit to non-volatile storage.
 *
 * @param cfg Target configuration structure
 * @return true on success, false on error
 */
bool smc_config_factory_reset(smc_config_t *cfg);

/* Active virtual bank accessors */
uint8_t smc_config_get_pad_bank(void);
void smc_config_set_pad_bank(uint8_t bank);
uint8_t smc_config_get_encoder_bank(void);
void smc_config_set_encoder_bank(uint8_t bank);

const smc_pad_cfg_t *smc_config_get_active_pad(const smc_config_t *cfg, uint8_t pad_idx);
const smc_encoder_cfg_t *smc_config_get_active_encoder(const smc_config_t *cfg, uint8_t enc_idx);

#ifdef __cplusplus
}
#endif

#endif /* SMC_CONFIG_H */
