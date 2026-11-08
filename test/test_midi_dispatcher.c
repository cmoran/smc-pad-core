/**
 * @file test_midi_dispatcher.c
 * @brief Unit tests for MIDI Dispatcher, Active Note Tracking, and Panic Handler
 */

#include <stdio.h>
#include <assert.h>
#include "midi_dispatcher.h"

#define MAX_SIM_MSGS 64
static smc_midi_raw_msg_t sim_msgs[MAX_SIM_MSGS];
static uint32_t sim_msg_count = 0;

static void sim_send_midi(const smc_midi_raw_msg_t *msg) {
    if (sim_msg_count < MAX_SIM_MSGS) {
        sim_msgs[sim_msg_count++] = *msg;
    }
}

static smc_midi_raw_msg_t sim_uart_msgs[8];
static uint32_t sim_uart_count = 0;
static void sim_uart_send(const smc_midi_raw_msg_t *msg) {
    if (sim_uart_count < 8) {
        sim_uart_msgs[sim_uart_count++] = *msg;
    }
}

static uint32_t clock_ticks = 0;
static uint8_t last_clock_cmd = 0;
static void sim_clock_cb(uint8_t msg_type, void *ctx) {
    (void)ctx;
    if (msg_type == 0xF8) clock_ticks++;
    last_clock_cmd = msg_type;
}

int main(void) {
    printf("Running MIDI Dispatcher unit tests...\n");

    smc_midi_dispatcher_t dispatcher;
    smc_midi_init(&dispatcher, sim_send_midi);

    smc_encoder_state_t encoder;
    smc_encoder_init(&encoder, 2);

    assert(dispatcher.active_pads_mask == 0);
    assert(!smc_midi_is_pad_active(&dispatcher, 0));

    /* 1. Test Pad Strike -> Note On */
    smc_event_t ev_strike = {
        .type = EVENT_PAD_STRIKE,
        .index = 0,
        .value = 110,
        .timestamp = 100
    };
    smc_midi_process_event(&dispatcher, &encoder, &ev_strike);

    assert(sim_msg_count == 1);
    assert(sim_msgs[0].bytes[0] == 0x90); /* Note On Ch 1 */
    assert(sim_msgs[0].bytes[1] == 36);   /* Default note for Pad 0 */
    assert(sim_msgs[0].bytes[2] == 110);  /* Velocity */
    assert(smc_midi_is_pad_active(&dispatcher, 0));
    assert(dispatcher.active_pads_mask == 0x0001);

    /* 2. Test Pad Release -> Note Off */
    smc_event_t ev_release = {
        .type = EVENT_PAD_RELEASE,
        .index = 0,
        .value = 0,
        .timestamp = 200
    };
    smc_midi_process_event(&dispatcher, &encoder, &ev_release);

    assert(sim_msg_count == 2);
    assert(sim_msgs[1].bytes[0] == 0x80); /* Note Off Ch 1 */
    assert(sim_msgs[1].bytes[1] == 36);
    assert(sim_msgs[1].bytes[2] == 0);
    assert(!smc_midi_is_pad_active(&dispatcher, 0));
    assert(dispatcher.active_pads_mask == 0);

    /* 3. Test Encoder Delta -> CC */
    smc_encoder_set_value(&encoder, 0, 3, 75);
    smc_event_t ev_enc = {
        .type = EVENT_ENCODER_DELTA,
        .index = 3,
        .value = +1,
        .timestamp = 300
    };
    smc_midi_process_event(&dispatcher, &encoder, &ev_enc);

    assert(sim_msg_count == 3);
    assert(sim_msgs[2].bytes[0] == 0xB0); /* CC Ch 1 */
    assert(sim_msgs[2].bytes[1] == encoder.configs[0][3].cc_number);
    assert(sim_msgs[2].bytes[2] == 75);

    /* 4. Test Panic Behavior (Shift + Pause) */
    /* Strike Pad 2 and Pad 7 so they are active notes */
    smc_event_t strike_p2 = { .type = EVENT_PAD_STRIKE, .index = 2, .value = 90, .timestamp = 400 };
    smc_event_t strike_p7 = { .type = EVENT_PAD_STRIKE, .index = 7, .value = 100, .timestamp = 401 };
    smc_midi_process_event(&dispatcher, &encoder, &strike_p2);
    smc_midi_process_event(&dispatcher, &encoder, &strike_p7);

    assert(smc_midi_is_pad_active(&dispatcher, 2));
    assert(smc_midi_is_pad_active(&dispatcher, 7));

    /* Reset msg count to inspect panic output cleanly */
    sim_msg_count = 0;

    /* Press Shift */
    smc_event_t ev_shift_down = { .type = EVENT_BUTTON_DOWN, .index = BTN_SHIFT, .value = 1, .timestamp = 500 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_shift_down);
    assert(dispatcher.shift_active);

    /* Press Pause while Shift is held */
    smc_event_t ev_pause_down = { .type = EVENT_BUTTON_DOWN, .index = BTN_PAUSE, .value = 1, .timestamp = 501 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_pause_down);

    /* Panic should have fired!
     * 16 channels * 2 CCs (CC 120 + CC 123) = 32 CC messages
     * + 2 Note-Off messages (for active pads 2 and 7) = 34 messages total!
     */
    assert(sim_msg_count >= 34);
    assert(!smc_midi_is_pad_active(&dispatcher, 2));
    assert(!smc_midi_is_pad_active(&dispatcher, 7));
    assert(dispatcher.active_pads_mask == 0);

    /* 5. Test Octave Transposition and Channel Override */
    smc_midi_set_octave_offset(&dispatcher, 12); /* +1 Octave */
    assert(smc_midi_get_octave_offset(&dispatcher) == 12);

    sim_msg_count = 0;
    smc_event_t ev_tr = { .type = EVENT_PAD_STRIKE, .index = 0, .value = 100, .timestamp = 600 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_tr);

    assert(sim_msg_count == 1);
    assert(sim_msgs[0].bytes[1] == 48); /* 36 + 12 = 48 */

    /* Channel override to Channel 5 (index 4) */
    smc_midi_set_channel_override(&dispatcher, 4);
    assert(smc_midi_get_channel_override(&dispatcher) == 4);

    sim_msg_count = 0;
    smc_midi_process_event(&dispatcher, &encoder, &ev_tr);
    assert(sim_msgs[0].bytes[0] == 0x94); /* Note-On Ch 5 (0x90 | 4) */

    /* 6. Test Zero Stuck Notes on Transposition Change While Pad Is Held */
    /* Release previous note first */
    smc_event_t ev_rel = { .type = EVENT_PAD_RELEASE, .index = 0, .value = 0, .timestamp = 650 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_rel);

    /* Strike Pad 3 with octave offset +12 (default note 39 -> 51) */
    smc_midi_set_channel_override(&dispatcher, -1);
    smc_midi_set_octave_offset(&dispatcher, 12);
    smc_midi_set_semitone_offset(&dispatcher, 2); /* 39 + 12 + 2 = 53 */
    sim_msg_count = 0;

    smc_event_t ev_p3_strike = { .type = EVENT_PAD_STRIKE, .index = 3, .value = 100, .timestamp = 700 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_p3_strike);
    assert(sim_msg_count == 1);
    assert(sim_msgs[0].bytes[0] == 0x90);
    assert(sim_msgs[0].bytes[1] == 53); /* Sounding note is 53 */
    assert(smc_midi_is_pad_active(&dispatcher, 3));

    /* While pad 3 is still held, transpose octave to -24 and semitone to -5! */
    smc_midi_set_octave_offset(&dispatcher, -24);
    smc_midi_set_semitone_offset(&dispatcher, -5);

    /* Now release Pad 3 */
    sim_msg_count = 0;
    smc_event_t ev_p3_release = { .type = EVENT_PAD_RELEASE, .index = 3, .value = 0, .timestamp = 750 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_p3_release);

    /* The emitted Note-Off MUST be Note 53, NOT (39 - 24 - 5 = 10)! */
    assert(sim_msg_count == 1);
    assert(sim_msgs[0].bytes[0] == 0x80);
    assert(sim_msgs[0].bytes[1] == 53); /* Exactly matches sounding note! Zero stuck notes! */
    assert(!smc_midi_is_pad_active(&dispatcher, 3));

    /* 7. Test UART 3.5mm Concurrent Dispatch */
    smc_midi_set_octave_offset(&dispatcher, 0);
    smc_midi_set_semitone_offset(&dispatcher, 0);
    smc_midi_set_uart_sink(&dispatcher, sim_uart_send);

    sim_msg_count = 0;
    sim_uart_count = 0;
    smc_event_t ev_uart_strike = { .type = EVENT_PAD_STRIKE, .index = 1, .value = 100, .timestamp = 800 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_uart_strike);
    assert(sim_msg_count == 1);
    assert(sim_uart_count == 1);
    assert(sim_uart_msgs[0].bytes[0] == 0x90);
    assert(sim_uart_msgs[0].bytes[1] == 37);

    /* 8. Test Continuous Pad Pressure (Channel Pressure 0xD0) */
    sim_msg_count = 0;
    smc_event_t ev_press = { .type = EVENT_PAD_PRESSURE, .index = 1, .value = 85, .timestamp = 820 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_press);
    assert(sim_msg_count == 1);
    assert(sim_msgs[0].bytes[0] == 0xD0); /* Channel Pressure Ch 1 */
    assert(sim_msgs[0].bytes[1] == 85);

    /* Release Pad 1 */
    smc_event_t ev_rel_p1 = { .type = EVENT_PAD_RELEASE, .index = 1, .value = 0, .timestamp = 850 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_rel_p1);

    /* 9. Test Relative CC Modes */
    encoder.configs[0][2].mode = ENC_MODE_RELATIVE_TWOS_COMPLEMENT;
    sim_msg_count = 0;
    smc_event_t ev_rel_cw = { .type = EVENT_ENCODER_DELTA, .index = 2, .value = +1, .timestamp = 900 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_rel_cw);
    assert(sim_msg_count == 1);
    assert(sim_msgs[0].bytes[0] == 0xB0);
    assert(sim_msgs[0].bytes[2] == 0x01); /* +1 in 2's comp */

    sim_msg_count = 0;
    smc_event_t ev_rel_ccw = { .type = EVENT_ENCODER_DELTA, .index = 2, .value = -1, .timestamp = 910 };
    smc_midi_process_event(&dispatcher, &encoder, &ev_rel_ccw);
    assert(sim_msg_count == 1);
    assert(sim_msgs[0].bytes[0] == 0xB0);
    assert(sim_msgs[0].bytes[2] == 0x7F); /* -1 in 2's comp */

    /* 10. Test MIDI Realtime Clock Callbacks */
    clock_ticks = 0;
    last_clock_cmd = 0;
    smc_midi_set_clock_callback(&dispatcher, sim_clock_cb, NULL);

    smc_midi_rx_feed_byte(&dispatcher, 0xFA); /* Start */
    assert(last_clock_cmd == 0xFA);
    smc_midi_rx_feed_byte(&dispatcher, 0xF8); /* Tick */
    assert(clock_ticks == 1);
    smc_midi_rx_feed_byte(&dispatcher, 0xF8); /* Tick */
    assert(clock_ticks == 2);
    smc_midi_rx_feed_byte(&dispatcher, 0xFC); /* Stop */
    assert(last_clock_cmd == 0xFC);

    printf("MIDI Dispatcher unit tests passed successfully!\n");
    return 0;
}
