## ADDED Requirements

### Requirement: Continuous Pad Pressure and Aftertouch
The pad ADC scanning engine SHALL monitor sensor pressure continuously during the held state (`PAD_PRESSED`) after Note-On has fired, and queue pressure change events when the change exceeds a deadband threshold.

#### Scenario: Continuous pressure modulation while held
- **WHEN** a pad is in the pressed state and the sensor ADC sample changes by more than the noise deadband without dropping below the release threshold
- **THEN** an `EVENT_PAD_PRESSURE` event is generated containing the pad index and normalized 7-bit pressure value (0–127).
