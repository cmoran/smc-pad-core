## Why

The M-VAVE SMC-Pad hardware possesses competent input hardware (16 velocity pads, 8 rotary encoders, 10 function buttons, RGB LED feedback, USB, and BLE), but is severely constrained by closed, jittery stock firmware and an inflexible groovebox paradigm in existing open modifications. To achieve a high-performance, low-latency, "dumb" MIDI controller with reversible flashing and open SysEx programmability, we need a clean-slate C99 firmware architecture adhering to strict real-time embedded constraints (zero dynamic allocation, lock-free ring buffers, deterministic debouncing, and crosstalk-free ADC scanning).

## What Changes

- Initialize the clean-slate firmware repository structure, C99 build conventions, and JieLi BR25 (AC6966C / pi32v2) memory safety constraints.
- Implement the unidirectional reactive MIDI data pipeline: ISRs push events to statically allocated lock-free ring buffers for the main dispatch loop.
- Implement hardware driver abstractions and scan loops for:
  - Key/Encoder matrix scan (PB0 for 8 rotary encoders, PC1 for 10 buttons) driven by 74HC595 shift registers.
  - Button LED 3×4 multiplexing with strict time-slicing to eliminate PC3/PC1 pin-sense crosstalk.
  - Dual HCF4051 analog multiplexers reading 16 velocity pads across ADC channels 3 (PA4) and 4 (PA6).
- Implement input processing engines:
  - Deterministic Gray code quadrature state machine for skip-free rotary encoder tracking.
  - Pad ADC pipeline with dynamic thresholding, peak velocity sampling window, and configurable velocity response curves.

## Capabilities

### New Capabilities
- `event-pipeline`: Statically allocated, lock-free ring buffer pipeline connecting hardware scan routines to the MIDI event dispatcher.
- `matrix-scan`: Shift-register driven matrix scanner for buttons and encoders with mutually exclusive button-LED time-slicing.
- `pad-adc`: Dual-mux ADC sampling pipeline with noise floor filtering, crosstalk rejection, and velocity curve calculation.
- `encoder-engine`: Deterministic Gray code state machine for 8 rotary encoders with bank value tracking.

### Modified Capabilities
<!-- None: Initial foundation proposal -->

## Impact

- **Affected Systems**: Foundational firmware skeleton, drivers, and core math/pipeline libraries.
- **Hardware/Toolchain**: Targets JieLi AC6966C (BR25 / pi32v2), ensuring memory addresses strictly respect factory bootloader and flash partition safety boundaries.
- **Dependencies**: Pure C99, freestanding standard library (`stdint.h`, `stdbool.h`, `stddef.h`), zero heap allocation.
