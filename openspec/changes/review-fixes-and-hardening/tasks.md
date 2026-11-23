# Tasks: Code Review Fixes & Hardening

- [x] 1. Extend SysEx protocol responses (`GET_PAD_MAP`, `GET/SET_ENC_MAP`) and add config-change callback (`include/sysex_protocol.h`, `src/sysex_protocol.c`)
- [x] 2. Fix MIDI dispatcher per-pad pressure mode, pad toggle mode, RX parser edge cases, and add `smc_midi_send_raw()` (`include/midi_dispatcher.h`, `src/midi_dispatcher.c`)
- [x] 3. Fix BLE-MIDI SysEx TX `0xF7` timestamp framing and RX continuation packet parsing (`src/ble_midi.c`)
- [x] 4. Add 8-sample baseline averaging and overflow-safe threshold math in `src/pad_adc.c`
- [x] 5. Harden `src/app_main.c` (USB-MIDI CIN `0x0F`, BLE timestamps & FIFO barriers, encoder bank/step/clamp sync, 4-bank cycling, Note Repeat clock sync, SysEx LED refresh, low-battery LED clear, `smcpad_hw_btn_t` enum, `smc_midi_send_raw` deduplication)
- [x] 6. Fix WebMIDI Configurator (`tools/web-configurator.html`): `0x15` opcode, RX handlers (`0x16`/`0x22`/`0x32`), calibration `dynamicRange`, and profile `scale` persistence
- [x] 7. Update and expand host unit tests (`test/test_sysex.c`, `test/test_midi_dispatcher.c`, `test/test_ble_midi.c`, `test/test_pad_adc.c`)
- [x] 8. Run `make test`, `make firmware`, and `openspec validate` to verify everything passes
