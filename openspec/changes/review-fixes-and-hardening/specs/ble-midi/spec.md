# Delta Spec: BLE-MIDI

## ADDED Requirements

### Requirement: BLE-MIDI SysEx End Timestamp and Continuation Parsing
`smc_ble_midi_send_sysex` MUST insert a timestamp byte immediately before `0xF7`, and `smc_ble_midi_parse_rx` MUST accept 2-byte and continuation packets without dropping the first payload byte.

#### Scenario: Multi-packet SysEx round-trip over BLE-MIDI
- **Given** a 35-byte SysEx frame sent with MTU 23
- **When** transmitted via `smc_ble_midi_send_sysex` and decoded via `smc_ble_midi_parse_rx`
- **Then** the decoded byte stream matches the original 35-byte SysEx frame byte-for-byte
