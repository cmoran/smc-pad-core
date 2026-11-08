/**
 * @file smc_config.c
 * @brief Configuration Engine & Virtual Bank Management Implementation
 */

#include "smc_config.h"
#include <string.h>

#ifndef SMC_HOST_TEST
#include "smc_hw.h"
/* Forward declaration of JieLi SDK syscfg if present in target */
#define CFG_SMC_USER_APP_ID   48
extern int syscfg_read(uint16_t item_id, void *buf, uint16_t len);
extern int syscfg_write(uint16_t item_id, const void *buf, uint16_t len);
#endif

/* Active virtual banks */
static uint8_t g_active_pad_bank = 0;
static uint8_t g_active_encoder_bank = 0;

/* Registered flash persistence callbacks */
static smc_flash_read_fn  g_flash_read = NULL;
static smc_flash_write_fn g_flash_write = NULL;

/* Host simulation flash buffer (for tests when no callback provided) */
#ifdef SMC_HOST_TEST
static uint8_t g_sim_flash[sizeof(smc_config_t)];
static bool    g_sim_flash_valid = false;
#endif

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) */
uint16_t smc_config_calc_crc(const smc_config_t *cfg) {
    if (!cfg) return 0;

    const uint8_t *data = (const uint8_t *)&cfg->pads;
    size_t length = sizeof(smc_config_t) - offsetof(smc_config_t, pads);

    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (crc << 1) ^ 0x1021u;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void smc_config_set_defaults(smc_config_t *cfg) {
    if (!cfg) return;

    memset(cfg, 0, sizeof(smc_config_t));
    cfg->magic = SMC_CONFIG_MAGIC;
    cfg->version = SMC_CONFIG_VERSION;

    /* Initialize 4 Virtual Banks of 16 Pads */
    for (uint8_t bank = 0; bank < SMC_CONFIG_BANKS; bank++) {
        for (uint8_t pad = 0; pad < SMC_CONFIG_PADS; pad++) {
            /* Bank 0: 36..51, Bank 1: 52..67, Bank 2: 68..83, Bank 3: 84..99 */
            cfg->pads[bank][pad].note = (uint8_t)(36 + (bank * 16) + pad);
            cfg->pads[bank][pad].channel = 0; /* MIDI Channel 1 */
            cfg->pads[bank][pad].mode = 0;    /* Momentary */
        }
    }

    /* Initialize 4 Virtual Banks of 8 Encoders */
    for (uint8_t bank = 0; bank < SMC_CONFIG_BANKS; bank++) {
        for (uint8_t enc = 0; enc < SMC_CONFIG_ENCODERS; enc++) {
            /* Bank 0: 16..23, Bank 1: 24..31, Bank 2: 32..39, Bank 3: 40..47 */
            cfg->encoders[bank][enc].cc = (uint8_t)(16 + (bank * 8) + enc);
            cfg->encoders[bank][enc].channel = 0;
            cfg->encoders[bank][enc].step_size = 1;
            cfg->encoders[bank][enc].min_val = 0;
            cfg->encoders[bank][enc].max_val = 127;
        }
    }

    /* Pad Calibration Defaults */
    cfg->strike_threshold = 50;
    cfg->release_threshold = 25;
    cfg->dynamic_range = 1000;
    cfg->scan_window_ms = 3;
    cfg->velocity_curve = (uint8_t)VELOCITY_CURVE_LINEAR;

    /* Visual Defaults */
    cfg->pad_idle_color = 0x000020u; /* Dim Blue */
    cfg->pad_hit_color  = 0x00FF00u; /* Bright Green */
    cfg->led_mode       = SMC_LED_MODE_LOCAL;

    /* Compute CRC */
    cfg->crc16 = smc_config_calc_crc(cfg);
}

bool smc_config_validate(const smc_config_t *cfg) {
    if (!cfg) return false;
    if (cfg->magic != SMC_CONFIG_MAGIC) return false;
    if (cfg->version != SMC_CONFIG_VERSION) return false;

    /* Validate CRC */
    if (cfg->crc16 != smc_config_calc_crc(cfg)) {
        return false;
    }

    /* Validate sanity of parameters */
    if (cfg->velocity_curve > VELOCITY_CURVE_FIXED) return false;
    if (cfg->strike_threshold == 0 || cfg->release_threshold >= cfg->strike_threshold) return false;
    if (cfg->dynamic_range == 0) return false;
    if (cfg->scan_window_ms == 0 || cfg->scan_window_ms > 20) return false;
    if (cfg->led_mode > SMC_LED_MODE_HOST) return false;

    for (uint8_t b = 0; b < SMC_CONFIG_BANKS; b++) {
        for (uint8_t p = 0; p < SMC_CONFIG_PADS; p++) {
            if (cfg->pads[b][p].note > 127) return false;
            if (cfg->pads[b][p].channel > 15) return false;
        }
        for (uint8_t e = 0; e < SMC_CONFIG_ENCODERS; e++) {
            if (cfg->encoders[b][e].cc > 127) return false;
            if (cfg->encoders[b][e].channel > 15) return false;
            if (cfg->encoders[b][e].min_val > cfg->encoders[b][e].max_val) return false;
        }
    }

    return true;
}

void smc_config_init(smc_config_t *cfg, smc_flash_read_fn read_fn, smc_flash_write_fn write_fn) {
    g_flash_read = read_fn;
    g_flash_write = write_fn;

    if (!smc_config_load(cfg)) {
        smc_config_set_defaults(cfg);
        smc_config_save(cfg);
    }
}

bool smc_config_load(smc_config_t *cfg) {
    if (!cfg) return false;

    bool read_ok = false;
    if (g_flash_read) {
        read_ok = g_flash_read(0, cfg, sizeof(smc_config_t));
    }
#ifndef SMC_HOST_TEST
    else {
        int ret = syscfg_read(CFG_SMC_USER_APP_ID, cfg, sizeof(smc_config_t));
        read_ok = (ret == (int)sizeof(smc_config_t));
    }
#else
    else if (g_sim_flash_valid) {
        memcpy(cfg, g_sim_flash, sizeof(smc_config_t));
        read_ok = true;
    }
#endif

    if (read_ok && smc_config_validate(cfg)) {
        return true;
    }

    /* Fallback on invalid flash */
    smc_config_set_defaults(cfg);
    return false;
}

bool smc_config_save(smc_config_t *cfg) {
    if (!cfg) return false;

    cfg->magic = SMC_CONFIG_MAGIC;
    cfg->version = SMC_CONFIG_VERSION;
    cfg->crc16 = smc_config_calc_crc(cfg);

    if (g_flash_write) {
        return g_flash_write(0, cfg, sizeof(smc_config_t));
    }
#ifndef SMC_HOST_TEST
    else {
        int ret = syscfg_write(CFG_SMC_USER_APP_ID, cfg, sizeof(smc_config_t));
        return (ret == (int)sizeof(smc_config_t));
    }
#else
    else {
        memcpy(g_sim_flash, cfg, sizeof(smc_config_t));
        g_sim_flash_valid = true;
        return true;
    }
#endif
}

bool smc_config_factory_reset(smc_config_t *cfg) {
    if (!cfg) return false;
    smc_config_set_defaults(cfg);
    return smc_config_save(cfg);
}

uint8_t smc_config_get_pad_bank(void) {
    return g_active_pad_bank;
}

void smc_config_set_pad_bank(uint8_t bank) {
    g_active_pad_bank = bank & 0x03u;
}

uint8_t smc_config_get_encoder_bank(void) {
    return g_active_encoder_bank;
}

void smc_config_set_encoder_bank(uint8_t bank) {
    g_active_encoder_bank = bank & 0x03u;
}

const smc_pad_cfg_t *smc_config_get_active_pad(const smc_config_t *cfg, uint8_t pad_idx) {
    if (!cfg || pad_idx >= SMC_CONFIG_PADS) return NULL;
    return &cfg->pads[g_active_pad_bank][pad_idx];
}

const smc_encoder_cfg_t *smc_config_get_active_encoder(const smc_config_t *cfg, uint8_t enc_idx) {
    if (!cfg || enc_idx >= SMC_CONFIG_ENCODERS) return NULL;
    return &cfg->encoders[g_active_encoder_bank][enc_idx];
}
