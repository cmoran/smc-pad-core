# Tasks: Pad RGB LED Driver & Runtime MIDI Integration

## Phase 1: Reference Quick-Fix Verification
- [x] 1.1 Verify reference `smcpad_pads.c` patch (`clk / LED_SPI_HZ - 1`) via clean build of `fw_area.bin`.
- [x] 1.2 Reflash target via `dfu-util` and verify pad RGB commands (`n a 00ff00`, `N` walk test).

## Phase 2: Core Clean-Slate RGB Driver (`smc-pad-core`)
- [x] 2.1 Implement `include/rgb_led.h` and `src/rgb_led.c` with static buffer encoding, daisy-chain index mapping, and non-blocking SPI DMA initiation.
- [x] 2.2 Create unit test `test/test_rgb_led.c` covering GRB bit encoding, color setting, and buffer serialization.
- [x] 2.3 Integrate `test/test_rgb_led.c` into `Makefile` and verify passing under host GCC.

## Phase 3: Runtime Application Loop Integration
- [x] 3.1 Update `src/app_main.c` to orchestrate 1 kHz scanning of `matrix`, `pad_adc`, and `encoder`.
- [x] 3.2 Implement event processing loop in `src/app_main.c` dispatching events from `smc_ring_buffer` to USB-MIDI.
- [x] 3.3 Connect pad strike/release state transitions to local RGB LED feedback.
- [x] 3.4 Implement Panic function (`Shift + Pause`) emitting CC 120, CC 123, and resetting active LEDs.

## Phase 4: Target Build & Hardware Verification
- [x] 4.1 Build target firmware image with clean-slate application loop.
- [x] 4.2 Flash image to physical SMC-Pad hardware via `dfu-util`.
- [x] 4.3 Verify USB-MIDI input in DAW/synth (e.g. Surge XT) on pad strikes, encoder turns, and button presses.
