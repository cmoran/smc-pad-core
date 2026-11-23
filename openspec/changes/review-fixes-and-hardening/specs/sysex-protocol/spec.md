# Delta Spec: SysEx Protocol

## ADDED Requirements

### Requirement: Complete Pad and Encoder Map Round-Trip
`SMC_CMD_GET_PAD_MAP` MUST return chord and pressure parameters, and `SMC_CMD_GET_ENC_MAP` / `SMC_CMD_SET_ENC_MAP` MUST support encoder mode.

#### Scenario: Querying a pad with chord and poly pressure settings
- **Given** a pad configured with `chord_count = 3`, `chord_offsets = {4, 7, 0}`, and `pressure_mode = 1`
- **When** the host sends `SMC_CMD_GET_PAD_MAP` (`0x10`)
- **Then** the device responds with `SMC_CMD_RESP_PAD_MAP` (`0x12`) containing all 15 bytes including `chord_count`, biased `chord_offsets`, and `pressure_mode`

### Requirement: Live Config Change Notification
The SysEx engine MUST notify the application via an optional callback whenever a configuration parameter is modified so live hardware state (such as pad LEDs) updates immediately.

#### Scenario: Updating scale configuration via SysEx
- **Given** a registered `smc_sysex_change_cb`
- **When** `SMC_CMD_SET_SCALE_CFG` (`0x25`) succeeds
- **Then** the change callback is invoked with `SMC_CMD_SET_SCALE_CFG`
