## Context

The SMC-Pad firmware recently enabled bank switching, RGB color palettes, and basic octave transpositions. However, live testing by the user surfaced three issues:
1. Note Repeat flooded MIDI streams because it evaluated against arbitrary main loop ticks (`g_system_ticks`) rather than wall-clock time (`smcpad_millis()`), and lacked an envelope gate (Note-Off) interval.
2. If a pad is held while an octave arrow or bank switch occurs, releasing the pad recalculated Note-Off using the new offset/bank, leaving the originally struck note sounding permanently.
3. The front panel Shift behavior was incomplete compared to the official hardware manual (`SMC-PAD_ZE.pdf`).

## Goals / Non-Goals

**Goals:**
- Eliminate stuck notes across any transpose or bank switch by storing sounding note and MIDI channel per pad.
- Implement wall-clock driven Note Repeat with configurable rate divisions (1/4 to 1/32t) and gated Note-Off pulses (60% duty cycle) to prevent polyphony overload.
- Match official SMC-PAD manual shift layer specifications:
  - `Shift + Pads 1-8`: Bank/Preset selection.
  - `Shift + Pads 9-12`: Velocity curves (Linear, Soft, Hard, Fixed 127).
  - `Shift + Pads 13-14`: Semitone transpose (-1 / +1).
  - `Shift + Pads 15-16`: Octave transpose (-12 / +12).
  - `Shift + Pads 15 & 16`: Reset transpose and octave to 0.
  - `Shift + Note Repeat`: Enter/exit rate division selection mode on Pads 1-8.

**Non-Goals:**
- Complex arpeggiator patterns or swing quantize engines (reserved for future changes).
- Altering the raw JieLi bootloader or flash partition map.

## Decisions

### 1. Sounding Note State Storage in `smc_midi_dispatcher_t`
- **Choice:** Maintain `uint8_t sounding_notes[SMC_NUM_PADS]` and `uint8_t sounding_channels[SMC_NUM_PADS]` directly in `smc_midi_dispatcher_t`.
- **Rationale:** Statically allocated (zero `malloc`). Completely decouples `Note-Off` calculation from current bank, octave, or semitone state.
- **Alternative Considered:** Scanning currently active voices dynamically. Rejected because an array indexed directly by pad index (0..15) is O(1), predictable, and deterministic.

### 2. Wall-Clock Note Repeat with Gated Note-Off
- **Choice:** Note repeat interval is tracked using `smcpad_millis()`.
  - Gate duration = `period_ms * 60 / 100`.
  - When elapsed time >= gate duration and note is currently gated on, send `Note-Off`.
  - When elapsed time >= period, send `Note-On` and restart timer.
- **Rationale:** Host synthesizers (like Surge XT) require distinct Note-Off events to release envelopes and prevent voice polyphony exhaustion.

### 3. Stock Shift Layout Mapping
- **Choice:** Intercept pad events when `g_shift_active` is true in `app_main.c`:
  - Pads 0..7: Bank selection (Bank 0..3 supported, clamped/wrapped).
  - Pads 8..11: Curve selection (0=Linear, 1=Soft, 2=Hard, 3=Fixed).
  - Pad 12: Semitone -1; Pad 13: Semitone +1.
  - Pad 14: Octave -12; Pad 15: Octave +12.
  - Pad 14 + Pad 15 pressed together: Reset transpose offsets to 0.
  - When in Shift mode, pads do not emit MIDI note strikes.

## Risks / Trade-offs

- **[Risk]** User plays very fast trills while switching shift layers.
  - **Mitigation:** Pad release always checks `sounding_notes[pad_index]` regardless of whether Shift is currently held, guaranteeing Note-Off is always emitted for any active note.
- **[Risk]** Semitone or octave transpose could push note number beyond MIDI 0..127 range.
  - **Mitigation:** Explicit clamping to `0 <= note <= 127` before emission and storage.
