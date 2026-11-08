# ble-midi Specification

## Purpose
TBD - created by archiving change config-sysex-and-ble. Update Purpose after archive.
## Requirements
### Requirement: Standard BLE-MIDI GATT Peripheral Service
The firmware SHALL advertise and host the standard Apple/MMA MIDI Service (UUID `03B80E5A-EDE8-4B33-A751-6CE34EC4C700`) with dynamic connection interval negotiation (7.5 ms to 15 ms).

#### Scenario: Host subscription to BLE-MIDI characteristic
- **WHEN** a central connects and writes non-zero to the Client Characteristic Configuration descriptor (0x2902)
- **THEN** the BLE-MIDI engine activates notifications and requests low-latency connection intervals.

### Requirement: BLE-MIDI Packet Packaging and Dual Output
The system SHALL package outgoing MIDI events into timestamped BLE-MIDI frames and dispatch messages concurrently across active USB-MIDI and BLE-MIDI connections.

#### Scenario: Concurrent dual dispatch of Note-On
- **GIVEN** USB-MIDI is connected and BLE-MIDI central is subscribed
- **WHEN** a pad strike event occurs
- **THEN** the Note-On is emitted immediately via USB-MIDI USB packet and as a formatted BLE-MIDI notification packet without dropping events.

### Requirement: Physical Bluetooth Toggle and Stack Binding
The firmware SHALL provide runtime activation and deactivation of the JieLi BLE peripheral stack and standard advertising via physical Button 0.

#### Scenario: Toggling Bluetooth on
- **GIVEN** Bluetooth is currently disabled
- **WHEN** physical Button 0 is pressed
- **THEN** the BLE controller initializes (`smcpad_ble_begin`), advertising is started (`smcpad_ble_advertise(1)`), and runtime state records Bluetooth as enabled.

#### Scenario: Toggling Bluetooth off
- **GIVEN** Bluetooth is currently enabled
- **WHEN** physical Button 0 is pressed
- **THEN** advertising is stopped (`smcpad_ble_advertise(0)`), the BLE-MIDI engine state is marked disconnected, and runtime state records Bluetooth as disabled.

### Requirement: Bluetooth Status Indicator LED
The firmware SHALL indicate Bluetooth radio state visually using button LED 5 (`LED_BLUETOOTH`).

#### Scenario: Advertising status indication
- **GIVEN** Bluetooth advertising is active and no host central is connected
- **WHEN** the main event loop executes
- **THEN** LED 5 flashes at a 500 ms interval to signal pairing mode.

#### Scenario: Connected status indication
- **GIVEN** a host central is connected and subscribed to the BLE-MIDI GATT characteristic
- **WHEN** the main event loop executes
- **THEN** LED 5 is illuminated solid ON.

#### Scenario: Disabled status indication
- **GIVEN** Bluetooth is disabled
- **WHEN** the main event loop executes
- **THEN** LED 5 is OFF.

