# runtime-midi Specification

## Purpose
TBD - created by archiving change 2026-11-07-rgb-led-and-runtime-midi. Update Purpose after archive.
## Requirements
### Requirement: Active Scan & Event Dispatch Loop
The firmware application SHALL continuously scan matrix inputs, ADC pad channels, and rotary encoders at a minimum rate of 1 kHz, transforming detected actions into MIDI 1.0 messages dispatched over USB-MIDI.

#### Scenario: Striking a pad sends Note-On to host
- **GIVEN** Pad 0 is struck with physical velocity corresponding to MIDI value 100
- **WHEN** the ADC scanner reaches the peak velocity window
- **THEN** a MIDI `Note-On` message (Channel 1, Note 36, Velocity 100) is emitted over USB-MIDI within 3 milliseconds.

#### Scenario: Releasing a pad sends Note-Off to host
- **GIVEN** Pad 0 was previously active
- **WHEN** the analog signal drops below the dynamic baseline release threshold
- **THEN** a MIDI `Note-Off` message (Channel 1, Note 36, Velocity 0) is emitted over USB-MIDI.

### Requirement: Local RGB Pad Feedback
In Local Mode, the firmware SHALL illuminate the struck pad with a designated active strike color upon Note-On and restore its resting idle color upon Note-Off.

#### Scenario: Pad visual strike feedback
- **GIVEN** Pad 0 is configured with idle color `0x000020` (dim blue) and hit color `0x00FF00` (bright green)
- **WHEN** Pad 0 is struck
- **THEN** its LED turns `0x00FF00` immediately, returning to `0x000020` when released.

### Requirement: Rotary Encoder Virtual Banking
The firmware SHALL map rotary encoder rotations to Control Change (CC) messages across 4 selectable virtual banks with bank-specific CC assignments, channels, and step resolutions loaded from configuration.

#### Scenario: Turning Encoder 0 in Bank 1
- **GIVEN** Bank 1 is active and Encoder 0 is rotated clockwise by one step
- **WHEN** the Gray code state machine detects a valid CW transition
- **THEN** the stored CC value increments according to the bank resolution and a MIDI CC message is transmitted over all active MIDI transports.

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

