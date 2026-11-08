## MODIFIED Requirements

### Requirement: Startup Boot Animation
The RGB LED driver SHALL render an outward spiral boot animation across the 4x4 pad matrix on system power-on, writing pixel values directly via the caller-provided hardware set function, spiraling from center pads to perimeter pads over approximately 1.2 seconds before transitioning to the active bank palette.

#### Scenario: Running startup animation
- **WHEN** the device boots and `smc_rgb_led_boot_animation()` is invoked
- **THEN** pixel values are written via `set_fn`, the center pads ignite sequentially, the perimeter pads chase clockwise, and the matrix flashes briefly before settling on the resting palette.

### Requirement: Shift Status View Rendering
The RGB LED driver SHALL provide a function `smc_rgb_led_render_shift_status()` that renders distinct row color themes and highlights the currently selected preset (across Rows 1 and 2), velocity curve, and transpose states.

#### Scenario: Rendering shift dashboard
- **WHEN** `smc_rgb_led_render_shift_status()` is called with the active preset pad (0..7), velocity curve, octave, and semitone offsets
- **THEN** Pads 0..7 render in preset theme with active preset pad brightly highlighted, Pads 8..11 render in curve theme with active curve highlighted, and Pads 12..15 render in transpose theme indicating offset directions.
