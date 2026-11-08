## MODIFIED Requirements

### Requirement: Note Repeat Engine
The system SHALL provide a rhythmic Note Repeat mode toggled via Button 9, continuously retriggering held and latched pad strikes with dynamic tempo scaling (30..300 BPM), swing shuffle, latch persistence, and gated Note-Off duty cycles.

#### Scenario: Engaging Note Repeat
- **WHEN** Button 9 (Note Repeat) is pressed
- **THEN** Note Repeat mode is activated, Button LED 9 turns on, and holding any pad emits periodic Note-On and Note-Off pulses timed to the active division interval.

#### Scenario: Rate division configuration
- **WHEN** Shift is held and Button 9 is pressed
- **THEN** the device enters repeat configuration mode, allowing Pads 1-8 to select divisions (1/4, 1/4t, 1/8, 1/8t, 1/16, 1/16t, 1/32, 1/32t), Pads 9-13 to set swing, Pad 14 to toggle Latch, and Pad 16 to tap tempo.

#### Scenario: Gated note off emission
- **WHEN** a pad is held in Note Repeat mode and the gate duration (60% of division period) elapses
- **THEN** an explicit Note-Off is emitted before the next period's Note-On, preventing polyphony flooding.

#### Scenario: Dynamic tempo scaling
- **WHEN** the tempo is adjusted between 30 and 300 BPM (via Knob 3 or Tap Tempo)
- **THEN** repeat division intervals scale proportionally based on `60000 / BPM`.

#### Scenario: Swing shuffle timing
- **WHEN** swing percentage is set above 50%
- **THEN** odd and even repeat intervals alternatingly stretch and compress while preserving average cycle period.

#### Scenario: Latch mode operation
- **WHEN** Latch mode is enabled (via Pad 14 or Knob 4) and a pad is struck
- **THEN** the pad continues repeating after release until tapped again or Note Repeat is deactivated.

#### Scenario: Tap tempo calculation
- **WHEN** Pad 16 is tapped consecutively in repeat configuration mode
- **THEN** the system calculates BPM from the tap interval and updates tempo.
