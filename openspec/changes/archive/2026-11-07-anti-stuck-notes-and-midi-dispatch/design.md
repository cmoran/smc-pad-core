## Context

Piezoelectric and force-sensitive resistor (FSR / velostat) materials exhibit mechanical relaxation hysteresis and thermal drift. When a pad is repeatedly tapped or firmly held, its resting resistance changes, causing the raw analog ADC level in the unpressed state to drift upward by 10–50 ADC counts. With static thresholds, the reading never falls back below the release threshold, stranding the pad in a pressed state and preventing the emission of the MIDI `Note-Off` event.

## Goals / Non-Goals

**Goals:**
- **Dynamic Auto-Zero Baseline Tracking**: Maintain an adaptive resting baseline for each of the 16 pads using an integer exponential moving filter during `PAD_IDLE`.
- **Relative Thresholding**: Compute strike and release points as offsets relative to the tracked baseline:
  - `trigger_adc = baseline + delta_strike`
  - `release_adc = baseline + delta_release` (where `delta_release < delta_strike`)
- **Active Note Tracking & Guaranteed Note-Off**: Track which pads currently have sounding notes via an internal bitmask.
- **Panic / All-Notes-Off**: Provide an immediate panic command (`Shift + Pause`) sending CC 120 (All Sound Off), CC 123 (All Notes Off), and direct `Note-Off` for all sounding notes.

**Non-Goals:**
- Floating point computations (all filtering must remain integer fixed-point).
- Polyphonic aftertouch pressure streaming in this phase.

## Decisions

### 1. Integer Leaky Baseline Filter
- **Decision**: In `PAD_IDLE`, update baseline via:
  `baseline = (uint16_t)(((uint32_t)baseline * 31u + sample) / 32u)`
- **Rationale**: Provides smooth drift compensation (~300 ms time constant) while completely ignoring sudden spikes (which trigger strike mode before baseline can track upwards).
- **Freezing Baseline**: The baseline filter is frozen as soon as a strike is detected and remains frozen while in `PAD_STRIKING` and `PAD_PRESSED`.

### 2. Strict Active Note State in MIDI Dispatcher
- **Decision**: Maintain a bitmask `uint16_t active_notes_mask` in `smc_midi_dispatcher_t`.
- **Rationale**: Guarantees that every `Note-On` has a corresponding `Note-Off` and allows the panic routine to flush only and all sounding notes without flooding unnecessary packets.

### 3. Dedicated Panic Shortcut
- **Decision**: Intercept `Shift + Pause` in the event handler to trigger `smc_midi_panic()`.
- **Rationale**: Immediate hardware-level escape hatch for users if an external synth or DAW plugin hangs.

## Risks / Trade-offs

- **[Risk] Long sustained press tracking**: If a user holds a pad down for several minutes, could the baseline drift?
  - *Mitigation*: The baseline is strictly frozen whenever the pad state is `PAD_STRIKING` or `PAD_PRESSED`. It only adapts when the pad is verified `PAD_IDLE`.
