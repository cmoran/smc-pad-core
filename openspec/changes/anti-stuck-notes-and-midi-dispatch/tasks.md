## 1. Dynamic Baseline Tracking in Pad ADC

- [x] 1.1 Add baseline field and adaptation filter to `pad_adc.h` and `pad_adc.c`
- [x] 1.2 Implement relative strike thresholding (`baseline + delta_strike`) and relative release hysteresis (`baseline + delta_release`)
- [x] 1.3 Write unit tests in `test/test_pad_adc.c` verifying baseline adaptation in idle and freeze during strikes

## 2. Dedicated MIDI Dispatcher & Active Note Tracking

- [x] 2.1 Implement `include/midi_dispatcher.h` and `src/midi_dispatcher.c` with 3-byte MIDI message output
- [x] 2.2 Implement active note tracking bitmask ensuring 1:1 Note-On / Note-Off correspondence
- [x] 2.3 Implement Panic handler emitting CC 120, CC 123, and releasing all tracked active notes
- [x] 2.4 Wire `Shift + Pause` shortcut in matrix/dispatcher to trigger panic

## 3. Verification & Host Test Harness

- [x] 3.1 Write unit tests in `test/test_midi_dispatcher.c` verifying message generation, active note tracking, and panic behavior
- [x] 3.2 Add sensor mechanical drift simulation in `test/test_pad_adc.c` proving that elevated resting voltages after hard strikes never stick notes
- [x] 3.3 Update Makefile and run complete test suite
