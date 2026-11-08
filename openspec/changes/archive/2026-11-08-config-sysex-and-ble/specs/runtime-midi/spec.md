## MODIFIED Requirements

### Requirement: Rotary Encoder Virtual Banking
The firmware SHALL map rotary encoder rotations to Control Change (CC) messages across 4 selectable virtual banks with bank-specific CC assignments, channels, and step resolutions loaded from configuration.

#### Scenario: Turning Encoder 0 in Bank 1
- **GIVEN** Bank 1 is active and Encoder 0 is rotated clockwise by one step
- **WHEN** the Gray code state machine detects a valid CW transition
- **THEN** the stored CC value increments according to the bank resolution and a MIDI CC message is transmitted over all active MIDI transports.

## ADDED Requirements

### Requirement: Pad Virtual Banking
The firmware SHALL support switching between 4 virtual banks for the 16 pads using the physical `BTN_PAD_BANK` button.

#### Scenario: Cycling pad banks
- **GIVEN** the device is on Pad Bank 0
- **WHEN** `BTN_PAD_BANK` is pressed
- **THEN** the active pad bank transitions to Bank 1 and the Pad Bank LED is illuminated.

### Requirement: Startup Configuration Initialization
The main application loop SHALL load user configuration from non-volatile flash storage on boot and apply saved velocity curves, ADC noise thresholds, and resting RGB colors.

#### Scenario: Applying stored configuration on boot
- **WHEN** device initialization executes in `main()`
- **THEN** the configuration engine validates flash data and configures the pad ADC engine and RGB LED resting colors with saved parameters.
