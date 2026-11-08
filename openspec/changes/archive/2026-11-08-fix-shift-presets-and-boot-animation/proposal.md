# Proposal: Fix Shift Presets, Release Restoration, and Hardware Boot Animation

## Why

User hardware testing of the clean-slate firmware on the physical M-VAVE SMC-Pad revealed three operational defects:
1. **Shift Status Not Restoring on Release:** Releasing the `Shift` button (Button 8) fails to return the pad matrix back to the active bank resting color palette because `restore_bank_leds()` was only called if `g_note_repeat_cfg_mode` was active.
2. **Preset & Bank Selection Alignment:** The stock SMC-Pad firmware provides 4 Presets (Pads 1-4) with two Bank layers (Bank A and Bank B):
   - Shift + Pads 1-4 select Presets 1-4 on Bank A (PAD BANK LED off).
   - Shift + Pads 5-8 select Presets 1-4 on Bank B (PAD BANK LED on, matching pads 1-4 directly above).
   - In the Shift status dashboard, the active selection across all 8 pads (0..7) was previously limited to pads 0..3.
   - The `PAD BANK` and `KNOB BANK` panel buttons operate as 2-bank toggles (Bank 0 and Bank 1) rather than cycling across 4 banks.
3. **Boot Animation Inaudible/Invisible to Hardware:**
   - The startup animation in `smc_rgb_led_boot_animation()` invoked `smc_rgb_led_set_pad()` instead of routing through the hardware driver's `smcpad_rgb_set()`, causing `smcpad_rgb_show()` to transmit an empty buffer.
   - The duration (~500 ms) was too brief during bootloader handover for the human eye to perceive.

## What Changes

- In `src/app_main.c`:
  - Always invoke `restore_bank_leds()` when `Shift` button is released.
  - Track `g_preset_idx` (0..3) and `g_pad_bank` (0..1).
  - Map Shift + Pads 1-4 to Preset 0..3 (Bank 0, Pad Bank LED off).
  - Map Shift + Pads 5-8 to Preset 0..3 (Bank 1, Pad Bank LED on).
  - Configure `PAD BANK` (Btn 1) and `KNOB BANK` (Btn 2) as 2-bank toggles (0 <-> 1).
  - Pass hardware pixel setter `smcpad_rgb_set` to the boot animation function.
- In `src/rgb_led.c` & `include/rgb_led.h`:
  - Update `smc_rgb_led_render_shift_status()` to accept the combined active preset selection (0..7) and highlight accordingly.
  - Update `smc_rgb_led_boot_animation()` to accept `set_fn(uint8_t pad, uint32_t color)` so pixel updates write directly to the active hardware driver buffer.
  - Extend animation step pacing (~1.2 seconds total) with vibrant spiral sequencing from center out to perimeter.
