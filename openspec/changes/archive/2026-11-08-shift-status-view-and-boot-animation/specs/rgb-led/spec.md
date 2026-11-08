## ADDED Requirements

### Requirement: Startup Boot Animation
The RGB LED driver SHALL render an outward spiral boot animation across the 4x4 pad matrix on system power-on, initiating at the center pads and spiraling to the perimeter pads before settling into the active bank palette.

#### Scenario: Running startup animation
- **WHEN** the device boots and `smc_rgb_led_boot_animation()` is invoked
- **THEN** center pads [5, 6, 10, 9] ignite sequentially followed by perimeter pads [4, 8, 12, 13, 14, 15, 11, 7, 3, 2, 1, 0], culminating in a brief matrix flash before transitioning to Bank 0 resting palette.

### Requirement: Shift Status View Rendering
The RGB LED driver SHALL provide a function `smc_rgb_led_render_shift_status()` that renders distinct row color themes and highlights the currently selected bank, velocity curve, and transpose states.

#### Scenario: Rendering shift dashboard
- **WHEN** `smc_rgb_led_render_shift_status()` is called with the active bank, velocity curve, octave, and semitone offsets
- **THEN** Pads 0..7 render in preset theme with active bank highlighted, Pads 8..11 render in curve theme with active curve highlighted, and Pads 12..15 render in transpose theme indicating offset directions.
