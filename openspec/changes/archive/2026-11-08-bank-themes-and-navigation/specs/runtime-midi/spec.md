## ADDED Requirements

### Requirement: Note Repeat Runtime Service
The application loop SHALL service active held notes when Note Repeat is toggled on, periodically pulsing Note-Off and Note-On messages without blocking the main event queue or USB/BLE transmission.

#### Scenario: Periodic note pulse generation
- **GIVEN** Note Repeat is enabled and Pad 0 is held down
- **WHEN** 125 ms elapsed since the last repeat pulse
- **THEN** a Note-Off followed immediately by a Note-On with current velocity is emitted.

### Requirement: Hardware Navigation Button Handling
The application loop SHALL handle button events for Left Arrow (Btn 3), Right Arrow (Btn 4), Shift (Btn 8), and Note Repeat (Btn 9), driving their corresponding panel LEDs.

#### Scenario: Shift button LED feedback
- **WHEN** Shift (Button 8) is pressed down
- **THEN** the Shift LED turns on, and when released, the Shift LED turns off.
