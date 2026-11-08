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


### Requirement: Relative CC and Acceleration Modes
The rotary encoder engine SHALL support Relative CC output formats (Relative Signed Bit, 2's Complement, and Binary Offset) and dynamic rotation acceleration based on turn speed.

#### Scenario: Relative CC increment
- **GIVEN** an encoder is configured for Relative 2's Complement mode
- **WHEN** the encoder is turned clockwise by one detent
- **THEN** a Control Change event is dispatched with value 1 (0x01).

#### Scenario: Relative CC decrement
- **GIVEN** an encoder is configured for Relative 2's Complement mode
- **WHEN** the encoder is turned counter-clockwise by one detent
- **THEN** a Control Change event is dispatched with value 127 (0x7F).

### Requirement: Adaptive Rotary Velocity Acceleration
The rotary encoder engine SHALL calculate the time delta between consecutive detent ticks and apply dynamic acceleration multipliers to step changes when turned rapidly.

#### Scenario: Fast knob rotation trigger acceleration
- **GIVEN** encoder acceleration is enabled
- **WHEN** an encoder detent tick occurs within 25 ms of the previous tick
- **THEN** a $\times 4$ acceleration multiplier is applied to the step increment, advancing the value by 4 units instead of 1.
