# midi-dispatcher Specification

## Purpose
TBD - created by archiving change anti-stuck-notes-and-midi-dispatch. Update Purpose after archive.
## Requirements
### Requirement: MIDI message formatting
The MIDI dispatcher SHALL consume events from the event ring buffer and format them into standard 3-byte MIDI 1.0 channel voice messages (Note On, Note Off, Control Change).

#### Scenario: Pad strike to Note On
- **WHEN** an `EVENT_PAD_STRIKE` is dequeued
- **THEN** a MIDI `Note-On` (0x90) packet is produced with the mapped note number and calculated velocity, and the pad is marked active in the note tracking mask

#### Scenario: Pad release to Note Off
- **WHEN** an `EVENT_PAD_RELEASE` is dequeued
- **THEN** a MIDI `Note-Off` (0x80) packet is produced with the mapped note number and zero velocity, and the pad is cleared in the note tracking mask

#### Scenario: Encoder step to Control Change
- **WHEN** an `EVENT_ENCODER_DELTA` is dequeued
- **THEN** a MIDI `Control Change` (0xB0) packet is produced with the encoder's assigned CC number and updated value

### Requirement: All-Notes-Off Panic mechanism
The system SHALL provide an immediate Panic mechanism that silences all sounding notes and resets channel controller states.

#### Scenario: Panic triggered via shortcut
- **WHEN** the panic shortcut (Shift + Pause) or panic command is invoked
- **THEN** the dispatcher outputs MIDI CC 120 (All Sound Off), CC 123 (All Notes Off), emits explicit `Note-Off` messages for all tracked active notes, and clears the active note mask

