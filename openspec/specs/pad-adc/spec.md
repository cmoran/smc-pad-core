# pad-adc Specification

## Purpose
TBD - created by archiving change foundation-core. Update Purpose after archive.
## Requirements
### Requirement: Dual multiplexer ADC sampling
The ADC scanning engine SHALL control the shared HCF4051 channel select lines (PB2 as S0, PB3 as S1, PC4 as S2) and sample ADC Channel 3 (PA4, pads 0–7) and ADC Channel 4 (PA6, pads 8–15) simultaneously across 8 mux cycles.

#### Scenario: Channel switch settling and conversion
- **WHEN** the mux select lines S0–S2 are updated to the next channel index (0 to 7)
- **THEN** the system waits a minimum settling time of 5 microseconds before triggering ADC conversions on PA4 and PA6

### Requirement: Peak velocity sampling window
The pad engine SHALL monitor ADC readings against an adjustable noise floor threshold and capture the peak pressure across a sampling window before generating a `Note-On`.

#### Scenario: Pad strike exceeds threshold
- **WHEN** an idle pad's ADC reading rises above the noise floor threshold
- **THEN** the pad enters the striking state, samples readings across a 3 ms window, captures the maximum value, and computes velocity

### Requirement: Pad release hysteresis
The pad engine SHALL maintain hysteresis between the `Note-On` trigger threshold and the `Note-Off` release threshold to avoid false re-triggering.

#### Scenario: Pad release
- **WHEN** an active pad's ADC reading drops below the release threshold
- **THEN** the pad transitions to the idle state and queues a `Note-Off` event

### Requirement: Selectable velocity curve mapping
The velocity engine SHALL map calibrated peak 12-bit ADC values to 7-bit MIDI velocity (1–127) using precomputed curves: Linear, Soft (logarithmic), Hard (exponential), and Fixed (127).

#### Scenario: Velocity curve calculation
- **WHEN** a pad strike peak value is captured
- **THEN** the system applies the active curve lookup table and clamps the resulting velocity between 1 and 127

