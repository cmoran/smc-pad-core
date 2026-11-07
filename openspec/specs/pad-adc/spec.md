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
The pad engine SHALL maintain hysteresis between the `Note-On` trigger threshold and the `Note-Off` release threshold relative to the tracked resting baseline to avoid false re-triggering and eliminate stuck notes caused by sensor relaxation.

#### Scenario: Pad release
- **WHEN** an active pad's ADC reading drops below the baseline plus the relative release threshold
- **THEN** the pad transitions to the idle state, resumes resting baseline tracking, and queues a `Note-Off` event

### Requirement: Selectable velocity curve mapping
The velocity engine SHALL map calibrated peak 12-bit ADC values to 7-bit MIDI velocity (1–127) using precomputed curves: Linear, Soft (logarithmic), Hard (exponential), and Fixed (127).

#### Scenario: Velocity curve calculation
- **WHEN** a pad strike peak value is captured
- **THEN** the system applies the active curve lookup table and clamps the resulting velocity between 1 and 127

### Requirement: Dynamic resting baseline tracking
The pad engine SHALL continuously track the resting analog baseline voltage for each idle pad using an integer low-pass filter to compensate for thermal drift and material relaxation.

#### Scenario: Baseline adaptation in idle
- **WHEN** a pad is in the idle state and receives a stable sample below the strike threshold
- **THEN** the pad updates its resting baseline towards the current sample

#### Scenario: Baseline frozen during strike
- **WHEN** a pad is in the striking or pressed state
- **THEN** baseline adaptation is suspended, preserving the pre-strike reference level

