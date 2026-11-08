/**
 * @file test_config.c
 * @brief Unit tests for SMC-Pad Configuration Engine and Virtual Banking
 */

#include "smc_config.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static uint8_t s_mock_flash[sizeof(smc_config_t)];
static bool    s_mock_flash_valid = false;

static bool mock_flash_read(uint32_t offset, void *dest, size_t size) {
    (void)offset;
    if (!s_mock_flash_valid || size > sizeof(s_mock_flash)) {
        return false;
    }
    memcpy(dest, s_mock_flash, size);
    return true;
}

static bool mock_flash_write(uint32_t offset, const void *src, size_t size) {
    (void)offset;
    if (size > sizeof(s_mock_flash)) {
        return false;
    }
    memcpy(s_mock_flash, src, size);
    s_mock_flash_valid = true;
    return true;
}

static void test_defaults_and_validation(void) {
    printf("Running test_defaults_and_validation...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);

    assert(cfg.magic == SMC_CONFIG_MAGIC);
    assert(cfg.version == SMC_CONFIG_VERSION);
    assert(smc_config_validate(&cfg) == true);

    /* Verify Bank 0 pads: 36..51 */
    assert(cfg.pads[0][0].note == 36);
    assert(cfg.pads[0][15].note == 51);
    /* Verify Bank 1 pads: 52..67 */
    assert(cfg.pads[1][0].note == 52);
    assert(cfg.pads[1][15].note == 67);

    /* Verify Bank 0 encoders: 16..23 */
    assert(cfg.encoders[0][0].cc == 16);
    assert(cfg.encoders[0][7].cc == 23);
    /* Verify Bank 3 encoders: 40..47 */
    assert(cfg.encoders[3][0].cc == 40);
    assert(cfg.encoders[3][7].cc == 47);

    printf("PASS: test_defaults_and_validation\n");
}

static void test_crc_integrity(void) {
    printf("Running test_crc_integrity...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);
    assert(smc_config_validate(&cfg) == true);

    /* Tamper with a pad note */
    cfg.pads[0][0].note = 100;
    /* Without recalculating CRC, validation must fail */
    assert(smc_config_validate(&cfg) == false);

    /* Update CRC, validation should pass */
    cfg.crc16 = smc_config_calc_crc(&cfg);
    assert(smc_config_validate(&cfg) == true);

    /* Out of range parameters must fail */
    cfg.pads[0][0].note = 128; /* invalid MIDI note */
    cfg.crc16 = smc_config_calc_crc(&cfg);
    assert(smc_config_validate(&cfg) == false);

    printf("PASS: test_crc_integrity\n");
}

static void test_save_load_and_reset(void) {
    printf("Running test_save_load_and_reset...\n");
    s_mock_flash_valid = false;
    memset(s_mock_flash, 0, sizeof(s_mock_flash));

    smc_config_t cfg;
    /* Initialize with mock flash */
    smc_config_init(&cfg, mock_flash_read, mock_flash_write);
    assert(s_mock_flash_valid == true);

    /* Modify custom settings */
    cfg.pads[2][5].note = 77;
    cfg.encoders[1][2].cc = 99;
    cfg.strike_threshold = 85;
    cfg.pad_hit_color = 0xFF00FFu; /* Magenta */

    bool saved = smc_config_save(&cfg);
    assert(saved == true);

    /* Load into a fresh config instance */
    smc_config_t loaded_cfg;
    bool loaded = smc_config_load(&loaded_cfg);
    assert(loaded == true);
    assert(loaded_cfg.pads[2][5].note == 77);
    assert(loaded_cfg.encoders[1][2].cc == 99);
    assert(loaded_cfg.strike_threshold == 85);
    assert(loaded_cfg.pad_hit_color == 0xFF00FFu);

    /* Factory reset */
    bool reset = smc_config_factory_reset(&loaded_cfg);
    assert(reset == true);
    assert(loaded_cfg.pads[2][5].note == (uint8_t)(36 + 32 + 5)); /* 73 */
    assert(loaded_cfg.pad_hit_color == 0x00FF00u);

    printf("PASS: test_save_load_and_reset\n");
}

static void test_bank_switching(void) {
    printf("Running test_bank_switching...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);

    smc_config_set_pad_bank(0);
    assert(smc_config_get_pad_bank() == 0);
    assert(smc_config_get_active_pad(&cfg, 0)->note == 36);

    smc_config_set_pad_bank(1);
    assert(smc_config_get_pad_bank() == 1);
    assert(smc_config_get_active_pad(&cfg, 0)->note == 52);

    smc_config_set_encoder_bank(2);
    assert(smc_config_get_encoder_bank() == 2);
    assert(smc_config_get_active_encoder(&cfg, 0)->cc == 32);

    /* Bank index is clamped to 0..3 */
    smc_config_set_pad_bank(5); /* 5 & 3 == 1 */
    assert(smc_config_get_pad_bank() == 1);

    printf("PASS: test_bank_switching\n");
}

int main(void) {
    printf("=== Running SMC-Pad Config Engine Tests ===\n");
    test_defaults_and_validation();
    test_crc_integrity();
    test_save_load_and_reset();
    test_bank_switching();
    printf("=== All Config Engine Tests Passed Successfully! ===\n");
    return 0;
}
