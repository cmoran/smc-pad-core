## ADDED Requirements

### Requirement: Adaptive Rotary Velocity Acceleration
The rotary encoder engine SHALL calculate the time delta between consecutive detent ticks and apply dynamic acceleration multipliers to step changes when turned rapidly.

#### Scenario: Fast knob rotation trigger acceleration
- **GIVEN** encoder acceleration is enabled
- **WHEN** an encoder detent tick occurs within 25 ms of the previous tick
- **THEN** a $\times 4$ acceleration multiplier is applied to the step increment, advancing the value by 4 units instead of 1.
