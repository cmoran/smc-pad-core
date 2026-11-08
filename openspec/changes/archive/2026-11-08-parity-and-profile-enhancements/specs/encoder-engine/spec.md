## ADDED Requirements

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
