## 1. Virtual Banks and Configuration Engine

- [x] 1.1 Define configuration data structures and defaults in `include/smc_config.h`
- [x] 1.2 Implement configuration validation, factory reset, and memory storage logic in `src/smc_config.c`
- [x] 1.3 Add HAL hooks for non-volatile flash persistence (`syscfg` on target, simulated memory on host)
- [x] 1.4 Write unit tests for configuration engine, bank switching, and serialization in `test/test_config.c`

## 2. Open SysEx Protocol Engine (0x7D)

- [x] 2.1 Define open SysEx opcodes, 7-bit packing helpers, and frame handlers in `include/sysex_protocol.h`
- [x] 2.2 Implement SysEx parser and serializer in `src/sysex_protocol.c`
- [x] 2.3 Implement GET/SET handlers for Pad maps, Encoder maps, calibration, RGB colors, save, and factory reset
- [x] 2.4 Write unit tests for SysEx parser, byte stuffing, and command execution in `test/test_sysex.c`

## 3. BLE-MIDI Integration and Dual-Transport Dispatcher

- [x] 3.1 Define BLE-MIDI packet formatting, timestamp generation, and transport callbacks in `include/ble_midi.h`
- [x] 3.2 Implement BLE-MIDI packetizer and connection state machine in `src/ble_midi.c`
- [x] 3.3 Extend `smc_midi_dispatcher_t` to support dual-transport dispatching and incoming MIDI stream parsing
- [x] 3.4 Write unit tests for BLE-MIDI packetization and dual dispatch in `test/test_ble_midi.c`

## 4. Application Integration & Host DAW LED Control

- [x] 4.1 Update `src/app_main.c` to load configuration from flash on startup and initialize virtual banks
- [x] 4.2 Route physical bank buttons (`BTN_PAD_BANK`, `BTN_KNOB_BANK`) to switch active virtual banks
- [x] 4.3 Process incoming host MIDI messages for real-time Pad RGB LED control
- [x] 4.4 Extend end-to-end integration tests in `test/test_e2e_pipeline.c` to cover bank switching, SysEx, and BLE-MIDI
