/**
 * @file test_sysex.c
 * @brief Unit tests for SMC-Pad Open SysEx Configuration Protocol (0x7D)
 */

#include "sysex_protocol.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static uint8_t s_last_resp[SMC_SYSEX_MAX_LEN];
static size_t  s_last_resp_len = 0;

static void mock_sysex_tx(const uint8_t *data, size_t len) {
    assert(len <= sizeof(s_last_resp));
    memcpy(s_last_resp, data, len);
    s_last_resp_len = len;
}

static void test_get_dev_info(void) {
    printf("Running test_get_dev_info...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);

    smc_sysex_engine_t sysex;
    smc_sysex_init(&sysex, &cfg, mock_sysex_tx);

    s_last_resp_len = 0;
    const uint8_t query[] = { SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, SMC_CMD_GET_DEV_INFO, SMC_SYSEX_END };
    smc_sysex_feed_bytes(&sysex, query, sizeof(query));

    assert(s_last_resp_len == 11);
    assert(s_last_resp[0] == SMC_SYSEX_START);
    assert(s_last_resp[1] == SMC_SYSEX_MFG_ID);
    assert(s_last_resp[2] == SMC_SYSEX_DEV_ID);
    assert(s_last_resp[3] == SMC_CMD_RESP_DEV_INFO);
    assert(s_last_resp[7] == SMC_CONFIG_BANKS);
    assert(s_last_resp[8] == SMC_CONFIG_PADS);
    assert(s_last_resp[9] == SMC_CONFIG_ENCODERS);
    assert(s_last_resp[10] == SMC_SYSEX_END);

    printf("PASS: test_get_dev_info\n");
}

static void test_pad_map_get_set(void) {
    printf("Running test_pad_map_get_set...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);

    smc_sysex_engine_t sysex;
    smc_sysex_init(&sysex, &cfg, mock_sysex_tx);

    /* 1. Query Bank 1, Pad 4 */
    s_last_resp_len = 0;
    const uint8_t query[] = { SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, SMC_CMD_GET_PAD_MAP, 1, 4, SMC_SYSEX_END };
    smc_sysex_feed_bytes(&sysex, query, sizeof(query));

    assert(s_last_resp_len == 10);
    assert(s_last_resp[3] == SMC_CMD_RESP_PAD_MAP);
    assert(s_last_resp[4] == 1); /* Bank */
    assert(s_last_resp[5] == 4); /* Pad */
    assert(s_last_resp[6] == 56); /* Default Note 52 + 4 */

    /* 2. Set Bank 1, Pad 4 to Note 64, Ch 3, Mode 1 */
    s_last_resp_len = 0;
    const uint8_t set_cmd[] = { SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, SMC_CMD_SET_PAD_MAP, 1, 4, 64, 3, 1, SMC_SYSEX_END };
    smc_sysex_feed_bytes(&sysex, set_cmd, sizeof(set_cmd));

    assert(s_last_resp_len == 7);
    assert(s_last_resp[3] == SMC_CMD_RESP_ACK);
    assert(s_last_resp[4] == SMC_CMD_SET_PAD_MAP);
    assert(s_last_resp[5] == SMC_STATUS_OK);

    /* Verify in-memory config updated */
    assert(cfg.pads[1][4].note == 64);
    assert(cfg.pads[1][4].channel == 3);
    assert(cfg.pads[1][4].mode == 1);

    printf("PASS: test_pad_map_get_set\n");
}

static void test_encoder_map_get_set(void) {
    printf("Running test_encoder_map_get_set...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);

    smc_sysex_engine_t sysex;
    smc_sysex_init(&sysex, &cfg, mock_sysex_tx);

    /* Set Bank 2, Encoder 5 to CC 85, Ch 0, step 2, min 10, max 100 */
    s_last_resp_len = 0;
    const uint8_t set_cmd[] = { SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, SMC_CMD_SET_ENC_MAP, 2, 5, 85, 0, 2, 10, 100, SMC_SYSEX_END };
    smc_sysex_feed_bytes(&sysex, set_cmd, sizeof(set_cmd));

    assert(s_last_resp_len == 7);
    assert(s_last_resp[5] == SMC_STATUS_OK);
    assert(cfg.encoders[2][5].cc == 85);
    assert(cfg.encoders[2][5].step_size == 2);
    assert(cfg.encoders[2][5].min_val == 10);
    assert(cfg.encoders[2][5].max_val == 100);

    /* Query Bank 2, Encoder 5 */
    s_last_resp_len = 0;
    const uint8_t query[] = { SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, SMC_CMD_GET_ENC_MAP, 2, 5, SMC_SYSEX_END };
    smc_sysex_feed_bytes(&sysex, query, sizeof(query));

    assert(s_last_resp_len == 12);
    assert(s_last_resp[3] == SMC_CMD_RESP_ENC_MAP);
    assert(s_last_resp[6] == 85);
    assert(s_last_resp[8] == 2);
    assert(s_last_resp[9] == 10);
    assert(s_last_resp[10] == 100);

    printf("PASS: test_encoder_map_get_set\n");
}

static void test_calibration_and_rgb(void) {
    printf("Running test_calibration_and_rgb...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);

    smc_sysex_engine_t sysex;
    smc_sysex_init(&sysex, &cfg, mock_sysex_tx);

    /* Set calibration: strike 120 (0x78), release 60 (0x3C), dr 1200 (0x4B0), win 4, curve 2 */
    uint8_t st_lsb, st_msb, rt_lsb, rt_msb, dr_lsb, dr_msb;
    smc_pack_u16_to_7bit(120, &st_lsb, &st_msb);
    smc_pack_u16_to_7bit(60, &rt_lsb, &rt_msb);
    smc_pack_u16_to_7bit(1200, &dr_lsb, &dr_msb);

    const uint8_t set_cal[] = {
        SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, SMC_CMD_SET_CALIBRATION,
        st_lsb, st_msb, rt_lsb, rt_msb, dr_lsb, dr_msb, 4, 2, SMC_SYSEX_END
    };
    smc_sysex_feed_bytes(&sysex, set_cal, sizeof(set_cal));

    assert(s_last_resp[5] == SMC_STATUS_OK);
    assert(cfg.strike_threshold == 120);
    assert(cfg.release_threshold == 60);
    assert(cfg.dynamic_range == 1200);
    assert(cfg.scan_window_ms == 4);
    assert(cfg.velocity_curve == 2);

    /* Set RGB Config: idle 0x000030, hit 0xFF8000 (Orange), mode 1 (Host) */
    uint8_t idle[4], hit[4];
    smc_pack_color_to_7bit(0x000030u, idle);
    smc_pack_color_to_7bit(0xFF8000u, hit);

    const uint8_t set_rgb[] = {
        SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, SMC_CMD_SET_RGB_CFG,
        idle[0], idle[1], idle[2], idle[3],
        hit[0], hit[1], hit[2], hit[3],
        SMC_LED_MODE_HOST, SMC_SYSEX_END
    };
    smc_sysex_feed_bytes(&sysex, set_rgb, sizeof(set_rgb));

    assert(s_last_resp[5] == SMC_STATUS_OK);
    assert(cfg.pad_idle_color == 0x000030u);
    assert(cfg.pad_hit_color == 0xFF8000u);
    assert(cfg.led_mode == SMC_LED_MODE_HOST);

    printf("PASS: test_calibration_and_rgb\n");
}

static void test_malformed_and_interrupted_sysex(void) {
    printf("Running test_malformed_and_interrupted_sysex...\n");
    smc_config_t cfg;
    smc_config_set_defaults(&cfg);

    smc_sysex_engine_t sysex;
    smc_sysex_init(&sysex, &cfg, mock_sysex_tx);

    /* 1. Foreign manufacturer ID: 0x42 */
    s_last_resp_len = 0;
    const uint8_t foreign[] = { SMC_SYSEX_START, 0x42, SMC_SYSEX_DEV_ID, SMC_CMD_GET_DEV_INFO, SMC_SYSEX_END };
    smc_sysex_feed_bytes(&sysex, foreign, sizeof(foreign));
    assert(s_last_resp_len == 0); /* Must be ignored */

    /* 2. Interrupted SysEx by Real-Time Clock or Voice status */
    const uint8_t interrupted[] = { SMC_SYSEX_START, SMC_SYSEX_MFG_ID, SMC_SYSEX_DEV_ID, 0x90, /* Note-On interrupts */ 0x3C, 0x7F };
    smc_sysex_feed_bytes(&sysex, interrupted, sizeof(interrupted));
    assert(sysex.in_sysex == false);

    printf("PASS: test_malformed_and_interrupted_sysex\n");
}

int main(void) {
    printf("=== Running SMC-Pad Open SysEx Protocol Tests ===\n");
    test_get_dev_info();
    test_pad_map_get_set();
    test_encoder_map_get_set();
    test_calibration_and_rgb();
    test_malformed_and_interrupted_sysex();
    printf("=== All Open SysEx Protocol Tests Passed Successfully! ===\n");
    return 0;
}
