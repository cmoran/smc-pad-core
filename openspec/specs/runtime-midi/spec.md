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
The firmware SHALL map rotary encoder rotations to Control Change (CC) messages across 4 selectable virtual banks.

#### Scenario: Turning Encoder 0 in Bank 1
- **GIVEN** Bank 1 is active and Encoder 0 is rotated clockwise by one step
- **WHEN** the Gray code state machine detects a valid CW transition
- **THEN** the stored CC value increments and a MIDI CC message is transmitted over USB-MIDI.

