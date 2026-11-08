## ADDED Requirements

### Requirement: External MIDI Clock Slaving
The Note Repeat engine SHALL support synchronization to incoming external MIDI Clock pulses (24 PPQN) when Sync mode (`Pad 15`) is active, advancing repeat steps based on clock tick counts rather than local timer intervals.

#### Scenario: Note repeat slaved to external clock
- **GIVEN** Note Repeat is active and Sync mode is enabled
- **WHEN** incoming MIDI Timing Clock (0xF8) pulses arrive
- **THEN** repeating note pulses trigger at the exact division tick boundary (e.g., every 6 ticks for 1/16 notes).

#### Scenario: External MIDI Start and Stop
- **GIVEN** Note Repeat is active and Sync mode is enabled
- **WHEN** byte 0xFA (Start) is received
- **THEN** the repeat tick accumulator resets to zero and triggers the first repeat pulse.
