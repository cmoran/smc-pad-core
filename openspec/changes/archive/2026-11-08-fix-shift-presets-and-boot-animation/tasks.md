## 1. Shift Release Palette Restoration

- [x] 1.1 Update Button 8 UP handler in src/app_main.c to unconditionally invoke restore_bank_leds()

## 2. Preset and Bank Alignment

- [x] 2.1 Add g_preset_idx and wire Shift + Pads 1-4 to Bank A and Shift + Pads 5-8 to Bank B in src/app_main.c
- [x] 2.2 Wire PAD BANK and KNOB BANK buttons as 2-bank toggles (0 <-> 1) with button LED updates
- [x] 2.3 Update smc_rgb_led_render_shift_status() in src/rgb_led.c to highlight active preset pad across all 8 pads (0..7)

## 3. Hardware Boot Animation

- [x] 3.1 Update smc_rgb_led_boot_animation() in include/rgb_led.h and src/rgb_led.c to accept set_fn, show_fn, delay_fn
- [x] 3.2 Extend boot animation timing to ~1.2s with vivid spiral progression
- [x] 3.3 Pass smcpad_rgb_set to smc_rgb_led_boot_animation() in setup() of src/app_main.c

## 4. Verification and Target Build

- [x] 4.1 Update host unit tests in test/test_rgb_led.c and test/test_e2e_pipeline.c
- [x] 4.2 Run make test and ensure all unit tests pass
- [x] 4.3 Run make firmware and verify clean target DFU binary output
