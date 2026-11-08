# Design: Shift Presets, Release Restoration, and Hardware Boot Animation

## Context

The SMC-Pad surface has 16 RGB pads in a 4x4 grid, with Row 1 (Pads 1..4) and Row 2 (Pads 5..8) serving as the Preset and Bank matrix in stock firmware:
- Row 1: Preset 1, 2, 3, 4 with Pad Bank A (LED off)
- Row 2: Preset 1, 2, 3, 4 with Pad Bank B (LED on)

## Key Technical Decisions

### 1. Shift Release Palette Restoration
In `src/app_main.c` `SMCPAD_EV_BUTTON_UP` for `hw_ev.index == 8`:
```c
if (hw_ev.index == 8) {
    g_midi_engine.shift_active = false;
    smcpad_led_set(8, 0);
    if (g_note_repeat_cfg_mode) {
        g_note_repeat_cfg_mode = false;
    }
    restore_bank_leds();
}
```
This guarantees that releasing Shift unconditionally restores the resting palette of the currently active bank.

### 2. Preset & 2-Bank Toggle Mapping
- Presets are 0..3 (Presets 1..4).
- `g_pad_bank` is 0 (Bank A) or 1 (Bank B).
- `active_preset_pad = (g_pad_bank ? 4 : 0) + g_preset_idx;`
- On `Shift + Pad` (index < 8):
  - `g_preset_idx = hw_ev.index % 4;`
  - `g_pad_bank = (hw_ev.index >= 4) ? 1 : 0;`
  - `smc_config_set_pad_bank(g_pad_bank);`
  - `smcpad_led_set(1, g_pad_bank);`
  - `refresh_shift_status_view();`
- Button 1 (`PAD BANK`):
  - `g_pad_bank ^= 1;`
  - `smc_config_set_pad_bank(g_pad_bank);`
  - `smcpad_led_set(1, g_pad_bank);`
  - `restore_bank_leds();`
- Button 2 (`KNOB BANK`):
  - `g_encoder_bank ^= 1;`
  - `smc_config_set_encoder_bank(g_encoder_bank);`
  - `smcpad_led_set(2, g_encoder_bank);`

### 3. Hardware Boot Animation Route
Update `smc_rgb_led_boot_animation` signature:
```c
void smc_rgb_led_boot_animation(void (*set_fn)(uint8_t pad, uint32_t color),
                                void (*show_fn)(void),
                                void (*delay_fn)(uint32_t ms));
```
In hardware target (`setup()`):
```c
smc_rgb_led_boot_animation(smcpad_rgb_set, smcpad_rgb_show, smcpad_delay);
```
In host test mode:
```c
smc_rgb_led_boot_animation(smc_rgb_led_set_pad, smc_rgb_led_show, NULL);
```
Animation stages:
- Step 1: Sequential center spiral [5, 6, 10, 9] ignite (60 ms each = 240 ms)
- Step 2: Outer perimeter chase [4, 8, 12, 13, 14, 15, 11, 7, 3, 2, 1, 0] (50 ms each = 600 ms)
- Step 3: Sustained full matrix spiral hold (150 ms)
- Step 4: Full matrix flash white (120 ms)
- Step 5: Clean transition to Bank palette (80 ms)
Total: ~1.2s.
