## Context

The clean-slate SMC-Pad Core firmware has proven its performance advantages: zero stuck notes, drift rejection, Gray code quadrature scanning, and a complete note repeat engine. However, to achieve full feature parity with and surpass the original stock firmware, we must activate remaining hardware peripherals (3.5mm TRS MIDI OUT, battery sensing), implement DAW transport and Mackie Control emulation, support continuous pad aftertouch, sync note repeat to incoming MIDI clock, and provide named configuration profiles in the WebMIDI configurator.

## Goals / Non-Goals

**Goals:**
- Provide concurrent 3-way MIDI output: USB-MIDI, BLE-MIDI, and physical 3.5mm TRS hardware UART MIDI.
- Implement DAW transport controls (`PLAY`, `STOP`, `RECORD`) using industry-standard Mackie Control and MMC protocols.
- Add continuous pad pressure sensing for MIDI Channel Pressure (`0xD0`) and Polyphonic Aftertouch (`0xA0`).
- Implement external MIDI clock synchronization (`0xF8`, `0xFA`, `0xFB`, `0xFC`) to slave the Note Repeat engine to host DAWs.
- Implement battery voltage monitoring via `smcpad_battery_mv()` and low-battery alert via blinking Left/Right LEDs.
- Add relative CC encoder modes (Signed Bit, 2's Complement, Binary Offset) to eliminate parameter jumping in DAWs.
- Add persistent profile management in `tools/web-configurator.html` with export/import and musical presets (Future Funk, Cumbia, Studio DAW, Finger Drumming).

**Non-Goals:**
- Emulate the closed proprietary *MidiSuite* / *CubeSuite* SysEx handshake.
- Add on-chip multi-track sequencers or groovebox audio synthesis.

## Decisions

### 1. Concurrent 3.5mm TRS MIDI Dispatch
- **Decision**: Add a `send_uart_msg` transport hook to `smc_midi_dispatcher_t`.
- **Implementation**: The hardware serial pin is `PC5` (UART1 at 31,250 baud, 8-N-1). In `app_main.c`, `smcpad_serial_write()` transmits all raw MIDI frames directly to UART1 simultaneously with USB and BLE.
- **Alternative considered**: Exclusive transport selector switch. Rejected because simultaneous output enables hybrid rigs (e.g. USB to DAW, TRS to hardware synth).

### 2. DAW Transport & Mackie Control Mapping
- **Decision**: Physical transport buttons emit standard Mackie Control Note-On/Off messages on MIDI Channel 1:
  - `PLAY` (Btn 5): Note 94 (`0x5E`), velocity 127 / 0
  - `STOP` (Btn 6): Note 93 (`0x5D`), velocity 127 / 0 (Shift + Stop remains MIDI Panic)
  - `RECORD` (Btn 7): Note 95 (`0x5F`), velocity 127 / 0
  - `LEFT ARROW` (Btn 3 in DAW mode): Bank Left Note 46 (`0x2E`)
  - `RIGHT ARROW` (Btn 4 in DAW mode): Bank Right Note 47 (`0x2F`)
- **Alternative considered**: CC-only transport. Rejected because Mackie Control is supported out-of-the-box in all DAWs (Reaper, Ableton, Logic, Studio One, Cubase).

### 3. Continuous Pad Pressure (Aftertouch)
- **Decision**: While a pad is in `PAD_PRESSED` state and the reading exceeds the release threshold, compute the relative pressure `(sample - release_threshold) * 127 / (max_adc - release_threshold)`. Apply a deadband filter (>= 2 change) and dispatch Channel Pressure (`0xD0`).

### 4. External MIDI Clock Sync
- **Decision**: When receiving Realtime status byte `0xF8`, increment an internal 24 PPQN clock tick counter. In Note Repeat mode with Sync active (`Pad 15`), repeat intervals are gated by clock ticks (e.g. 1/16 = 6 ticks) rather than millisecond intervals. `0xFA` (Start) resets phase, `0xFC` (Stop) clears gate.

### 5. Battery Monitoring
- **Decision**: Poll `smcpad_battery_mv()` every 2000 ms. If measured battery voltage is between 1000 mV and 3500 mV and not on USB bus power, trigger low-battery alert state, alternating Left and Right LEDs at 2 Hz.

### 6. Configurator Profiles Architecture
- **Decision**: Store named profiles in browser `localStorage` under `smcpad_profiles_v1`. Provide JSON file download/upload for backup and sharing, with preloaded genre presets: "Future Funk Setup", "Cumbia Setup", "Studio DAW Mackie", "MPC Finger Drumming".

## Risks / Trade-offs

- **[Risk] High-frequency UART writes causing buffer contention**:
  → *Mitigation*: Raw MIDI messages are compact (1 to 3 bytes). `smcpad_serial_write` non-blocking ring buffer buffers bytes cleanly without blocking the scan loop.
- **[Risk] Aftertouch flooding MIDI stream**:
  → *Mitigation*: Strict 2-unit deadband filter and rate-limiting to a maximum of 50 Hz per sounding pad.
