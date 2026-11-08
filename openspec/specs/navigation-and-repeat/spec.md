# navigation-and-repeat Specification

## Purpose
TBD - created by archiving change bank-themes-and-navigation. Update Purpose after archive.
## Requirements
### Requirement: Arrow Key Octave Transposition
The system SHALL transpose the note values of all pads in the active bank up or down by 12 semitones (1 octave) when the Right or Left arrow buttons are pressed, clamping notes to the valid MIDI range (0..127).

#### Scenario: Transposing octave up
- **GIVEN** Bank 0 maps Pad 0 to Note 36
- **WHEN** the Right Arrow button (Button 4) is pressed
- **THEN** Pad 0 produces Note 48 on subsequent strikes, and the Right Arrow LED flashes.

#### Scenario: Transposing octave down
- **GIVEN** Bank 0 maps Pad 0 to Note 36
- **WHEN** the Left Arrow button (Button 3) is pressed
- **THEN** Pad 0 produces Note 24 on subsequent strikes, and the Left Arrow LED flashes.

### Requirement: Note Repeat Engine
The system SHALL provide a rhythmic Note Repeat mode toggled via Button 9, continuously retriggering held pad strikes at a selected rate division based on wall-clock milliseconds with a gated Note-Off duty cycle.

#### Scenario: Engaging Note Repeat
- **WHEN** Button 9 (Note Repeat) is pressed
- **THEN** Note Repeat mode is activated, Button LED 9 turns on, and holding any pad emits periodic Note-On and Note-Off pulses timed to the active division interval.

#### Scenario: Rate division configuration
- **WHEN** Shift is held and Button 9 is pressed
- **THEN** the device enters repeat rate configuration mode, allowing Pads 1-8 to select divisions (1/4, 1/4t, 1/8, 1/8t, 1/16, 1/16t, 1/32, 1/32t).

#### Scenario: Gated note off emission
- **WHEN** a pad is held in Note Repeat mode and the gate duration (60% of division period) elapses
- **THEN** an explicit Note-Off is emitted before the next period's Note-On, preventing polyphony flooding.

### Requirement: Shift Layer Shortcuts
The system SHALL interpret dedicated key combinations while the Shift button (Button 8) is held down, matching the official SMC-PAD hardware specification.

#### Scenario: Shift + Pad bank jump
- **WHEN** Shift is held and Pad 1-8 (indices 0..7) is pressed
- **THEN** the active bank transitions directly to the selected bank and the pad matrix refreshes with the bank palette.

#### Scenario: Shift + Pad velocity curve selection
- **WHEN** Shift is held and Pad 9-12 (indices 8..11) is pressed
- **THEN** the active velocity curve changes (Pad 9: Linear, Pad 10: Soft, Pad 11: Hard, Pad 12: Fixed 127).

#### Scenario: Shift + Pad semitone transposition
- **WHEN** Shift is held and Pad 13 or Pad 14 (indices 12..13) is pressed
- **THEN** the semitone transpose offset is decremented (-1) or incremented (+1), clamped within -12 to +12 semitones.

#### Scenario: Shift + Pad octave transposition
- **WHEN** Shift is held and Pad 15 or Pad 16 (indices 14..15) is pressed
- **THEN** the octave transpose offset is decremented (-12) or incremented (+12), clamped within -36 to +36 semitones.

#### Scenario: Shift + Pad reset transpose
- **WHEN** Shift is held and both Pad 15 and Pad 16 are pressed simultaneously
- **THEN** both the octave offset and semitone transpose offsets are reset to 0.

#### Scenario: Shift + Arrow MIDI Channel switch
- **WHEN** Shift is held and the Right or Left Arrow button is pressed
- **THEN** the MIDI output channel increments or decrements (wrapping within 0..15).

