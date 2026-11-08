## ADDED Requirements

### Requirement: Four Virtual Banks for Pads and Encoders
The system SHALL support 4 distinct virtual banks (Bank 0, 1, 2, 3) for the 16 physical pads and 8 rotary encoders, allowing each bank to map independent MIDI channels, note numbers, CC numbers, and step resolutions.

#### Scenario: Switching pad bank updates pad output notes
- **GIVEN** Bank 0 maps Pad 0 to Note 36 and Bank 1 maps Pad 0 to Note 52
- **WHEN** the user switches active Pad Bank to Bank 1 and strikes Pad 0
- **THEN** the system dispatches a Note-On with Note 52.

#### Scenario: Switching encoder bank updates encoder CC parameters
- **GIVEN** Bank 0 maps Encoder 0 to CC 16 and Bank 1 maps Encoder 0 to CC 24
- **WHEN** the user switches active Encoder Bank to Bank 1 and turns Encoder 0 clockwise
- **THEN** the system emits a Control Change message for CC 24.

### Requirement: Non-Volatile Flash Configuration Persistence
The configuration engine SHALL persist device parameters (bank mappings, velocity curve, pad strike/idle RGB colors, thresholds) in non-volatile flash memory using wear-mitigating dirty caching.

#### Scenario: Preserving settings across power cycles
- **GIVEN** a modified configuration has been committed to non-volatile storage
- **WHEN** the device resets or power cycles
- **THEN** the configuration engine restores the customized mappings and calibration values from flash memory during startup.

### Requirement: Factory Reset Recovery
The configuration engine SHALL restore verified default factory parameters upon request or when flash memory is uninitialized/corrupted.

#### Scenario: Factory reset restoration
- **GIVEN** the device contains user-customized bank and threshold parameters
- **WHEN** a factory reset command is executed
- **THEN** all pad mappings, encoder CCs, velocity curves, and RGB defaults revert to factory baseline values and are committed to flash.
