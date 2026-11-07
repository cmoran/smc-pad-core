## Why

The stock M-VAVE SMC-Pad firmware is notorious for "ghost" or stuck notes, where a pad strike sends a `Note-On` but fails to emit `Note-Off` when released. This occurs due to velostat/piezo sensor mechanical relaxation drift against fixed ADC thresholds, lack of baseline tracking, and unhandled buffer states. To make the SMC-Pad completely reliable for expressive live performance and studio DAW use, we must implement dynamic auto-zero baseline calibration per pad, strict relative strike/release thresholding, state tracking in the MIDI dispatcher, and a dedicated All-Notes-Off Panic mechanism.

## What Changes

- Implement dynamic resting baseline tracking (auto-zero calibration filter) for each of the 16 pads in the ADC sampling engine.
- Calculate strike and release thresholds dynamically relative to each pad's measured baseline rather than hardcoded absolute values.
- Implement a dedicated MIDI Event Dispatcher (`include/midi_dispatcher.h` and `src/midi_dispatcher.c`) that transforms ring buffer events into standard 3-byte MIDI 1.0 messages with strict active note state tracking.
- Implement an All-Notes-Off panic handler triggered via `Shift + Pause` (or panic event) that emits MIDI CC 120 (All Sound Off), CC 123 (All Notes Off), and individual `Note-Off` for all tracked active notes.
- Write unit tests and an automated simulation test verifying that mechanical drift after hard strikes always successfully triggers `Note-Off`.

## Capabilities

### New Capabilities
- `midi-dispatcher`: Complete MIDI 1.0 message formatter, active note tracker, and panic (All-Notes-Off / CC 123) handler.

### Modified Capabilities
- `pad-adc`: Update ADC pad state machine with dynamic resting baseline tracking and relative release thresholding.

## Impact

- **Affected Systems**: `pad_adc` engine, MIDI event pipeline, user interaction (panic button).
- **Performance/Memory**: Completely static memory allocation (no malloc), lightweight fixed-point baseline filter in integer arithmetic.
- **Dependencies**: C99 freestanding.
