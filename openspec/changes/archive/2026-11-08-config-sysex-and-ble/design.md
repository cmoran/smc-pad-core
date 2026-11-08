## Context

The SMC-Pad hardware possesses 16 physical velocity pads, 8 rotary encoders, 8 function buttons, and 16 WS2812 RGB LEDs. While low-level scanning, debouncing, ADC velocity calculation, and reactive LED illumination are fully operational, the device currently operates on static, compile-time defaults.

To become an open, versatile MIDI controller as specified in `AGENTS.md`:
1. Physical controls must map across 4 virtual banks with custom note/CC numbers and MIDI channels.
2. Configuration must persist across power cycles using non-volatile flash storage without risking flash wear.
3. An open, vendor-neutral SysEx protocol (`0x7D`) must allow WebMIDI and DAW scripts to configure the device in real-time.
4. Incoming host MIDI packets must allow DAWs to control pad RGB colors directly.
5. The device must support wireless BLE-MIDI concurrently with USB-MIDI.

## Goals / Non-Goals

### Goals
- **4 Virtual Banks**: Independent mappings for all 16 pads and 8 encoders per bank.
- **Persistent Storage**: Non-volatile flash persistence via JieLi VM (`syscfg`) with dirty-flag caching and CRC integrity checking.
- **Open SysEx (`0x7D`)**: Clean, standardized bidirectional SysEx commands for all hardware settings.
- **Host DAW LED Control**: Support MIDI IN Note-On and CC to illuminate pads from external software.
- **BLE-MIDI + USB Dual Output**: Simultaneous, non-blocking dispatch across USB and BLE transports.
- **Strict Guardrails**: Zero dynamic allocation (`malloc`/`free`), statically allocated buffers, and full host unit testability under `SMC_HOST_TEST`.

### Non-Goals
- Complex on-device sequencers, arpeggiators, or scale quantizers (focus strictly on low-latency, transparent controller pipeline).
- Proprietary software protocols or closed-source tool dependencies.

## Architecture & Data Structures

### 1. Configuration Data Model (`smc_config_t`)

```c
#define SMC_NUM_BANKS        4
#define SMC_NUM_PADS         16
#define SMC_NUM_ENCODERS     8
#define SMC_CONFIG_MAGIC     0x534D4331 /* "SMC1" */
#define SMC_CONFIG_VERSION   1

typedef struct {
    uint8_t note;            /* MIDI Note Number (0..127) */
    uint8_t channel;         /* MIDI Channel (0..15) */
    uint8_t mode;            /* 0 = Momentary Note, 1 = Toggle */
    uint8_t reserved;
} smc_pad_cfg_t;

typedef struct {
    uint8_t cc;              /* MIDI CC Number (0..127) */
    uint8_t channel;         /* MIDI Channel (0..15) */
    uint8_t step_size;       /* Increment/Decrement per step */
    uint8_t min_val;         /* Clamp min */
    uint8_t max_val;         /* Clamp max */
    uint8_t reserved[3];
} smc_encoder_cfg_t;

typedef struct {
    uint32_t magic;          /* SMC_CONFIG_MAGIC */
    uint16_t version;        /* SMC_CONFIG_VERSION */
    uint16_t crc;            /* CRC16 of config payload */

    /* Bank Mappings */
    smc_pad_cfg_t     pads[SMC_NUM_BANKS][SMC_NUM_PADS];
    smc_encoder_cfg_t encoders[SMC_NUM_BANKS][SMC_NUM_ENCODERS];

    /* Pad Calibration & Curve */
    uint16_t strike_threshold;
    uint16_t release_threshold;
    uint16_t dynamic_range;
    uint8_t  scan_window_ms;
    uint8_t  velocity_curve; /* 0=Linear, 1=Soft, 2=Hard, 3=Fixed */

    /* RGB LED Defaults (GRB 24-bit encoded in uint32_t) */
    uint32_t pad_idle_color;
    uint32_t pad_hit_color;
    uint8_t  led_mode;       /* 0 = Local Reactive, 1 = Host Controlled */
    uint8_t  reserved[3];
} smc_config_t;
```

### 2. Open SysEx Specification (Educational ID `0x7D`)

Header: `0xF0, 0x7D, 0x01 (Device ID: SMC-Pad), <CMD>, <DATA...>, 0xF7`

Commands:
- `0x01` (`CMD_GET_DEVICE_INFO`): Query device firmware version, capabilities.
- `0x10` (`CMD_GET_PAD_MAP`) / `0x11` (`CMD_SET_PAD_MAP`): Bank, Pad Index, Note, Channel, Mode.
- `0x12` (`CMD_GET_ENCODER_MAP`) / `0x13` (`CMD_SET_ENCODER_MAP`): Bank, Encoder Index, CC, Channel, Step Size, Min, Max.
- `0x20` (`CMD_GET_CALIBRATION`) / `0x21` (`CMD_SET_CALIBRATION`): Thresholds, scan window, velocity curve.
- `0x30` (`CMD_GET_RGB_CONFIG`) / `0x31` (`CMD_SET_RGB_CONFIG`): Idle color, hit color, LED mode.
- `0x70` (`CMD_SAVE_TO_FLASH`): Commits in-memory config to non-volatile flash.
- `0x7F` (`CMD_FACTORY_RESET`): Restores default config and writes to flash.

Note: All 8-bit, 16-bit, and 24-bit values transferred over SysEx are encoded as 7-bit MIDI nibbles (MSB/LSB) so no data byte has bit 7 set.

### 3. Flash Non-Volatile Persistence (JieLi VM)

- On target hardware, reads and writes use `syscfg_read(CFG_USER_CONFIG_ID, buf, len)` and `syscfg_write(...)`.
- Writes are never committed per single knob turn or note hit. Writes only trigger on explicit `CMD_SAVE_TO_FLASH` or after a 3-second debounce timer following user config changes.
- On host test builds (`SMC_HOST_TEST`), a simulated RAM-backed flash memory buffer validates persistence, CRC checks, and recovery.

### 4. Dual Output MIDI Dispatcher & Incoming Stream Processing

- Outgoing messages: `smc_midi_dispatcher_t` maintains function pointers for both `send_usb_packet` and `send_ble_packet`.
- Incoming messages: `smc_midi_rx_feed(smc_midi_dispatcher_t *disp, const uint8_t *data, uint16_t len)` processes incoming bytes:
  - Channel Voice `Note-On` / `Note-Off`: If `led_mode == HOST_CONTROLLED`, routes velocity to pad RGB color.
  - SysEx streams: Reassembles multi-packet byte sequences and dispatches to the SysEx protocol parser.

### 5. BLE-MIDI Subsystem

- Uses the standard Apple/MMA BLE-MIDI GATT characteristic (`7772E5DB-3868-4112-A1A9-F2669D106BF3` inside service `03B80E5A-EDE8-4B33-A751-6CE34EC4C700`).
- Packet format: `[Header byte (0x80 | (timestamp_high & 0x3F))] [Timestamp byte (0x80 | (timestamp_low & 0x7F))] [MIDI Status] [Data1] [Data2]`.
- Connection management: When central subscribes to CCC (0x2902), requests 7.5–15 ms connection interval for low latency.

## Decisions & Rationale

1. **Static Pre-allocated Configuration**:
   * *Rationale*: In accordance with the firmware guardrails, zero dynamic memory allocation is permitted. A single static `smc_config_t` instance (~1.5 KB) lives in BSS.
2. **Standard 0x7D SysEx ID**:
   * *Rationale*: `0x7D` is the official MIDI Manufacturers Association educational/research identifier, avoiding proprietary collisions while allowing standard WebMIDI browsers and ReaScripts to configure the SMC-Pad.
3. **Decoupled HAL for Flash and BLE**:
   * *Rationale*: Abstracting flash read/write and BLE transmit via function pointers or mockable HAL interfaces allows 100% of the config engine, SysEx parser, and BLE packetizer to be tested with unit tests on x86_64 Linux.

## Risks / Trade-offs

- **[Flash Wear & Latency]**: Writing to SPI flash blocks the CPU for several milliseconds and wears flash blocks.
  * *Mitigation*: Changes are staged in RAM and only committed upon explicit `CMD_SAVE_TO_FLASH` or dedicated save actions.
- **[BLE Packet Fragmentation]**: BLE packets have an MTU limit (minimum 23 bytes, 20 payload).
  * *Mitigation*: MIDI events fit easily in single packets; larger SysEx dumps are chunked across multiple notifications.
