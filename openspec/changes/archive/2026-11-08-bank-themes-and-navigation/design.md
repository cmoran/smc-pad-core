## Context

On physical hardware, visual feedback and ergonomic navigation make the difference between a generic controller and a professional live performance instrument.
1. Users need to recognize at a glance which of the 4 virtual banks is active by color.
2. The Left and Right arrow buttons (Buttons 3 & 4) currently had no function assigned; standard pad controllers use these for octave transposition and channel switching.
3. The Shift button (Button 8) and Note Repeat button (Button 9) have physical LEDs and panel labels that should be fully interactive.

## Goals / Non-Goals

### Goals
- **Per-Bank Visual Palettes**:
  - Bank 0: Dim Blue (`0x000020`) idle / Green (`0x00FF00`) hit
  - Bank 1: Dim Amber (`0x201000`) idle / Yellow (`0xFFFF00`) hit
  - Bank 2: Dim Magenta (`0x200020`) idle / Cyan (`0x00FFFF`) hit
  - Bank 3: Dim Cyan (`0x002020`) idle / White (`0xFFFFFF`) hit
  - Instant whole-matrix color refresh when changing pad banks.
- **Octave Transposition**:
  - Left Arrow (Btn 3): Transpose -12 semitones (-1 octave).
  - Right Arrow (Btn 4): Transpose +12 semitones (+1 octave).
  - Notes strictly clamped to valid MIDI range (0..127).
- **Shift Modifiers**:
  - `Shift + Left / Right`: Decrement / Increment global MIDI Channel (1..16).
  - `Shift + Pad 1..4`: Direct bank jump.
  - `Shift + Pad 5..8`: Direct velocity curve switch.
  - `Shift + Pause`: MIDI Panic.
- **Note Repeat Engine**:
  - Button 9 toggles Note Repeat on/off with Button 9 LED indication.
  - Non-blocking rhythmic repeat pulses at 125 ms (1/16 note at 120 BPM).

### Non-Goals
- Complex multi-menu LCD display UI (the SMC-Pad has no screen; visual feedback is through RGB pads and button LEDs).

## Architecture & Implementation

### 1. Bank Color Table
```c
typedef struct {
    uint32_t idle_color;
    uint32_t hit_color;
} smc_bank_palette_t;

static const smc_bank_palette_t g_bank_palettes[4] = {
    { 0x000020u, 0x00FF00u }, /* Bank 0: Blue / Green */
    { 0x201000u, 0xFFFF00u }, /* Bank 1: Amber / Yellow */
    { 0x200020u, 0x00FFFFu }, /* Bank 2: Magenta / Cyan */
    { 0x002020u, 0xFFFFFFu }  /* Bank 3: Cyan / White */
};
```

### 2. Octave Transpose & Channel State
- `int8_t g_octave_offset`: Clamped between -4 and +4 octaves (-48 to +48 semitones).
- `uint8_t g_midi_channel_offset`: Global channel override when set (0..15).

### 3. Note Repeat Pulse Engine
- Maintains `uint32_t g_last_repeat_tick` and `uint16_t g_held_pads_mask`.
- In `loop()`, when `g_note_repeat_enabled` is true and `g_held_pads_mask > 0`:
  - If `millis() - g_last_repeat_tick >= 125`:
    - For each held pad: emits Note-Off, updates `g_last_repeat_tick`, and immediately re-emits Note-On with struck velocity!

## Decisions & Rationale

- **Instant Matrix Color Wipe**: Updating all 16 RGB LEDs upon bank change takes only 0.73 ms via SPI1 DMA and gives immediate, unmistakable visual awareness to the player.
- **Button LED Indication**: Driving panel LEDs for Shift (LED 8) and Note Repeat (LED 9) provides clear tactile and visual confirmation of active modifiers.
