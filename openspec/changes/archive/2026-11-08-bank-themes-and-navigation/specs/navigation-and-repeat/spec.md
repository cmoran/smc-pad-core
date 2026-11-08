## ADDED Requirements

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
The system SHALL provide a rhythmic Note Repeat mode toggled via Button 9, continuously retriggering held pad strikes at an interval corresponding to 1/16th notes (125 ms at 120 BPM) while the button is active.

#### Scenario: Engaging Note Repeat
- **WHEN** Button 9 (Note Repeat) is pressed
- **THEN** Note Repeat mode is activated, Button LED 9 turns on, and holding any pad emits periodic Note-On / Note-Off pulses until released.

### Requirement: Shift Layer Shortcuts
The system SHALL interpret dedicated key combinations while the Shift button (Button 8) is held down.

#### Scenario: Shift + Pad bank jump
- **WHEN** Shift is held and Pad 2 is pressed
- **THEN** the active pad bank transitions directly to Bank 1 and the matrix refreshes with Bank 1's color palette.

#### Scenario: Shift + Arrow MIDI Channel switch
- **WHEN** Shift is held and the Right Arrow button is pressed
- **THEN** the MIDI output channel increments by 1 (wrapping from 16 to 1).
