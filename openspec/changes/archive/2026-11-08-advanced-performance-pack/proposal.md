## Why

To transform the SMC-Pad into a world-class expressive instrument, the controller needs advanced performance capabilities: multi-note chord triggering with guitar-style strumming, musical scale and key quantization with visual RGB lighting, adaptive rotary encoder acceleration for fast parameter sweeps, polyphonic aftertouch (Poly Key Pressure) for MPE-lite expressiveness, and a real-time oscilloscope with an auto-calibration wizard in the WebMIDI configurator.

## What Changes

- **Pad Chord Engine & Strum Mode**: Allow each pad to trigger user-configured chords (up to 4 notes: e.g., Maj7, Min7, 9th, Sus4, Octave stabs) with independent sounding note tracking to ensure zero stuck notes, and an optional micro-timing strum spread (0–50 ms).
- **Scale & Key Quantization with Smart RGB Lighting**: Quantize pad notes to a selected Root Note (C..B) and Scale (Major, Natural Minor, Dorian, Mixolydian, Minor Pentatonic, Blues, Hirajoshi). Visually indicate root notes in bright gold, in-scale notes in cyan/purple, and dim out-of-scale notes.
- **Rotary Encoder Adaptive Acceleration**: Dynamically detect turn speed (inter-detent timing) to scale step size from 1 (slow, precision 1/128) up to 8 (fast flick sweeping 0–127 in half a turn).
- **Polyphonic Aftertouch (Poly Key Pressure 0xA0)**: Add configurable pressure mode allowing pads to stream Polyphonic Aftertouch targeting their specific sounding notes rather than global Channel Pressure.
- **WebMIDI Live Pad Oscilloscope & Calibration Wizard**: Add an interactive HTML5 visualizer in `tools/web-configurator.html` showing real-time ADC waveforms, noise floors, and peak windows, along with a guided 3-strike calibration wizard.

## Capabilities

### New Capabilities
- `chord-engine`: Polyphonic multi-note chord emission, note memory tracking, and micro-timing strum delay.
- `scale-engine`: Scale quantization and visual RGB key mapping.
- `web-calibration`: Real-time ADC oscilloscope and automated calibration wizard in the WebMIDI configurator.

### Modified Capabilities
- `encoder-engine`: Adaptive angular velocity acceleration.
- `midi-dispatcher`: Polyphonic Key Pressure (0xA0) emission alongside Channel Pressure.

## Impact

- `include/midi_dispatcher.h`, `include/encoder.h`, `include/smc_config.h`: Data structures extended for chord definitions, scales, acceleration, and poly aftertouch.
- `src/midi_dispatcher.c`, `src/encoder.c`, `src/app_main.c`: Runtime processing logic.
- `tools/web-configurator.html`: Interactive canvas oscilloscope, chord editor, scale picker, and calibration wizard.
