## MODIFIED Requirements

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
