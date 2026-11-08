## Why

With the core MIDI controller pipeline, persistent storage, and open SysEx operational, users need clear visual feedback to identify the active virtual bank, intuitive navigation controls for octave transposition and channel switching, an active Note Repeat engine, and multi-purpose Shift layer combinations.

## What Changes

- **Bank-Specific RGB Palettes**: Give each of the 4 virtual pad banks a distinctive resting and active strike color scheme (Bank 0: Dim Blue / Green, Bank 1: Dim Amber / Yellow, Bank 2: Dim Magenta / Cyan, Bank 3: Dim Cyan / White), refreshing the entire 4x4 matrix immediately when cycling banks.
- **Arrow Keys (Left / Right)**:
  - Default: Octave Transposition (-12 / +12 semitones, clamped between MIDI notes 0 and 127).
  - Shift + Left / Right: MIDI Channel decrement / increment (Channels 1..16).
- **Note Repeat Engine**:
  - Toggling or holding the physical `Note Repeat` button (Button 9) activates rhythmic repetition of any currently held pad strikes at configurable clock subdivisions (default: 1/16th notes at 120 BPM or MIDI clock).
  - LED 9 illuminates when Note Repeat is engaged.
- **Enhanced Shift Combinations**:
  - `Shift + Pad 1..4`: Instant jump to Pad Bank 0, 1, 2, or 3.
  - `Shift + Pad 5..8`: Instant selection of Velocity Curve (Linear, Soft, Hard, Fixed).
  - `Shift + Pause`: MIDI Panic (All Notes Off / All Sound Off).

## Capabilities

### New Capabilities
- `navigation-and-repeat`: Octave transposition engine via Arrow buttons, Note Repeat rhythmic pulse generator with LED indicator, and Shift modifier matrix combinations.

### Modified Capabilities
- `rgb-led`: Support per-bank resting and strike color palettes with synchronous full-matrix palette updates upon bank switching.
- `runtime-midi`: Integrate Note Repeat clock ticks, arrow key octave shifts, and Shift layer chord shortcuts into the main application execution loop.

## Impact

- **Hardware Button LEDs**: Button 3 (Left), Button 4 (Right), Button 8 (Shift), and Button 9 (Note Repeat) now actively drive their corresponding panel LEDs.
- **Real-Time Responsiveness**: Note Repeat uses non-blocking millisecond timer ticks without degrading microsecond input scan latency.
