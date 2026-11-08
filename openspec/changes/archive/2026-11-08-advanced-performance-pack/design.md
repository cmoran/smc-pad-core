## Context

With core MIDI transmission, dual/triple transport, note repeat, and profile storage in place, the M-VAVE SMC-Pad can now be augmented with high-level performance features: multi-note chord voicing with strumming, musical scale quantization with active RGB key illumination, encoder acceleration, and polyphonic aftertouch.

## Goals / Non-Goals

**Goals:**
- Enable pads to trigger full chord voicings (up to 4 notes) with zero stuck notes and micro-timing strum options.
- Support key and scale quantization (Major, Minor, Dorian, Mixolydian, Pentatonic, Blues) with visual RGB root and scale feedback.
- Implement dynamic angular velocity acceleration for rotary encoders.
- Implement Polyphonic Key Pressure (`0xA0`) as a selectable pressure mode alongside Channel Pressure (`0xD0`).
- Provide an interactive ADC oscilloscope and auto-calibration wizard in the WebMIDI configurator.

**Non-Goals:**
- Polyphonic microtonal tunings.
- Audio synthesis.

## Decisions

### 1. Chord Voicings & Sounding Note Memory
- **Voicing Structure**: Each pad config can hold a chord definition consisting of a note count (1 to 4) and up to 3 interval offsets (e.g., Minor 7th = `[0, 3, 7, 10]`).
- **Sounding Note Memory**: `smc_midi_dispatcher_t` maintains `sounding_chord_notes[SMC_NUM_PADS][4]` and `sounding_chord_count[SMC_NUM_PADS]`. When a pad is released, every sounding note of that chord receives an explicit Note-Off, guaranteeing zero stuck notes regardless of any transposition or scale changes while held.

### 2. Scale Quantization & Visual Lighting
- **Quantization**: Root note (0..11) and 7 popular musical scales. When scale quantization is enabled, pad indices map to scale degrees rather than chromatic semitones.
- **Visual Feedback**: The RGB matrix visually reflects scale degrees:
  - Root notes: Vibrant Amber/Gold (`0xFFB300`)
  - In-scale notes: Bright Cyan/Aqua (`0x00E5FF`)
  - Accent / 5th notes: Vivid Magenta (`0xFF007F`)

### 3. Adaptive Encoder Acceleration
- **Ballistics Algorithm**: Computes interval $\Delta t$ between consecutive detent ticks:
  - $\Delta t < 25\text{ ms}$: Step multiplier $\times 4$
  - $25\text{ ms} \le \Delta t < 60\text{ ms}$: Step multiplier $\times 2$
  - $\Delta t \ge 60\text{ ms}$: Step multiplier $\times 1$ (single-increment precision)

### 4. Polyphonic Key Pressure
- **Selection**: Configurable pressure mode. In Poly mode, `EVENT_PAD_PRESSURE` produces standard MIDI 1.0 Polyphonic Key Pressure (`0xA0 | ch`, `note`, `pressure`).

### 5. WebMIDI Oscilloscope
- **Visualizer**: 60 FPS HTML5 Canvas graph rendering live velocity strike curves, threshold triggers, and real-time peak pressure values with a guided calibration assistant.

## Risks / Trade-offs

- **[Risk] Chord note-off packet bursts over BLE**:
  → *Mitigation*: BLE packetizer buffers all notes of a 4-note chord into a single 15-byte BLE packet, ensuring instantaneous synchronized release without jitter.
