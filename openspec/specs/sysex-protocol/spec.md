# sysex-protocol Specification

## Purpose
TBD - created by archiving change config-sysex-and-ble. Update Purpose after archive.
## Requirements
### Requirement: Open SysEx Message Framing and Verification
The system SHALL parse and serialize MIDI System Exclusive (SysEx) messages conforming to the educational/development manufacturer ID `0x7D` (or designated open ID), rejecting truncated, malformed, or invalid checksum messages.

#### Scenario: Valid SysEx packet recognition
- **WHEN** a SysEx stream beginning with `0xF0, 0x7D, <model_id>, <cmd>` and terminating with `0xF7` is received
- **THEN** the protocol engine unpacks the payload and invokes the corresponding command handler.

#### Scenario: Malformed SysEx rejection
- **WHEN** a SysEx packet contains an invalid manufacturer ID or unexpected termination before end-of-exclusive (0xF7)
- **THEN** the protocol engine discards the frame without altering device configuration state.

### Requirement: Parameter Inspection and Configuration via SysEx
The SysEx engine SHALL provide query (`GET`) and configuration (`SET`) commands for pad note mappings, encoder CC mappings, pad strike/idle RGB colors, velocity curve selection, and ADC thresholds.

#### Scenario: Setting pad note mapping via SysEx
- **WHEN** a valid `SET_PAD_MAP` SysEx command is received specifying Bank 0, Pad 4, Note 40, Channel 1
- **THEN** the active configuration is updated accordingly and an acknowledgment response is emitted.

#### Scenario: Committing changes to flash via SysEx
- **WHEN** a valid `SAVE_TO_FLASH` SysEx command is received
- **THEN** the configuration engine writes all dirty parameters to non-volatile flash and replies with a success status frame.

