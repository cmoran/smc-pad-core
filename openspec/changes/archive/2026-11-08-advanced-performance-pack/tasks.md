## 1. Pad Chord Engine & Polyphonic Tracking

- [x] 1.1 Add chord definition structures to `smc_config.h` and multi-note sounding tracking to `smc_midi_dispatcher_t`
- [x] 1.2 Implement multi-note chord strike and release dispatching in `src/midi_dispatcher.c`
- [x] 1.3 Add unit tests verifying chord triggering and zero stuck notes on release

## 2. Scale & Musical Key Quantization

- [x] 2.1 Implement scale intervals table and scale degree mapper in `src/smc_config.c`
- [x] 2.2 Implement visual RGB scale lighting in `src/rgb_led.c` (Gold root, Cyan scale, dim out-of-scale)
- [x] 2.3 Add unit tests verifying scale note transposition and color rendering

## 3. Rotary Encoder Dynamic Acceleration

- [x] 3.1 Implement timing-based velocity ballistics multiplier in `src/encoder.c`
- [x] 3.2 Add unit tests verifying acceleration step scaling

## 4. Polyphonic Key Pressure (0xA0)

- [x] 4.1 Add Polyphonic Key Pressure mode toggle and emission logic in `src/midi_dispatcher.c`
- [x] 4.2 Add unit tests verifying Poly Aftertouch (0xA0) message generation

## 5. WebMIDI Oscilloscope, Calibration Wizard & Chord Editor

- [x] 5.1 Add Real-Time Canvas Oscilloscope to `tools/web-configurator.html`
- [x] 5.2 Implement automated 3-strike calibration assistant
- [x] 5.3 Add Chord Voicing Selector and Scale Quantizer UI controls

## 6. Verification & Build Validation

- [x] 6.1 Run full host test suite `make test` and ensure all tests pass cleanly
- [x] 6.2 Compile target firmware `make firmware` to verify zero compiler warnings or linking errors
