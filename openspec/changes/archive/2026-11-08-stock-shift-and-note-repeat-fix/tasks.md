## 1. Sounding Note Tracking & Stuck Note Prevention

- [x] 1.1 Add sounding_notes and sounding_channels arrays and semitone_offset to smc_midi_dispatcher_t in include/midi_dispatcher.h
- [x] 1.2 Update smc_midi_dispatcher_init to initialize sounding_notes to 0xFF in src/midi_dispatcher.c
- [x] 1.3 Update EVENT_PAD_STRIKE in src/midi_dispatcher.c to record emitted note and channel
- [x] 1.4 Update EVENT_PAD_RELEASE in src/midi_dispatcher.c to emit Note-Off using stored sounding note and channel
- [x] 1.5 Add unit tests in test/test_midi_dispatcher.c verifying transpose while pad is held emits correct Note-Off

## 2. Wall-Clock Note Repeat Engine

- [x] 2.1 Implement wall-clock millisecond timing with gated duty cycle for Note Repeat in src/app_main.c
- [x] 2.2 Define rate division lookups (1/4 to 1/32t) in src/app_main.c
- [x] 2.3 Implement Repeat Rate configuration mode triggered via Shift + Note Repeat on Pads 1-8

## 3. Stock Shift Layers & Front Panel Controls

- [x] 3.1 Implement Shift + Pads 1-8 for bank/preset switching with RGB feedback
- [x] 3.2 Implement Shift + Pads 9-12 for velocity curve selection
- [x] 3.3 Implement Shift + Pads 13-14 for semitone transposition
- [x] 3.4 Implement Shift + Pads 15-16 for octave transposition
- [x] 3.5 Implement Shift + Pad 15 and 16 dual-press for transpose reset
- [x] 3.6 Suppress standard Note-On strikes when Shift is held for function pad presses

## 4. Verification and Target Build

- [x] 4.1 Run host test suite (make test) and verify all test suites pass
- [x] 4.2 Compile target firmware (make firmware) and verify build/app.bin output
