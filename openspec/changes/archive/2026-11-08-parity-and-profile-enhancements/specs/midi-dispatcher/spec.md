## ADDED Requirements

### Requirement: 3.5mm Hardware UART MIDI Output
The MIDI dispatcher SHALL provide a hardware UART serial sink callback and broadcast all emitted MIDI voice messages, system realtime messages, and SysEx data to the physical 3.5mm TRS port at 31,250 baud concurrently with USB-MIDI and BLE-MIDI.

#### Scenario: Dispatching Note-On to hardware UART
- **WHEN** a pad strike event is processed by the dispatcher
- **THEN** the formatted 3-byte Note-On message is delivered to the registered UART sink callback in addition to the USB and BLE sinks.

### Requirement: DAW Transport and Control Messages
The system SHALL provide physical button mapping for DAW transport operations (Play, Stop, Record) and track banking using standard Mackie Control Protocol messages on MIDI Channel 1.

#### Scenario: Pressing Play button
- **WHEN** Button 5 (Play) is pressed
- **THEN** a MIDI Note-On message is emitted for Note 94 (0x5E) with velocity 127 on Channel 1, and on release a Note-Off message is emitted with velocity 0.

#### Scenario: Pressing Record button
- **WHEN** Button 7 (Record) is pressed
- **THEN** a MIDI Note-On message is emitted for Note 95 (0x5F) with velocity 127 on Channel 1, and on release a Note-Off message is emitted with velocity 0.

#### Scenario: Pressing Stop button without Shift
- **WHEN** Button 6 (Stop) is pressed while Shift is inactive
- **THEN** a MIDI Note-On message is emitted for Note 93 (0x5D) with velocity 127 on Channel 1, and on release a Note-Off message is emitted with velocity 0.

### Requirement: Incoming MIDI Realtime Clock Reception
The MIDI dispatcher SHALL parse incoming System Realtime messages (0xF8 Timing Clock, 0xFA Start, 0xFB Continue, 0xFC Stop) from USB-MIDI or BLE-MIDI streams without interfering with running status or SysEx state, and notify registered clock callbacks.

#### Scenario: Timing clock byte received
- **WHEN** byte 0xF8 is received in the incoming stream
- **THEN** the registered clock tick callback is invoked immediately without corrupting any in-progress channel voice message buffer.
