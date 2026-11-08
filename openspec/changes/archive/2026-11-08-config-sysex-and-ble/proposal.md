## Why

The SMC-Pad firmware currently features a high-performance input scanning pipeline and RGB LED feedback, but operates with static, compile-time mappings and lacks non-volatile configuration, host bidirectional SysEx communication, and wireless BLE-MIDI connectivity. To fulfill the clean-slate vision defined in `AGENTS.md`, the device must empower users with 4 switchable virtual banks, persistent flash parameter storage, an open SysEx configuration protocol compatible with WebMIDI/DAW tools, and low-latency BLE-MIDI dual output.

## What Changes

- **Virtual Bank & Storage Engine**: Add 4 virtual banks for 16 pads and 8 rotary encoders, non-volatile EEPROM/VM flash persistence with wear mitigation and dirty-commit caching, and factory reset recovery.
- **Open SysEx Configuration Protocol (`0x7D`)**: Implement a non-proprietary SysEx parser and serializer for querying and updating pad notes, encoder CCs, velocity curves, thresholds, and custom pad RGB colors without closed vendor tools.
- **Host MIDI-IN & Reactive LED Feedback**: Process incoming MIDI messages (Note-On/CC) from DAW hosts to control pad RGB colors directly, enabling sequencer and tracker host feedback.
- **BLE-MIDI Peripheral Integration**: Implement the standard Apple/MMA BLE-MIDI GATT service atop the JieLi BR25 Bluetooth stack, enabling dual-stream USB and wireless MIDI dispatching.

## Capabilities

### New Capabilities
- `config-engine`: Non-volatile configuration storage (JieLi VM flash), virtual bank mapping (4 banks of 16 pads and 8 encoders), velocity calibration profiles, and factory defaults.
- `sysex-protocol`: Bidirectional open SysEx protocol parser/serializer (`0x7D` manufacturer ID) supporting parameter read/write (`PAD_MAP`, `ENCODER_MAP`, `PAD_CURVE`, `THRESHOLD_GAIN`, `RGB_COLOR`, `SAVE_FLASH`, `FACTORY_RESET`).
- `ble-midi`: BLE-MIDI GATT peripheral service, connection management, packet packaging with timestamps, and dual-output routing alongside USB-MIDI.

### Modified Capabilities
- `midi-dispatcher`: Support dual-transport dispatching (USB-MIDI and BLE-MIDI) and incoming MIDI stream parsing (SysEx message framing and host LED feedback).
- `runtime-midi`: Integrate runtime bank switching (`BTN_PAD_BANK`, `BTN_KNOB_BANK`), startup config loading, and incoming host packet processing in the main event loop.

## Impact

- **Firmware Size & Memory**: All configuration data structures statically allocated in BSS (~1.5 KB); flash VM writes throttled to protect endurance.
- **Hardware Drivers**: Hooks JieLi BR25 BTstack GATT routines and flash VM APIs (`syscfg_read` / `syscfg_write`).
- **Host Testing**: Test suites expanded to verify SysEx serialization/deserialization, config serialization/validation, dual dispatch, and BLE-MIDI packet framing on host x86_64.
