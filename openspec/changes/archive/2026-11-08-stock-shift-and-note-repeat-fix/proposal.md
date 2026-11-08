## Why

User testing of the recent firmware builds revealed three critical usability and playability issues:
1. **Note Repeat Polyphony Flooding:** Note repeat timing was ticking on raw loop iterations (`g_system_ticks`), firing hundreds of notes per millisecond without gate intervals and overloading synths (e.g., Surge XT polyphony reaching 11/16 immediately).
2. **Stuck Notes on Transpose / Bank Switch:** Releasing a pad after transposing or changing banks recalculated the Note-Off note from the new bank/octave instead of the note that was originally struck, leaving notes sounding indefinitely.
3. **Missing Stock Shift Functionality:** The official M-VAVE SMC-Pad hardware manual (`SMC-PAD_ZE.pdf`) defines comprehensive Shift layers (Bank 1-8 jump, Velocity curves 1-4, Semitone transpose ±1, Octave transpose ±12, Octave reset, and Note Repeat rate selection) that were missing or inconsistent with the physical front panel layout.

## What Changes

- **Sounding Note & Channel Memory in MIDI Dispatcher:** Add explicit `sounding_notes[16]` and `sounding_channels[16]` tracking to `smc_midi_dispatcher_t`. On `EVENT_PAD_STRIKE`, record the dispatched note and channel. On `EVENT_PAD_RELEASE`, emit `Note-Off` matching the sounding note and channel, guaranteeing zero stuck notes regardless of concurrent bank or octave shifts.
- **Wall-Clock Note Repeat Engine:** Drive Note Repeat using hardware wall-clock time (`smcpad_millis()`) with configurable rate divisions (1/4, 1/8, 1/16, 1/32, plus triplet modes) and an envelope gate interval (60% on, 40% off) to prevent note flooding and give host synths clean gate transitions.
- **Stock Hardware Shift Layer Implementation:** Match the SMC-PAD manual specification:
  - `Shift + Pads 1-8` (indices 0..7): Switch active preset / bank.
  - `Shift + Pads 9-12` (indices 8..11): Select pad velocity curve (Linear, Soft, Hard, Fixed Full Velocity).
  - `Shift + Pads 13-14` (indices 12..13): Semitone transpose down (-1) and up (+1).
  - `Shift + Pads 15-16` (indices 14..15): Octave transpose down (-12) and up (+12).
  - `Shift + Pads 15 & 16` simultaneously: Reset transpose and octave offset to default (0).
  - `Shift + Note Repeat`: Enter Note Repeat rate division selection mode on Pads 1-8 (1/4 to 1/32t).
  - Arrow buttons: Octave transpose and track bank navigation.

## Capabilities

### Modified Capabilities
- `midi-dispatcher`: Pad release MUST emit Note-Off using the note number and MIDI channel recorded at the time of the pad strike, eliminating stuck notes across bank and transpose changes.
- `navigation-and-repeat`: Wall-clock driven note repeat with gated note-off cycles, and full stock Shift navigation layers (preset selection, velocity curves, semitone transpose, octave transpose and reset, and repeat rate division).

## Impact

- `include/midi_dispatcher.h` and `src/midi_dispatcher.c`: Struct additions for `sounding_notes` and `sounding_channels`, updated strike and release logic.
- `src/app_main.c`: Wall-clock timing for note repeat, shift-layer state machine, rate division handling, velocity curve selection, and transpose logic.
- `test/test_midi_dispatcher.c` and `test/test_e2e_pipeline.c`: Unit and pipeline tests to verify stuck note prevention and repeat gate timing.
