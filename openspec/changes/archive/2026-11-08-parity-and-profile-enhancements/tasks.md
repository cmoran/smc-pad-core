## 1. Hardware UART 3.5mm MIDI Output & DAW Transport Mappings

- [x] 1.1 Add UART serial sink registration and concurrent 3-way dispatch to `smc_midi_dispatcher_t`
- [x] 1.2 Wire native `smcpad_serial_write` into `app_main.c` for 3.5mm TRS hardware MIDI output
- [x] 1.3 Implement DAW transport button handlers in `app_main.c` (Play = 0x5E, Stop = 0x5D, Record = 0x5F, Left = 0x2E, Right = 0x2F) on Channel 1

## 2. Continuous Pad Pressure & Aftertouch

- [x] 2.1 Update `pad_adc` to sample pressure in `PAD_PRESSED` state and generate `EVENT_PAD_PRESSURE`
- [x] 2.2 Add Channel Pressure (0xD0) emission to MIDI dispatcher pipeline
- [x] 2.3 Add host tests verifying pad pressure detection and aftertouch MIDI emission

## 3. External MIDI Clock Synchronization

- [x] 3.1 Implement incoming MIDI Realtime Clock parsing (0xF8, 0xFA, 0xFB, 0xFC) in `midi_dispatcher`
- [x] 3.2 Add clock tick hook and slave Note Repeat intervals to incoming 24 PPQN clock ticks when `Sync` is enabled
- [x] 3.3 Add unit tests verifying external clock sync and start/stop behavior

## 4. Relative CC Encoders & Speed Acceleration

- [x] 4.1 Add Relative CC modes (Signed Bit, 2's Complement, Binary Offset) to `smc_encoder_cfg_t` and `encoder.c`
- [x] 4.2 Add encoder delta formatting and verify in unit tests

## 5. Battery Voltage Monitoring & Low-Battery Alert

- [x] 5.1 Implement periodic `smcpad_battery_mv()` polling and low-battery detection in `app_main.c`
- [x] 5.2 Implement low-battery visual alert blinking Left (LED 3) and Right (LED 4) indicators

## 6. WebMIDI Configurator Named Profiles & Factory Presets

- [x] 6.1 Implement Profiles Manager UI in `tools/web-configurator.html` with persistent localStorage save/load
- [x] 6.2 Implement JSON profile export and import handlers
- [x] 6.3 Add built-in factory profiles ("Future Funk Setup", "Cumbia Setup", "Studio DAW Mackie", "MPC Finger Drumming")

## 7. Verification & Build Validation

- [x] 7.1 Run full host test suite `make test` and ensure all tests pass cleanly
- [x] 7.2 Compile target firmware `make firmware` to verify zero compiler warnings or linking errors
