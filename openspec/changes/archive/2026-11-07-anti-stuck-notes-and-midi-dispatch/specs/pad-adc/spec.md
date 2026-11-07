## MODIFIED Requirements

### Requirement: Pad release hysteresis
The pad engine SHALL maintain hysteresis between the `Note-On` trigger threshold and the `Note-Off` release threshold relative to the tracked resting baseline to avoid false re-triggering and eliminate stuck notes caused by sensor relaxation.

#### Scenario: Pad release
- **WHEN** an active pad's ADC reading drops below the baseline plus the relative release threshold
- **THEN** the pad transitions to the idle state, resumes resting baseline tracking, and queues a `Note-Off` event

## ADDED Requirements

### Requirement: Dynamic resting baseline tracking
The pad engine SHALL continuously track the resting analog baseline voltage for each idle pad using an integer low-pass filter to compensate for thermal drift and material relaxation.

#### Scenario: Baseline adaptation in idle
- **WHEN** a pad is in the idle state and receives a stable sample below the strike threshold
- **THEN** the pad updates its resting baseline towards the current sample

#### Scenario: Baseline frozen during strike
- **WHEN** a pad is in the striking or pressed state
- **THEN** baseline adaptation is suspended, preserving the pre-strike reference level
