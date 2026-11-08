## ADDED Requirements

### Requirement: Multi-Note Chord Voicing
The system SHALL support triggering user-defined multi-note chords (up to 4 notes) from a single pad strike, maintaining complete polyphonic note tracking to guarantee matching Note-Off emission for every sounded note on pad release.

#### Scenario: Striking a pad mapped to a Minor 7th chord
- **GIVEN** Pad 0 is configured with a Minor 7th chord (root + 3, + 7, + 10 semitones) on Note 60
- **WHEN** an `EVENT_PAD_STRIKE` is processed for Pad 0
- **THEN** four Note-On messages are dispatched (Notes 60, 63, 67, 70), and all four notes are registered in the sounding chord memory for Pad 0.

#### Scenario: Releasing a held chord pad
- **GIVEN** Pad 0 has sounding notes 60, 63, 67, and 70
- **WHEN** an `EVENT_PAD_RELEASE` is processed for Pad 0
- **THEN** four Note-Off messages are dispatched (Notes 60, 63, 67, 70), and the sounding chord memory for Pad 0 is cleared.
