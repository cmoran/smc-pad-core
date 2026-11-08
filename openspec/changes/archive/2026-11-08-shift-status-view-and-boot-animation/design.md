## Context

The SMC-Pad hardware has 16 RGB LEDs (WS2812/SK6812) positioned directly under the 16 velocity pads in a 4x4 matrix. Users need immediate visual status feedback when pressing Shift, and a distinct boot animation sequence on startup.

## Goals / Non-Goals

**Goals:**
- Implement a spiral boot animation starting from the center 4 pads expanding out to the 12 perimeter pads, ending with a brief flash before settling into the bank palette.
- Implement a dedicated Shift Status Dashboard:
  - Rows 1 & 2 (Pads 1-8): Preset/Bank selection row (Amber theme, active bank highlighted bright).
  - Row 3 (Pads 9-12): Velocity curve selection row (Magenta theme, active curve highlighted bright).
  - Row 4 (Pads 13-16): Semitone and Octave transpose controls (Cyan & Blue theme, active transpose offsets highlighted).
- Update status highlights dynamically whenever a parameter changes while Shift is held.
- Cleanly restore bank idle palette on Shift release.

**Non-Goals:**
- Custom user-uploaded RGB animation scripts via SysEx (reserved for future changes).

## Decisions

### 1. Boot Animation Sequence & Timing
- **Sequence:**
  1. Center 4 pads: `[5, 6, 10, 9]` ignite sequentially in vibrant cyan.
  2. Outer perimeter: `[4, 8, 12, 13, 14, 15, 11, 7, 3, 2, 1, 0]` chase in clockwise order.
  3. All 16 pads flash brief white/cyan flare (100 ms).
  4. Fade directly into Bank 0 resting palette.
- **Timing:** ~30 ms per step; total sequence takes ~500–600 ms so it feels snappy and immediate.

### 2. Shift Status View Palette
- Distinct color coding by functional row:
  - **Presets (Pads 0..7):** Base Dim Amber `0x150800`, Active Bank Bright Gold `0xFFA000`.
  - **Velocity Curves (Pads 8..11):** Base Dim Magenta `0x150015`, Active Curve Bright Magenta `0xFF00FF`.
  - **Semitone Transpose (Pads 12..13):** Base Dim Cyan `0x001515`, Active Bright Cyan `0x00FFFF` if offset != 0.
  - **Octave Transpose (Pads 14..15):** Base Dim Blue `0x000820`, Active Bright Blue `0x0080FF` if offset != 0.

## Risks / Trade-offs

- **[Risk]** Boot animation blocking input handling if device starts while already plugged in.
  - **Mitigation:** Total duration is ~500 ms, occurring in `setup()` before `smcpad_boot_ok()` completes handshake. Inputs arriving during boot are safely queued in hardware FIFO.
