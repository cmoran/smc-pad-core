## Why

Two visual feedback enhancements are needed to elevate device usability and match the stock controller experience:
1. **Shift Layer Status View:** When holding the Shift button, the user needs an instant visual status dashboard across the 4x4 pad matrix. Each functional row (Bank/Presets, Velocity Curves, Transposition) should illuminate in a distinct color, with the active option highlighted brightly so the user can immediately see their current configuration.
2. **Startup Boot Animation:** A vibrant visual startup animation (e.g., an outward spiral from the 4 center pads to the outer perimeter) provides immediate confirmation of hardware power-on, LED integrity, and firmware boot success before settling into the resting bank palette.

## What Changes

- **Boot Animation Sequence:**
  - Implement a non-blocking or timed startup sequence in `rgb_led` / `app_main.c`:
    - Spiral out from center pads `[5, 6, 10, 9]` to the outer perimeter ring `[4, 8, 12, 13, 14, 15, 11, 7, 3, 2, 1, 0]`.
    - Colorful gradient pulse before displaying the active bank idle palette.
- **Shift Status Dashboard:**
  - When Shift (Button 8) is held:
    - **Rows 1 & 2 (Pads 1-8, indices 0..7 - Presets/Banks):** Dim amber / cyan base color, with the active bank pad (`g_pad_bank`) illuminated in bright white/accent.
    - **Row 3 (Pads 9-12, indices 8..11 - Velocity Curves):** Dim green / purple base color, with the active curve (`g_config.velocity_curve`) illuminated in bright green / magenta.
    - **Row 4 (Pads 13-16, indices 12..15 - Transposition):** Dim blue base color, with Semitone/Octave pads glowing bright when active offsets are non-zero.
  - When Shift is released:
    - Matrix smoothly restores the active bank palette.

## Capabilities

### Modified Capabilities
- `rgb-led`: Support startup boot animation (spiral/snake sequence) and dynamic shift status view rendering.
- `navigation-and-repeat`: Trigger Shift status view upon Shift button press, live-update highlights on parameter changes, and restore bank palette upon Shift button release.

## Impact

- `src/rgb_led.c` and `include/rgb_led.h`: Functions for rendering boot animation frames and shift status view.
- `src/app_main.c`: Trigger boot animation in `setup()` and drive Shift status rendering on Button 8 down/up events.
- `test/test_rgb_led.c`: Unit tests verifying boot animation and shift status view buffer states.
