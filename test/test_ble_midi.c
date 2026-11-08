/**
 * @file test_ble_midi.c
 * @brief Unit tests for BLE-MIDI Protocol & Dual Transport Dispatcher
 */

#include "ble_midi.h"
#include "midi_dispatcher.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static uint8_t s_last_ble_pkt[SMC_BLE_MIDI_MAX_PACKET_LEN];
static uint16_t s_last_ble_len = 0;
static uint32_t s_ble_pkt_count = 0;

static bool mock_ble_send(const uint8_t *data, uint16_t len) {
    assert(len <= sizeof(s_last_ble_pkt));
    memcpy(s_last_ble_pkt, data, len);
    s_last_ble_len = len;
    s_ble_pkt_count++;
    return true;
}

static void test_ble_connection_and_subscription(void) {
    printf("Running test_ble_connection_and_subscription...\n");
    smc_ble_midi_t ble;
    smc_ble_midi_init(&ble, mock_ble_send);

    uint8_t note_on[] = { 0x90, 60, 100 };

    /* Disconnected: should fail */
    assert(smc_ble_midi_send_message(&ble, note_on, 3, 100) == false);

    /* Connected but not subscribed: should fail */
    smc_ble_midi_set_connected(&ble, true);
    assert(smc_ble_midi_send_message(&ble, note_on, 3, 100) == false);

    /* Connected and subscribed: must succeed */
    smc_ble_midi_set_subscribed(&ble, true);
    assert(smc_ble_midi_send_message(&ble, note_on, 3, 100) == true);

    printf("PASS: test_ble_connection_and_subscription\n");
}

static void test_ble_packet_timestamp_formatting(void) {
    printf("Running test_ble_packet_timestamp_formatting...\n");
    smc_ble_midi_t ble;
    smc_ble_midi_init(&ble, mock_ble_send);
    smc_ble_midi_set_connected(&ble, true);
    smc_ble_midi_set_subscribed(&ble, true);

    /* Timestamp = 1234 ms (0x04D2) */
    /* ts_high = (1234 >> 7) & 0x3F = 9 (0x09) -> Header = 0x89 */
    /* ts_low  = 1234 & 0x7F = 82 (0x52) -> Timestamp byte = 0xD2 */
    uint8_t cc_msg[] = { 0xB0, 16, 64 };
    s_last_ble_len = 0;
    bool ok = smc_ble_midi_send_message(&ble, cc_msg, 3, 1234);
    assert(ok == true);
    assert(s_last_ble_len == 5);
    assert(s_last_ble_pkt[0] == 0x89u);
    assert(s_last_ble_pkt[1] == 0xD2u);
    assert(s_last_ble_pkt[2] == 0xB0u);
    assert(s_last_ble_pkt[3] == 16);
    assert(s_last_ble_pkt[4] == 64);

    printf("PASS: test_ble_packet_timestamp_formatting\n");
}

static void test_ble_sysex_chunking(void) {
    printf("Running test_ble_sysex_chunking...\n");
    smc_ble_midi_t ble;
    smc_ble_midi_init(&ble, mock_ble_send);
    smc_ble_midi_set_connected(&ble, true);
    smc_ble_midi_set_subscribed(&ble, true);
    smc_ble_midi_set_mtu(&ble, 23); /* max payload 20 */

    uint8_t sysex[35];
    sysex[0] = 0xF0;
    for (int i = 1; i < 34; i++) sysex[i] = 0x42;
    sysex[34] = 0xF7;

    s_ble_pkt_count = 0;
    bool ok = smc_ble_midi_send_sysex(&ble, sysex, sizeof(sysex), 500);
    assert(ok == true);
    assert(s_ble_pkt_count == 2); /* 35 bytes fits in 2 packets of <=20 bytes */
    assert(s_last_ble_pkt[s_last_ble_len - 1] == 0xF7u);

    printf("PASS: test_ble_sysex_chunking\n");
}

static smc_midi_raw_msg_t s_last_usb_msg;
static smc_midi_raw_msg_t s_last_ble_raw;

static void mock_usb_send(const smc_midi_raw_msg_t *msg) {
    s_last_usb_msg = *msg;
}

static void mock_ble_raw_send(const smc_midi_raw_msg_t *msg) {
    s_last_ble_raw = *msg;
}

static void test_dual_transport_and_banks(void) {
    printf("Running test_dual_transport_and_banks...\n");
    smc_midi_dispatcher_t disp;
    smc_midi_init(&disp, mock_usb_send);
    smc_midi_set_ble_sink(&disp, mock_ble_raw_send);

    smc_config_t cfg;
    smc_config_set_defaults(&cfg);
    smc_midi_set_config(&disp, &cfg);

    memset(&s_last_usb_msg, 0, sizeof(s_last_usb_msg));
    memset(&s_last_ble_raw, 0, sizeof(s_last_ble_raw));

    /* Bank 0: Pad 2 strike -> Note 38 */
    smc_config_set_pad_bank(0);
    smc_event_t ev_hit = { .type = EVENT_PAD_STRIKE, .index = 2, .value = 110 };
    smc_midi_process_event(&disp, NULL, &ev_hit);

    /* Verify USB received Note-On */
    assert(s_last_usb_msg.len == 3);
    assert(s_last_usb_msg.bytes[0] == 0x90);
    assert(s_last_usb_msg.bytes[1] == 38);
    assert(s_last_usb_msg.bytes[2] == 110);

    /* Verify BLE received identical Note-On */
    assert(s_last_ble_raw.len == 3);
    assert(s_last_ble_raw.bytes[0] == 0x90);
    assert(s_last_ble_raw.bytes[1] == 38);
    assert(s_last_ble_raw.bytes[2] == 110);

    /* Switch to Bank 1: Pad 2 strike -> Note 54 */
    smc_config_set_pad_bank(1);
    smc_event_t ev_hit_b1 = { .type = EVENT_PAD_STRIKE, .index = 2, .value = 95 };
    smc_midi_process_event(&disp, NULL, &ev_hit_b1);

    assert(s_last_usb_msg.bytes[1] == 54);
    assert(s_last_ble_raw.bytes[1] == 54);

    printf("PASS: test_dual_transport_and_banks\n");
}

static uint8_t s_rx_note = 0;
static uint8_t s_rx_vel = 0;
static void on_host_note(uint8_t ch, uint8_t note, uint8_t vel, void *ctx) {
    (void)ch;
    (void)ctx;
    s_rx_note = note;
    s_rx_vel = vel;
}

static void test_rx_stream_decoding(void) {
    printf("Running test_rx_stream_decoding...\n");
    smc_midi_dispatcher_t disp;
    smc_midi_init(&disp, mock_usb_send);
    smc_midi_set_host_callbacks(&disp, on_host_note, NULL, NULL);

    s_rx_note = 0;
    s_rx_vel = 0;

    /* Feed standard Note-On 0x90, 0x24 (Note 36), 0x7F (127) */
    uint8_t midi_in[] = { 0x90, 36, 127 };
    smc_midi_rx_feed_bytes(&disp, midi_in, sizeof(midi_in));

    assert(s_rx_note == 36);
    assert(s_rx_vel == 127);

    printf("PASS: test_rx_stream_decoding\n");
}

int main(void) {
    printf("=== Running SMC-Pad BLE-MIDI & Dual Transport Tests ===\n");
    test_ble_connection_and_subscription();
    test_ble_packet_timestamp_formatting();
    test_ble_sysex_chunking();
    test_dual_transport_and_banks();
    test_rx_stream_decoding();
    printf("=== All BLE-MIDI & Dual Transport Tests Passed Successfully! ===\n");
    return 0;
}
