## ADDED Requirements

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
