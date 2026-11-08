/**
 * @file sysex_protocol.c
 * @brief Open SysEx Configuration Protocol Implementation
 */

#include "sysex_protocol.h"
#include <string.h>

static void send_ack(smc_sysex_engine_t *engine, uint8_t orig_cmd, uint8_t status) {
    if (!engine || !engine->tx_fn) return;
    uint8_t resp[7] = {
        SMC_SYSEX_START,
        SMC_SYSEX_MFG_ID,
        SMC_SYSEX_DEV_ID,
        SMC_CMD_RESP_ACK,
        orig_cmd,
        status,
        SMC_SYSEX_END
    };
    engine->tx_fn(resp, sizeof(resp));
}

static void handle_get_dev_info(smc_sysex_engine_t *engine) {
    if (!engine->tx_fn) return;
    uint8_t resp[11] = {
        SMC_SYSEX_START,
        SMC_SYSEX_MFG_ID,
        SMC_SYSEX_DEV_ID,
        SMC_CMD_RESP_DEV_INFO,
        1, /* Major version */
        0, /* Minor version */
        0, /* Patch version */
        SMC_CONFIG_BANKS,
        SMC_CONFIG_PADS,
        SMC_CONFIG_ENCODERS,
        SMC_SYSEX_END
    };
    engine->tx_fn(resp, sizeof(resp));
}

static void handle_get_pad_map(smc_sysex_engine_t *engine, const uint8_t *payload, uint8_t len) {
    if (len < 2 || !engine->cfg || !engine->tx_fn) {
        send_ack(engine, SMC_CMD_GET_PAD_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }
    uint8_t bank = payload[0];
    uint8_t pad = payload[1];
    if (bank >= SMC_CONFIG_BANKS || pad >= SMC_CONFIG_PADS) {
        send_ack(engine, SMC_CMD_GET_PAD_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }

    const smc_pad_cfg_t *p = &engine->cfg->pads[bank][pad];
    uint8_t resp[10] = {
        SMC_SYSEX_START,
        SMC_SYSEX_MFG_ID,
        SMC_SYSEX_DEV_ID,
        SMC_CMD_RESP_PAD_MAP,
        bank,
        pad,
        p->note & 0x7Fu,
        p->channel & 0x0Fu,
        p->mode & 0x7Fu,
        SMC_SYSEX_END
    };
    engine->tx_fn(resp, sizeof(resp));
}

static void handle_set_pad_map(smc_sysex_engine_t *engine, const uint8_t *payload, uint8_t len) {
    if (len < 5 || !engine->cfg) {
        send_ack(engine, SMC_CMD_SET_PAD_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }
    uint8_t bank = payload[0];
    uint8_t pad = payload[1];
    uint8_t note = payload[2];
    uint8_t channel = payload[3];
    uint8_t mode = payload[4];

    if (bank >= SMC_CONFIG_BANKS || pad >= SMC_CONFIG_PADS || note > 127 || channel > 15) {
        send_ack(engine, SMC_CMD_SET_PAD_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }

    engine->cfg->pads[bank][pad].note = note;
    engine->cfg->pads[bank][pad].channel = channel;
    engine->cfg->pads[bank][pad].mode = mode;

    send_ack(engine, SMC_CMD_SET_PAD_MAP, SMC_STATUS_OK);
}

static void handle_get_enc_map(smc_sysex_engine_t *engine, const uint8_t *payload, uint8_t len) {
    if (len < 2 || !engine->cfg || !engine->tx_fn) {
        send_ack(engine, SMC_CMD_GET_ENC_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }
    uint8_t bank = payload[0];
    uint8_t enc = payload[1];
    if (bank >= SMC_CONFIG_BANKS || enc >= SMC_CONFIG_ENCODERS) {
        send_ack(engine, SMC_CMD_GET_ENC_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }

    const smc_encoder_cfg_t *e = &engine->cfg->encoders[bank][enc];
    uint8_t resp[12] = {
        SMC_SYSEX_START,
        SMC_SYSEX_MFG_ID,
        SMC_SYSEX_DEV_ID,
        SMC_CMD_RESP_ENC_MAP,
        bank,
        enc,
        e->cc & 0x7Fu,
        e->channel & 0x0Fu,
        e->step_size & 0x7Fu,
        e->min_val & 0x7Fu,
        e->max_val & 0x7Fu,
        SMC_SYSEX_END
    };
    engine->tx_fn(resp, sizeof(resp));
}

static void handle_set_enc_map(smc_sysex_engine_t *engine, const uint8_t *payload, uint8_t len) {
    if (len < 7 || !engine->cfg) {
        send_ack(engine, SMC_CMD_SET_ENC_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }
    uint8_t bank = payload[0];
    uint8_t enc = payload[1];
    uint8_t cc = payload[2];
    uint8_t channel = payload[3];
    uint8_t step_size = payload[4];
    uint8_t min_val = payload[5];
    uint8_t max_val = payload[6];

    if (bank >= SMC_CONFIG_BANKS || enc >= SMC_CONFIG_ENCODERS || cc > 127 ||
        channel > 15 || min_val > max_val || max_val > 127) {
        send_ack(engine, SMC_CMD_SET_ENC_MAP, SMC_STATUS_ERR_PARAM);
        return;
    }

    engine->cfg->encoders[bank][enc].cc = cc;
    engine->cfg->encoders[bank][enc].channel = channel;
    engine->cfg->encoders[bank][enc].step_size = step_size ? step_size : 1;
    engine->cfg->encoders[bank][enc].min_val = min_val;
    engine->cfg->encoders[bank][enc].max_val = max_val;

    send_ack(engine, SMC_CMD_SET_ENC_MAP, SMC_STATUS_OK);
}

static void handle_get_calibration(smc_sysex_engine_t *engine) {
    if (!engine->cfg || !engine->tx_fn) return;

    uint8_t st_lsb, st_msb;
    uint8_t rt_lsb, rt_msb;
    uint8_t dr_lsb, dr_msb;
    smc_pack_u16_to_7bit(engine->cfg->strike_threshold, &st_lsb, &st_msb);
    smc_pack_u16_to_7bit(engine->cfg->release_threshold, &rt_lsb, &rt_msb);
    smc_pack_u16_to_7bit(engine->cfg->dynamic_range, &dr_lsb, &dr_msb);

    uint8_t resp[13] = {
        SMC_SYSEX_START,
        SMC_SYSEX_MFG_ID,
        SMC_SYSEX_DEV_ID,
        SMC_CMD_RESP_CALIBRATION,
        st_lsb, st_msb,
        rt_lsb, rt_msb,
        dr_lsb, dr_msb,
        engine->cfg->scan_window_ms & 0x7Fu,
        engine->cfg->velocity_curve & 0x7Fu,
        SMC_SYSEX_END
    };
    engine->tx_fn(resp, sizeof(resp));
}

static void handle_set_calibration(smc_sysex_engine_t *engine, const uint8_t *payload, uint8_t len) {
    if (len < 8 || !engine->cfg) {
        send_ack(engine, SMC_CMD_SET_CALIBRATION, SMC_STATUS_ERR_PARAM);
        return;
    }

    uint16_t st = smc_unpack_7bit_to_u16(payload[0], payload[1]);
    uint16_t rt = smc_unpack_7bit_to_u16(payload[2], payload[3]);
    uint16_t dr = smc_unpack_7bit_to_u16(payload[4], payload[5]);
    uint8_t win = payload[6];
    uint8_t cur = payload[7];

    if (st == 0 || rt >= st || dr == 0 || win == 0 || win > 20 || cur > VELOCITY_CURVE_FIXED) {
        send_ack(engine, SMC_CMD_SET_CALIBRATION, SMC_STATUS_ERR_PARAM);
        return;
    }

    engine->cfg->strike_threshold = st;
    engine->cfg->release_threshold = rt;
    engine->cfg->dynamic_range = dr;
    engine->cfg->scan_window_ms = win;
    engine->cfg->velocity_curve = cur;

    send_ack(engine, SMC_CMD_SET_CALIBRATION, SMC_STATUS_OK);
}

static void handle_get_rgb_cfg(smc_sysex_engine_t *engine) {
    if (!engine->cfg || !engine->tx_fn) return;

    uint8_t idle[4], hit[4];
    smc_pack_color_to_7bit(engine->cfg->pad_idle_color, idle);
    smc_pack_color_to_7bit(engine->cfg->pad_hit_color, hit);

    uint8_t resp[14] = {
        SMC_SYSEX_START,
        SMC_SYSEX_MFG_ID,
        SMC_SYSEX_DEV_ID,
        SMC_CMD_RESP_RGB_CFG,
        idle[0], idle[1], idle[2], idle[3],
        hit[0], hit[1], hit[2], hit[3],
        engine->cfg->led_mode & 0x7Fu,
        SMC_SYSEX_END
    };
    engine->tx_fn(resp, sizeof(resp));
}

static void handle_set_rgb_cfg(smc_sysex_engine_t *engine, const uint8_t *payload, uint8_t len) {
    if (len < 9 || !engine->cfg) {
        send_ack(engine, SMC_CMD_SET_RGB_CFG, SMC_STATUS_ERR_PARAM);
        return;
    }

    uint32_t idle = smc_unpack_7bit_to_color(&payload[0]);
    uint32_t hit  = smc_unpack_7bit_to_color(&payload[4]);
    uint8_t mode  = payload[8];

    if (mode > SMC_LED_MODE_HOST) {
        send_ack(engine, SMC_CMD_SET_RGB_CFG, SMC_STATUS_ERR_PARAM);
        return;
    }

    engine->cfg->pad_idle_color = idle;
    engine->cfg->pad_hit_color  = hit;
    engine->cfg->led_mode       = mode;

    send_ack(engine, SMC_CMD_SET_RGB_CFG, SMC_STATUS_OK);
}

static void process_sysex_packet(smc_sysex_engine_t *engine) {
    if (!engine || engine->rx_idx < 4) return;

    /* Verify framing: 0xF0, 0x7D, 0x01 ... 0xF7 */
    if (engine->rx_buf[0] != SMC_SYSEX_START ||
        engine->rx_buf[1] != SMC_SYSEX_MFG_ID ||
        engine->rx_buf[2] != SMC_SYSEX_DEV_ID) {
        return;
    }

    uint8_t cmd = engine->rx_buf[3];
    const uint8_t *payload = &engine->rx_buf[4];
    uint8_t payload_len = (uint8_t)(engine->rx_idx - 5); /* exclude 0xF0, 0x7D, 0x01, cmd and 0xF7 */

    switch (cmd) {
    case SMC_CMD_GET_DEV_INFO:
        handle_get_dev_info(engine);
        break;
    case SMC_CMD_GET_PAD_MAP:
        handle_get_pad_map(engine, payload, payload_len);
        break;
    case SMC_CMD_SET_PAD_MAP:
        handle_set_pad_map(engine, payload, payload_len);
        break;
    case SMC_CMD_GET_ENC_MAP:
        handle_get_enc_map(engine, payload, payload_len);
        break;
    case SMC_CMD_SET_ENC_MAP:
        handle_set_enc_map(engine, payload, payload_len);
        break;
    case SMC_CMD_GET_CALIBRATION:
        handle_get_calibration(engine);
        break;
    case SMC_CMD_SET_CALIBRATION:
        handle_set_calibration(engine, payload, payload_len);
        break;
    case SMC_CMD_GET_RGB_CFG:
        handle_get_rgb_cfg(engine);
        break;
    case SMC_CMD_SET_RGB_CFG:
        handle_set_rgb_cfg(engine, payload, payload_len);
        break;
    case SMC_CMD_SAVE_TO_FLASH: {
        bool ok = smc_config_save(engine->cfg);
        send_ack(engine, SMC_CMD_SAVE_TO_FLASH, ok ? SMC_STATUS_OK : SMC_STATUS_ERR_FLASH);
        break;
    }
    case SMC_CMD_FACTORY_RESET: {
        bool ok = smc_config_factory_reset(engine->cfg);
        send_ack(engine, SMC_CMD_FACTORY_RESET, ok ? SMC_STATUS_OK : SMC_STATUS_ERR_FLASH);
        break;
    }
    default:
        send_ack(engine, cmd, SMC_STATUS_ERR_CMD);
        break;
    }
}

void smc_sysex_init(smc_sysex_engine_t *engine, smc_config_t *cfg, smc_sysex_tx_fn tx_fn) {
    if (!engine) return;
    engine->cfg = cfg;
    engine->tx_fn = tx_fn;
    engine->rx_idx = 0;
    engine->in_sysex = false;
}

void smc_sysex_feed_byte(smc_sysex_engine_t *engine, uint8_t byte) {
    if (!engine) return;

    if (byte == SMC_SYSEX_START) {
        engine->in_sysex = true;
        engine->rx_idx = 0;
        engine->rx_buf[engine->rx_idx++] = byte;
        return;
    }

    if (!engine->in_sysex) {
        return;
    }

    /* Any status byte other than 0xF7 terminates/aborts SysEx */
    if ((byte & 0x80u) && (byte != SMC_SYSEX_END)) {
        engine->in_sysex = false;
        engine->rx_idx = 0;
        return;
    }

    if (engine->rx_idx < SMC_SYSEX_MAX_LEN) {
        engine->rx_buf[engine->rx_idx++] = byte;
    } else {
        /* Buffer overflow - abort */
        engine->in_sysex = false;
        engine->rx_idx = 0;
        return;
    }

    if (byte == SMC_SYSEX_END) {
        engine->in_sysex = false;
        process_sysex_packet(engine);
        engine->rx_idx = 0;
    }
}

void smc_sysex_feed_bytes(smc_sysex_engine_t *engine, const uint8_t *data, size_t len) {
    if (!engine || !data) return;
    for (size_t i = 0; i < len; i++) {
        smc_sysex_feed_byte(engine, data[i]);
    }
}
