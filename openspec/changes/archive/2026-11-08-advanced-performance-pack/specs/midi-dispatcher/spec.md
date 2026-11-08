## ADDED Requirements

### Requirement: Polyphonic Key Pressure Support
The MIDI dispatcher SHALL support Polyphonic Key Pressure (0xA0) as an alternative to Channel Pressure (0xD0), dispatching pressure values targeted to the specific note assigned to the held pad.

#### Scenario: Dispatching Polyphonic Key Pressure
- **GIVEN** dispatcher pressure mode is set to Polyphonic Key Pressure
- **WHEN** an `EVENT_PAD_PRESSURE` event is dequeued for a pad with sounding note 60
- **THEN** a MIDI 3-byte message `0xA0 | ch`, `60`, `pressure` is emitted.
