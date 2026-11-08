## 1. Bank RGB Palette Engine

- [x] 1.1 Define per-bank color palettes and matrix refresh functions in `include/rgb_led.h` and `src/rgb_led.c`
- [x] 1.2 Update `src/app_main.c` to refresh all pad RGB LEDs with the active bank palette when cycling banks
- [x] 1.3 Add unit test for multi-bank color themes in `test/test_rgb_led.c`

## 2. Navigation and Octave Transposition

- [x] 2.1 Implement octave transposition logic (-48..+48 semitones) and clamping in `src/midi_dispatcher.c`
- [x] 2.2 Wire Arrow buttons (Btn 3 Left, Btn 4 Right) to octave shifts and channel modifiers with LED feedback
- [x] 2.3 Add unit tests for octave transposition in `test/test_midi_dispatcher.c`

## 3. Note Repeat and Shift Layer Shortcuts

- [x] 3.1 Implement Note Repeat rhythmic pulse state machine and Button 9 LED toggle in `src/app_main.c`
- [x] 3.2 Implement Shift combinations (`Shift + Pad 1..4`, `Shift + Pad 5..8`, `Shift + Left/Right`, `Shift + Pause`)
- [x] 3.3 Verify full pipeline in `test/test_e2e_pipeline.c` and compile target firmware binary `build/app.bin`
