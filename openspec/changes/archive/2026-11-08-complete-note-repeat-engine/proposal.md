## Why

User testing revealed that while the basic 1/16th rate division was working, the rest of the stock Note Repeat functionality specified in the manual (`SMC-PAD_ZE.pdf`) was missing or incomplete:
1. Knobs 2–4 while holding `Note Repeat` were not hooked up for Swing, Tempo (BPM), and Latch.
2. Knob 1 for rate division changed values too rapidly because detent pulses were unfiltered.
3. In `Shift + Note Repeat` pad mode, only Pads 1–8 had rate selections; Pads 9–13 (Swing), Pad 14 (Latch), and Pad 16 (Tap Tempo) were unmapped.
4. Note repeat timing was hardcoded to 120 BPM instead of adapting to dynamic tempo (30–300 BPM) and swing shuffle.

## What Changes

- **Dynamic BPM & Swing Timing Engine:**
  - Configurable tempo in range 30..300 BPM (default 120 BPM). Division periods scale dynamically based on `quarter_note_ms = 60000 / BPM`.
  - Configurable swing: 50% (Straight), 54%, 58%, 62%, 66%, 70%. Alternating repeat pulses stretch and compress according to swing percentage.
- **Latch Mode:**
  - Pad strikes latch in note repeat mode when active. Releasing a pad keeps it repeating. Striking a latched pad toggles it off. Disabling Note Repeat releases all latched notes.
- **Detent Smoothing on Knob Adjustments:**
  - Knobs 1–4 while holding Note Repeat:
    - Knob 1 (Rate): 2-detent accumulator for smooth, predictable rate division switching.
    - Knob 2 (Swing): Adjusts swing percentage.
    - Knob 3 (Tempo): Adjusts BPM in 1-BPM increments.
    - Knob 4 (Latch): Toggles Latch mode on/off.
- **Full 16-Pad Note Repeat Configuration Mode (`Shift + Note Repeat`):**
  - Pads 1–8 (indices 0..7): Rate selection (1/4 to 1/32t).
  - Pads 9–13 (indices 8..12): Swing selection (50% to 70%).
  - Pad 14 (index 13): Latch toggle with active illumination.
  - Pad 16 (index 15): Tap Tempo pad, recording tap intervals and pulsing to the BPM.

## Capabilities

### Modified Capabilities
- `navigation-and-repeat`: Implement complete Note Repeat engine with dynamic BPM scaling (30..300 BPM), swing shuffle, latch persistence, tap tempo, smoothed Knobs 1-4 adjustment, and full 16-pad configuration layout.

## Impact

- `src/app_main.c`: Note repeat state machine updated with BPM math, swing duty cycle, latch bitmasks, tap tempo timestamps, and encoder accumulator.
- Unit and pipeline tests in `test/test_e2e_pipeline.c` to verify BPM scaling and latch behavior.
