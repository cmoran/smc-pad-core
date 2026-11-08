/**
 * @file smc_config.c
 * @brief Configuration Engine & Virtual Bank Management Implementation
 */

#include "smc_config.h"
#include <string.h>

#ifndef SMC_HOST_TEST
#include "smc_hw.h"
/* Forward declaration of SMC-Pad runtime persistent storage */
#define CFG_SMC_USER_APP_ID   48
extern int smcpad_store_read(int key, void *buf, int len);
extern int smcpad_store_write(int key, const void *buf, int len);
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

    /* Scale Defaults */
    cfg->scale.enabled = false;
    cfg->scale.root_note = 0; /* C */
    cfg->scale.scale_type = SCALE_CHROMATIC;

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
    if (cfg->scale.root_note > 11 || cfg->scale.scale_type >= SCALE_COUNT) return false;

    for (uint8_t b = 0; b < SMC_CONFIG_BANKS; b++) {
        for (uint8_t p = 0; p < SMC_CONFIG_PADS; p++) {
            if (cfg->pads[b][p].note > 127) return false;
            if (cfg->pads[b][p].channel > 15) return false;
            if (cfg->pads[b][p].chord_count > 4) return false;
            if (cfg->pads[b][p].pressure_mode > 1) return false;
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
        int ret = smcpad_store_read(CFG_SMC_USER_APP_ID, cfg, sizeof(smc_config_t));
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
        int ret = smcpad_store_write(CFG_SMC_USER_APP_ID, cfg, sizeof(smc_config_t));
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

/* ========================================================================= */
/* Scale Engine Interval Definitions & Mappers                               */
/* ========================================================================= */

static const uint8_t s_scale_chromatic[12]        = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const uint8_t s_scale_major[7]            = { 0, 2, 4, 5, 7, 9, 11 };
static const uint8_t s_scale_natural_minor[7]    = { 0, 2, 3, 5, 7, 8, 10 };
static const uint8_t s_scale_dorian[7]           = { 0, 2, 3, 5, 7, 9, 10 };
static const uint8_t s_scale_mixolydian[7]       = { 0, 2, 4, 5, 7, 9, 10 };
static const uint8_t s_scale_pentatonic_minor[5] = { 0, 3, 5, 7, 10 };
static const uint8_t s_scale_pentatonic_major[5] = { 0, 2, 4, 7, 9 };
static const uint8_t s_scale_blues[6]            = { 0, 3, 5, 6, 7, 10 };

const uint8_t *smc_scale_get_intervals(smc_scale_type_t scale, uint8_t *len_out) {
    switch (scale) {
        case SCALE_MAJOR:
            if (len_out) *len_out = 7;
            return s_scale_major;
        case SCALE_NATURAL_MINOR:
            if (len_out) *len_out = 7;
            return s_scale_natural_minor;
        case SCALE_DORIAN:
            if (len_out) *len_out = 7;
            return s_scale_dorian;
        case SCALE_MIXOLYDIAN:
            if (len_out) *len_out = 7;
            return s_scale_mixolydian;
        case SCALE_PENTATONIC_MINOR:
            if (len_out) *len_out = 5;
            return s_scale_pentatonic_minor;
        case SCALE_PENTATONIC_MAJOR:
            if (len_out) *len_out = 5;
            return s_scale_pentatonic_major;
        case SCALE_BLUES:
            if (len_out) *len_out = 6;
            return s_scale_blues;
        case SCALE_CHROMATIC:
        default:
            if (len_out) *len_out = 12;
            return s_scale_chromatic;
    }
}

uint8_t smc_scale_get_pad_note(const smc_scale_config_t *scale_cfg, uint8_t pad_idx, uint8_t base_note) {
    if (!scale_cfg || !scale_cfg->enabled || scale_cfg->scale_type == SCALE_CHROMATIC) {
        return (uint8_t)(base_note + pad_idx);
    }

    uint8_t len = 0;
    const uint8_t *intervals = smc_scale_get_intervals(scale_cfg->scale_type, &len);
    if (!intervals || len == 0) {
        return (uint8_t)(base_note + pad_idx);
    }

    uint8_t root = scale_cfg->root_note % 12;
    uint8_t base_octave = base_note / 12;
    uint8_t root_base = (base_octave * 12) + root;

    uint8_t oct = pad_idx / len;
    uint8_t deg = pad_idx % len;

    int note = root_base + intervals[deg] + (oct * 12);
    if (note < 0) note = 0;
    if (note > 127) note = 127;

    return (uint8_t)note;
}

uint8_t smc_scale_check_note(const smc_scale_config_t *scale_cfg, uint8_t note) {
    if (!scale_cfg) return 1;

    uint8_t root = scale_cfg->root_note % 12;
    uint8_t semitone = (note + 12 - root) % 12;

    if (semitone == 0) {
        return 2; /* Root Note */
    }

    if (!scale_cfg->enabled || scale_cfg->scale_type == SCALE_CHROMATIC) {
        return 1; /* In-scale */
    }

    uint8_t len = 0;
    const uint8_t *intervals = smc_scale_get_intervals(scale_cfg->scale_type, &len);
    if (!intervals || len == 0) {
        return 1;
    }

    for (uint8_t i = 1; i < len; i++) {
        if (intervals[i] == semitone) {
            return 1; /* In-scale */
        }
    }

    return 0; /* Out-of-scale */
}
