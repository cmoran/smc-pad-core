# encoder-engine Specification

## Purpose
TBD - created by archiving change foundation-core. Update Purpose after archive.
## Requirements
### Requirement: Gray code quadrature decoding
The rotary encoder engine SHALL track the 2-bit quadrature state of all 8 encoders using a deterministic Gray code state transition lookup table.

#### Scenario: Valid clockwise rotation
- **WHEN** an encoder quadrature input transitions through a valid forward Gray code sequence
- **THEN** the encoder value increments and an increment event is queued to the event pipeline

#### Scenario: Valid counter-clockwise rotation
- **WHEN** an encoder quadrature input transitions through a valid reverse Gray code sequence
- **THEN** the encoder value decrements and a decrement event is queued to the event pipeline

#### Scenario: Invalid state transition reject
- **WHEN** an encoder input skips states or indicates impossible transitions due to mechanical bounce
- **THEN** the transition is ignored and no movement event is generated

### Requirement: Virtual bank mapping
The encoder engine SHALL map the 8 physical encoders across 4 virtual banks, maintaining separate 7-bit CC values (0–127) for each of the 32 virtual controls.

#### Scenario: Active bank rotation
- **WHEN** an encoder rotation occurs in the currently active bank
- **THEN** the corresponding virtual CC value updates clamped to [0, 127] and a MIDI Control Change event is dispatched

