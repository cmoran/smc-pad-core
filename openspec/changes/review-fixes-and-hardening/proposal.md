# Proposal: Code Review Fixes & Hardening

## Problem Statement
A comprehensive code review of the SMC-Pad clean-slate firmware and WebMIDI configurator identified several protocol mismatches, hardware state desynchronizations, MIDI/BLE-MIDI spec compliance bugs, and maintainability gaps:
1. WebMIDI Configurator sends the wrong SysEx opcode (`0x13` instead of `0x15`) for `SET_ENC_MAP`, drops `RESP_ENC_MAP`/`RESP_CALIBRATION`/`RESP_RGB_CFG` responses, hardcodes dynamic range in `uploadCalibration()`, and omits `scale` from saved profiles.
2. SysEx `GET_PAD_MAP` omits chord and pressure fields, `GET/SET_ENC_MAP` omits encoder mode, and SysEx config mutations do not refresh live pad LEDs.
3. Hardware encoder bank switching does not update `g_encoder_engine`, causing Bank 1+ to emit Bank 0's cached value; hardware encoder handling also bypasses `step_size`, `min_val`, `max_val`, and acceleration.
4. Per-pad `pressure_mode` is unreachable in `midi_dispatcher.c` while a pad is sounding, and pad Toggle mode (`mode == 1`) is unimplemented.
5. USB-MIDI RX ignores CIN `0x0F` (MIDI Clock) and 2-byte messages; MIDI RX parser drops status bytes that abort SysEx and mishandles System Common messages.
6. BLE-MIDI passes loop counter `g_system_ticks` instead of milliseconds, omits the `ts_low` byte before `0xF7` on SysEx TX, mishandles SysEx continuation packets on RX, and lacks memory barriers on `g_ble_rx_fifo`.
7. Note Repeat external clock sync ignores `g_midi_clock_running` and derives gate duration from internal BPM rather than external clock interval.
8. Minor hardware polish: low-battery LED cleanup on recovery, single `smcpad_rgb_show()` in host feedback loop, 8-sample baseline averaging in `pad_adc.c`, 4-bank hardware cycling, named button constants, and `smc_midi_send_raw()` deduplication.

## Scope
- Firmware core (`include/` & `src/`): `sysex_protocol`, `midi_dispatcher`, `ble_midi`, `pad_adc`, `app_main`.
- Web Configurator (`tools/web-configurator.html`).
- Host unit tests (`test/`).

## Non-Goals
- Modifying the prebuilt `bsp/smcpad_core.o` binary or flash partition layout.
- Adding new major musical features beyond completing and fixing existing advertised features.
