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

    printf("MIDI Dispatcher unit tests passed successfully!\n");
    return 0;
}
