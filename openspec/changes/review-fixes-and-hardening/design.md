# Design: Code Review Fixes & Hardening

## 1. SysEx Protocol & Live Refresh
- `SMC_CMD_RESP_PAD_MAP` (`0x12`) emits 15 bytes: `[0xF0, 0x7D, 0x01, 0x12, bank, pad, note, ch, mode, chord_count, off0+64, off1+64, off2+64, pressure_mode, 0xF7]`.
- `SMC_CMD_RESP_ENC_MAP` (`0x16`) emits 13 bytes: adds `mode` before `0xF7`. `SET_ENC_MAP` (`0x15`) accepts optional 8th payload byte for `mode` (0..3).
- `smc_sysex_engine_t` gains an optional `smc_sysex_change_cb` callback (`smc_sysex_set_change_callback`) invoked after mutating commands so `app_main.c` calls `restore_bank_leds()`.

## 2. MIDI Dispatcher & Toggle Mode
- Add `smc_midi_send_raw(dispatcher, msg)` to broadcast to USB, BLE, and UART sinks.
- In `EVENT_PAD_PRESSURE`, always inspect `p_cfg->pressure_mode` when `dispatcher->config` is set (overriding default `dispatcher->pressure_mode` when non-zero).
- In `EVENT_PAD_STRIKE` / `EVENT_PAD_RELEASE`, check `p_cfg->mode == 1` (Toggle): striking an already-active pad emits Note-Off and clears `active_pads_mask`; physical release is ignored while in Toggle mode.
- In `smc_midi_rx_feed_byte`:
  - When `rx_in_sysex` is aborted by a non-`0xF7` status byte, clear `rx_in_sysex` and fall through to parse that status byte.
  - System Common (`0xF1..0xF6`) clears `rx_running_status` and sets appropriate `rx_expected_len`.

## 3. BLE-MIDI Spec Compliance
- `smc_ble_midi_send_sysex`: emit `ts_low` immediately before `0xF7` per Apple/MMA BLE-MIDI specification, spilling into a final `[header, ts_low, 0xF7]` packet if needed.
- `smc_ble_midi_parse_rx`: accept `len >= 2`; only skip `data[1]` if `(data[1] & 0x80u)` is set (timestamp present); skip inline timestamp bytes before any status byte including `0xF7` while never skipping `0xF0` or `0xF7` themselves.
- `app_main.c`: pass `app_get_millis()` to BLE-MIDI TX functions; add compiler barriers on `g_ble_rx_fifo`.

## 4. Hardware Loop, Pad ADC & Web Configurator
- `pad_adc.c`: average 8 samples in `smc_pad_calibrate_baseline()`; use `uint32_t` arithmetic when computing `strike_threshold` / `release_threshold`.
- `app_main.c`:
  - Add `smcpad_hw_btn_t` enum for `smcpad_core.o` event indices.
  - Handle all USB-MIDI RX CINs including `0x0F` (Real-Time Clock).
  - Sync `smc_encoder_set_bank` on Knob Bank press; cycle all 4 banks (`0..3`); apply `step_size`, `min_val`, `max_val` and acceleration in hardware encoder handling.
  - Gate Note Repeat external clock sync on `g_midi_clock_running` and compute gate duration from measured clock interval.
  - Clear arrow LEDs on low-battery recovery; batch `smcpad_rgb_show()` in `hal_host_note_feedback()`.
- `tools/web-configurator.html`: fix `SET_ENC_MAP` opcode (`0x15`), handle `0x16`/`0x22`/`0x32` RX responses, wire `dynamicRange` through `uploadCalibration()`, save/load `scale` in profiles.
