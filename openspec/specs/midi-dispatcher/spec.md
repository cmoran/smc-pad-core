# midi-dispatcher Specification

## Purpose
TBD - created by archiving change anti-stuck-notes-and-midi-dispatch. Update Purpose after archive.
## Requirements
### Requirement: MIDI message formatting
The MIDI dispatcher SHALL consume events from the event ring buffer and format them into standard 3-byte MIDI 1.0 channel voice messages (Note On, Note Off, Control Change) using the active bank configuration and sounding note memory.

#### Scenario: Pad strike to Note On
- **WHEN** an `EVENT_PAD_STRIKE` is dequeued
- **THEN** a MIDI `Note-On` (0x90) packet is produced with the active bank's mapped note number (adjusted by current octave and semitone offsets), MIDI channel, and calculated velocity, the sounding note and channel are recorded in the pad's tracking slot, and the pad is marked active in the note tracking mask.

#### Scenario: Pad release to Note Off
- **WHEN** an `EVENT_PAD_RELEASE` is dequeued
- **THEN** a MIDI `Note-Off` (0x80) packet is produced using the sounding note number and MIDI channel recorded at the time of the pad strike, regardless of any subsequent bank or transpose changes, and the pad is cleared in the note tracking mask.

#### Scenario: Encoder step to Control Change
- **WHEN** an `EVENT_ENCODER_DELTA` is dequeued
- **THEN** a MIDI `Control Change` (0xB0) packet is produced with the active bank encoder's assigned CC number, MIDI channel, and updated value.

### Requirement: All-Notes-Off Panic mechanism
The system SHALL provide an immediate Panic mechanism that silences all sounding notes and resets channel controller states.

#### Scenario: Panic triggered via shortcut
- **WHEN** the panic shortcut (Shift + Pause) or panic command is invoked
- **THEN** the dispatcher outputs MIDI CC 120 (All Sound Off), CC 123 (All Notes Off), emits explicit `Note-Off` messages for all tracked active notes, and clears the active note mask

### Requirement: Dual transport dispatching
The MIDI dispatcher SHALL route outgoing MIDI messages simultaneously to USB-MIDI and BLE-MIDI transports when active.

#### Scenario: Dispatching over both active transports
- **GIVEN** both USB and BLE output sinks are registered
- **WHEN** an outgoing MIDI packet is dispatched
- **THEN** the packet is delivered to both the USB-MIDI transmitter and the BLE-MIDI transmitter without blocking.

### Requirement: Incoming MIDI stream parsing
The MIDI dispatcher SHALL parse incoming MIDI 1.0 byte streams from USB and BLE, distinguishing Channel Voice messages (Note-On, CC) from System Exclusive messages.

#### Scenario: Host Note-On message triggers RGB LED feedback
- **WHEN** an incoming MIDI `Note-On` message targeted at a pad's assigned note number is parsed
- **THEN** the dispatcher translates the velocity/note to the corresponding pad RGB LED color.

#### Scenario: Ingestion of incoming SysEx byte streams
- **WHEN** an incoming `0xF0` SysEx stream is parsed
- **THEN** the bytes are forwarded to the SysEx protocol handler until `0xF7` is received.


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
