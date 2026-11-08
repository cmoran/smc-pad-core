## ADDED Requirements

### Requirement: Battery Voltage Monitoring and Low-Battery Alert
The firmware SHALL periodically measure battery voltage and trigger a visual low-battery alert when operating on battery below a critical threshold.

#### Scenario: Low battery threshold breached
- **GIVEN** the device is operating on battery power
- **WHEN** the battery voltage reading falls below 3500 mV
- **THEN** the low-battery warning mode activates, flashing the Left Arrow (LED 3) and Right Arrow (LED 4) indicators simultaneously at 2 Hz.

#### Scenario: Battery returns to normal or USB power connected
- **GIVEN** the low-battery warning mode is active
- **WHEN** USB bus power is detected or voltage rises above 3600 mV
- **THEN** the low-battery alert is cleared and normal LED operation resumes.
