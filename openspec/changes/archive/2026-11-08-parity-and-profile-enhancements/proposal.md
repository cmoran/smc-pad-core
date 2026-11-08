## Why

While SMC-Pad Core has successfully replaced the stock firmware's core architecture with an ultra-responsive, zero-stuck-note engine, several hardware peripherals and operational modes of the original hardware remain unused (3.5mm TRS hardware MIDI OUT, DAW transport controls, Mackie track banking, continuous pad aftertouch, external MIDI clock sync, and low-battery alert). Additionally, users need the ability to quickly store, switch, and export musical "profiles" (e.g., Future Funk, Cumbia, MPC finger drumming, DAW mixing) in the WebMIDI configurator, alongside advanced modern controller capabilities like relative CC encoder modes.

## What Changes

- **3.5mm TRS Hardware MIDI OUT**: Wire UART1 TX (`PC5` at 31250 baud) into the MIDI dispatcher pipeline so all MIDI events are concurrently broadcast to hardware synths via the 3.5mm jack, USB-MIDI, and BLE-MIDI.
- **DAW Transport & Track Banking**: Map `PLAY`, `STOP`, and `RECORD` physical buttons to standard MMC and Mackie Control transport messages; support DAW track banking with Left/Right buttons.
- **Continuous Pad Pressure / Aftertouch**: Sample continuous pad pressure after strike Note-On to emit MIDI Channel Pressure (0xD0).
- **External MIDI Clock Sync**: Parse incoming Realtime MIDI Clock (`0xF8`, `0xFA`, `0xFB`, `0xFC`) to slave the Note Repeat engine tempo to host DAWs when `Pad 15 (Sync)` is enabled.
- **Battery Management & Low-Battery Indication**: Periodically monitor battery voltage via `smcpad_battery_mv()` and blink Left and Right arrow LEDs when battery drops below 3500 mV.
- **Relative CC & Accelerated Encoders**: Add configurable encoder relative CC modes (Relative Signed Bit, 2's Complement, Binary Offset) to prevent parameter jumping in DAWs.
- **WebMIDI Configurator Profiles**: Add a full Profile Management engine to `tools/web-configurator.html` allowing users to save, name, load, import, and export complete performance profiles (with built-in presets: "Future Funk Setup", "Cumbia Setup", "Studio DAW Mackie", "MPC Finger Drumming").

## Capabilities

### New Capabilities
- `power-management`: Battery voltage monitoring and low-battery visual alert state.
- `web-profiles`: WebMIDI profile manager supporting persistent named setups, JSON import/export, and genre-oriented factory presets.

### Modified Capabilities
- `midi-dispatcher`: Direct 3.5mm UART MIDI output, DAW transport command dispatch, Channel Pressure/Aftertouch dispatch, and incoming MIDI Clock reception.
- `pad-adc`: Continuous pressure sensing after Note-On trigger.
- `navigation-and-repeat`: External MIDI clock synchronization for Note Repeat engine.
- `encoder-engine`: Relative CC modes and rotation acceleration.

## Impact

- `include/smc_hw.h`, `include/midi_dispatcher.h`, `include/pad_adc.h`, `include/encoder.h`: Extended structs and configuration types.
- `src/midi_dispatcher.c`, `src/pad_adc.c`, `src/encoder.c`, `src/app_main.c`: Pipeline execution updates for UART output, transport events, clock ticks, and pressure scanning.
- `tools/web-configurator.html`: Added UI tabs and local storage management for Profiles and relative encoder options.
- Zero breaking changes to existing SysEx protocol wire formats.
