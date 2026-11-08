# SMC-Pad Core — Comprehensive User & Performance Guide

Welcome to the definitive user and performance guide for the **M-VAVE SMC-Pad** running **SMC-Pad Core Clean-Slate Open Firmware**.

This guide covers everything from basic performance gestures and shortcuts to advanced features such as the **Pad Chord Engine**, **Musical Scale Quantization**, **Adaptive Encoder Acceleration**, **Polyphonic Aftertouch**, and the **WebMIDI Oscilloscope & Calibration Wizard**.

---

## Table of Contents
1. [Physical Layout & Controls Overview](#1-physical-layout--controls-overview)
2. [Shift Dashboard & Hardware Shortcuts](#2-shift-dashboard--hardware-shortcuts)
3. [Pad Chord Engine & Voicings](#3-pad-chord-engine--voicings)
4. [Musical Scale & Key Quantization](#4-musical-scale--key-quantization)
5. [Smart RGB Scale Lighting](#5-smart-rgb-scale-lighting)
6. [Adaptive Rotary Encoder Acceleration](#6-adaptive-rotary-encoder-acceleration)
7. [Polyphonic Key Pressure (0xA0) vs Channel Pressure (0xD0)](#7-polyphonic-key-pressure-0xa0-vs-channel-pressure-0xd0)
8. [Note Repeat & Arpeggiator Engine](#8-note-repeat--arpeggiator-engine)
9. [DAW Transport & Mackie Control Integration](#9-daw-transport--mackie-control-integration)
10. [Hardware 3-Way Concurrent Output (USB, BLE, 3.5mm TRS)](#10-hardware-3-way-concurrent-output-usb-ble-35mm-trs)
11. [WebMIDI Configurator, Oscilloscope & 3-Strike Calibration](#11-webmidi-configurator-oscilloscope--3-strike-calibration)
12. [Profiles Manager (Future Funk, Cumbia, Finger Drumming)](#12-profiles-manager-future-funk-cumbia-finger-drumming)
13. [Open SysEx Protocol Reference (0x7D)](#13-open-sysex-protocol-reference-0x7d)

---

## 1. Physical Layout & Controls Overview

The SMC-Pad hardware interface consists of:
* **16 Velocity-Sensitive Pads (4x4 Matrix):** Dynamic piezo/velostat sensor matrix with thermal drift cancellation and continuous aftertouch pressure. Arranged in standard MPC layout (Pad 1 at bottom-left, Pad 16 at top-right).
* **8 Endless 360° Rotary Encoders:** High-resolution quadrature knobs with Gray code state machine tracking (supports Absolute 0–127 and Relative CC modes).
* **10 Function & Transport Buttons:**
  - `BT` (Button 0): Toggle Bluetooth LE MIDI.
  - `PAD BANK` (Button 1): Switch Pad Banks (Bank A = LED off, Bank B = LED on).
  - `KNOB BANK` (Button 2): Switch Knob Banks (Bank A = LED off, Bank B = LED on).
  - `LEFT Arrow` (Button 3): Octave Down / DAW Track Left.
  - `RIGHT Arrow` (Button 4): Octave Up / DAW Track Right.
  - `PLAY` (Button 5): DAW Transport Play.
  - `STOP` (Button 6): DAW Transport Stop / Panic with Shift.
  - `RECORD` (Button 7): DAW Transport Record.
  - `SHIFT` (Button 8): Engage Shift Status Dashboard and secondary functions.
  - `NOTE REPEAT` (Button 9): Toggle Note Repeat engine.

---

## 2. Shift Dashboard & Hardware Shortcuts

Holding down the physical **SHIFT** button (Button 8) turns the 16 RGB pads into a live **Visual Configuration Dashboard**:

```
Row 4 (Top):    [ Pad 13: Semi -1 ] [ Pad 14: Semi +1 ] [ Pad 15: Oct -12 ] [ Pad 16: Oct +12 ]
Row 3:          [ Pad 9:  Linear  ] [ Pad 10: Soft    ] [ Pad 11: Hard    ] [ Pad 12: Fixed 127]
Row 2:          [ Pad 5:  Preset 1] [ Pad 6:  Preset 2] [ Pad 7:  Preset 3] [ Pad 8:  Preset 4 ] (Bank B)
Row 1 (Bottom): [ Pad 1:  Preset 1] [ Pad 2:  Preset 2] [ Pad 3:  Preset 3] [ Pad 4:  Preset 4 ] (Bank A)
```

### Essential Hardware Shortcuts
* **Instant Octave Transpose:** `Shift + Pad 15` (-12) or `Shift + Pad 16` (+12).
* **Instant Semitone Transpose:** `Shift + Pad 13` (-1) or `Shift + Pad 14` (+1).
* **Reset All Transposition to Neutral (0):** Simultaneously press `Shift + Pad 15 + Pad 16`.
* **Select Velocity Curve On the Fly:**
  - `Shift + Pad 9`: **Linear** (balanced response).
  - `Shift + Pad 10`: **Soft** (high sensitivity, ideal for light finger-tappers).
  - `Shift + Pad 11`: **Hard** (wide dynamic range, ideal for heavy drum hits).
  - `Shift + Pad 12`: **Fixed** (all strikes emit full 127 velocity, ideal for organs and vintage synths).
* **Change Global MIDI Output Channel (1–16):** `Shift + Left Arrow` (decrement) or `Shift + Right Arrow` (increment).
* **Emergency MIDI Panic (All Notes Off):** `Shift + STOP` (Button 6). Instantly sends CC 120 (All Sound Off), CC 123 (All Notes Off), and individual Note-Off messages for all currently sounding notes across all 16 MIDI channels.

---

## 3. Pad Chord Engine & Voicings

The firmware includes an onboard polyphonic chord generator allowing a single pad strike to sound full harmonic voicings (up to 4 simultaneous notes) with guaranteed **zero stuck notes**.

### How It Works
* Each pad configuration contains a root note and up to 3 relative semitone offsets.
* When struck, all notes in the chord voicing are emitted concurrently.
* The firmware maintains an internal table of all sounding chord notes per pad. Even if you switch virtual banks, change presets, or transpose octaves while holding a pad down, **every sounding note is released cleanly** when you let go.

### Supported Voicing Presets in WebMIDI Configurator
* **Single Note:** Normal standard MIDI operation (1 note).
* **Major Triad (`+4, +7`):** Root, Major 3rd, Perfect 5th.
* **Minor Triad (`+3, +7`):** Root, Minor 3rd, Perfect 5th.
* **Major 7th (`+4, +7, +11`):** Root, Major 3rd, Perfect 5th, Major 7th (dreamy jazz/neo-soul).
* **Minor 7th (`+3, +7, +10`):** Root, Minor 3rd, Perfect 5th, Minor 7th (deep house / funk / R&B).
* **Dominant 7th (`+4, +7, +10`):** Root, Major 3rd, Perfect 5th, Minor 7th (blues / rock).
* **Suspended 4th (`+5, +7`):** Root, Perfect 4th, Perfect 5th.
* **Power Chord (`+7, +12`):** Root, 5th, and Octave.
* **Custom Offsets:** Enter arbitrary semitones (e.g. `2, 7, 9` for add9).

---

## 4. Musical Scale & Key Quantization

When Scale Quantization is enabled, the 16 pads cease to be simple chromatic semitones and are automatically mapped to consecutive scale degrees across octaves.

### Selecting Musical Scales
In the WebMIDI Configurator under **Scale & Key Quantizer**:
1. Check **Scale Quantization Mode**.
2. Select your **Root Musical Key** (C, C#, D, D#, E, F, F#, G, G#, A, A#, B).
3. Select your **Scale Type**:
   * **Chromatic:** Standard 12-tone semitones.
   * **Major (Ionian):** Uplifting, bright, pop/classical.
   * **Natural Minor (Aeolian):** Dark, emotional, modern electronic/hip-hop.
   * **Dorian Mode:** Soulful minor with a raised 6th (synthwave, funk, jazz).
   * **Mixolydian Mode:** Major with a b7 (blues, funk, jam).
   * **Minor Pentatonic:** Universal 5-note scale for blues, rock, solos, and finger drumming.
   * **Major Pentatonic:** Sweet, melodic 5-note scale (country, R&B, soul).
   * **Blues Scale:** 6-note scale with the classic diminished 5th "blue note".

---

## 5. Smart RGB Scale Lighting

When Scale Mode is active, the RGB LED matrix illuminates to guide your hands:

```
[ GOLD / AMBER ]  =  ROOT NOTE (Tonic key center, e.g. all 'C' pads in C Major)
[ BRIGHT CYAN  ]  =  IN-SCALE NOTES (Harmonically correct scale degrees)
[ SUBTLE DIM   ]  =  OUT-OF-SCALE NOTES (Chromatic notes not part of the active scale)
```

The WebMIDI Configurator matches this exact visual feedback in real-time, displaying golden and cyan badges on the pad matrix.

---

## 6. Adaptive Rotary Encoder Acceleration

The 8 rotary encoders feature dynamic angular velocity ballistics. Rather than requiring hundreds of turns to sweep from CC 0 to 127, the firmware calculates the interval $\Delta t$ between consecutive detents:

| Rotation Speed | Detent Interval ($\Delta t$) | Step Multiplier | Use Case |
| :--- | :--- | :--- | :--- |
| **Slow / Deliberate** | $\ge 60\text{ ms}$ | $\mathbf{\times 1}$ (Precision) | Fine-tuning filter resonance, EQ notch, micro-tempo |
| **Moderate** | $25\text{ ms} \le \Delta t < 60\text{ ms}$ | $\mathbf{\times 2}$ (Medium) | Normal volume / pan adjustments |
| **Rapid Flick** | $< 25\text{ ms}$ | $\mathbf{\times 4}$ (Ballistic) | Sweeping filter cutoff from 0 to 127 in a single motion |

*Acceleration applies to both Absolute CC and Relative CC modes.*

---

## 7. Polyphonic Key Pressure (`0xA0`) vs Channel Pressure (`0xD0`)

The SMC-Pad hardware features continuous pressure sensing beneath the pads after the initial strike. You can choose how aftertouch messages are dispatched:

### 1. Channel Pressure (`0xD0`) — Default
* Emits standard MIDI Channel Aftertouch (`0xD0 | ch`, `pressure`).
* Compatible with vintage synths, DAWs, and general MIDI sound modules.
* Pressure applied to any held pad modulates the entire instrument channel (e.g. global vibrato or filter cutoff).

### 2. Polyphonic Key Pressure (`0xA0`) — Modern Poly Synths & MPE
* Emits Polyphonic Aftertouch (`0xA0 | ch`, `note_number`, `pressure`).
* Modulates **only the specific note of the pad you press harder**, while other held notes remain unaffected.
* Perfect for expressive synths like **Serum**, **Vital**, **Arturia Pigments**, **u-he Diva**, **Ableton Wavetable**, and **Bitwig Studio**.

*You can configure aftertouch globally or customize it per-pad in the WebMIDI Configurator.*

---

## 8. Note Repeat & Arpeggiator Engine

The built-in Note Repeat engine transforms pad strikes into continuous rhythmic rolls synced to internal or external tempo.

### Quick Knob Controls (Hold `Note Repeat` Button)
* **Knob 1 (Rate):** Turn to step through: `1/4` $\to$ `1/4t` $\to$ `1/8` $\to$ `1/8t` $\to$ `1/16` $\to$ `1/16t` $\to$ `1/32` $\to$ `1/32t`.
* **Knob 2 (Swing):** Turn to adjust groove: `50%` (straight) $\to$ `54%` $\to$ `58%` $\to$ `62%` $\to$ `66%` (hard swing).
* **Knob 3 (Tempo):** Scale internal BPM from **30 to 300 BPM**.
* **Knob 4 (Latch):** Clockwise engages hands-free repeat latching; counter-clockwise turns it off.

### Visual Note Repeat Dashboard (`Shift + Note Repeat`)
* **Pads 1–8:** Directly select rhythm rates `1/4` through `1/32t`.
* **Pads 9–13:** Directly select swing percentages `50%` to `66%`.
* **Pad 14 (Latch):** Toggle latch on/off (illuminates bright green).
* **Pad 15 (Sync):** Toggle external MIDI Clock sync (slaves to incoming `0xF8` DAW ticks).
* **Pad 16 (Tap Tempo):** Tap 3 or more times rhythmically to calculate BPM; flashes bright white on strike.

---

## 9. DAW Transport & Mackie Control Integration

The SMC-Pad hardware transport buttons transmit standard Mackie Control Protocol messages on **MIDI Channel 1**:

| Physical Button | MIDI Message | Mackie Assignment | Function |
| :--- | :--- | :--- | :--- |
| **PLAY** (Btn 5) | `Note-On 94` (`0x5E`), Vel 127 | `TRANSPORT_PLAY` | Start / Resume DAW playback |
| **STOP** (Btn 6) | `Note-On 93` (`0x5D`), Vel 127 | `TRANSPORT_STOP` | Stop DAW playback |
| **RECORD** (Btn 7) | `Note-On 95` (`0x5F`), Vel 127 | `TRANSPORT_RECORD`| Toggle DAW recording |
| **LEFT Arrow** | `Note-On 46` (`0x2E`), Vel 127 (Preset 2) | `TRACK_LEFT` | Previous track / Bank left in DAW |
| **RIGHT Arrow** | `Note-On 47` (`0x2F`), Vel 127 (Preset 2) | `TRACK_RIGHT` | Next track / Bank right in DAW |

*Works out of the box in Reaper, Ableton Live, Logic Pro, FL Studio, Cubase, and Studio One by enabling Mackie Control on the SMC-Pad MIDI input.*

---

## 10. Hardware 3-Way Concurrent Output (USB, BLE, 3.5mm TRS)

All performance actions (pad strikes, chords, aftertouch, knob turns, transport buttons) are dispatched simultaneously to **three physical output paths**:

1. **USB-MIDI:** Class-compliant USB 2.0 full-speed connection with zero driver installation.
2. **Wireless BLE-MIDI:** Standard Apple/MMA MIDI over Bluetooth Low Energy (device name: `SMC-PAD`).
3. **3.5mm TRS Hardware UART MIDI OUT:** Pin `PC5` outputs standard 31,250 baud serial MIDI for controlling dawless hardware synthesizers, drum modules (Roland, Korg, Elektron), and Eurorack modular rigs directly without a computer.

---

## 11. WebMIDI Configurator, Oscilloscope & 3-Strike Calibration

Open [`tools/web-configurator.html`](../tools/web-configurator.html) in Google Chrome, Chromium, Brave, or Microsoft Edge.

### Live CRT Oscilloscope
* Located on the left panel beneath the knobs.
* Displays a real-time, 60 FPS CRT visualization of pad strikes and aftertouch pressure waveforms.
* Renders current threshold boundary lines and dynamic peak meters.

### Automated 3-Strike Calibration Wizard
To customize pad sensitivity to your unique finger-drumming style:
1. Click **Start Wizard** in the 3-Strike Calibration card.
2. **Strike 1 (Pianissimo):** Tap any pad very lightly. This calibrates the noise floor and soft-strike baseline.
3. **Strike 2 (Mezzo-Forte):** Strike with your normal playing intensity.
4. **Strike 3 (Fortissimo):** Strike firmly with your maximum playing power.
5. The wizard automatically calculates your optimal Strike Threshold, Release Threshold, Dynamic Range, and recommended Velocity Curve.
6. Click **Save Calibration** to immediately push these settings to device memory.

---

## 12. Profiles Manager (Future Funk, Cumbia, Finger Drumming)

The configurator features a built-in profile manager with preloaded setups:
* **"Future Funk Setup":** Soft velocity curve, mapped drums, classic French house synth bass notes, and high-pass/low-pass filter macro knobs.
* **"Cumbia & Latin Percussion":** Hard velocity curve for hand slapping, mapped to authentic Latin percussion (Tambora, Timbales, Shaker, Conga Open/Slap, Campana, Guiro).
* **"Studio DAW Mixing (Mackie)":** Mackie control transport, track selection, and channel pan/volume macros.
* **"MPC Finger Drumming":** Classic GM drum layout with Linear velocity dynamics.

You can create your own setups, click **Push to SMC-Pad** to flash them to the hardware, or click **Export JSON** to share your presets with other musicians.

---

## 13. Open SysEx Protocol Reference (0x7D)

The firmware communicates via open, bidirectional SysEx packets using Educational/Open ID `0x7D`:

```
Header:  0xF0 0x7D 0x01 <COMMAND> [PAYLOAD...] 0xF7
```

### Opcodes
* `0x01`: `GET_DEV_INFO` $\to$ Returns firmware version, bank count, pad count, encoder count.
* `0x10`: `GET_PAD_MAP` (`<bank> <pad>`) $\to$ Returns note, channel, trigger mode.
* `0x11`: `SET_PAD_MAP` (`<bank> <pad> <note> <ch> <mode> [<chord_cnt> <off0+64> <off1+64> <off2+64> <press_mode>]`) $\to$ Configures pad mapping, chord voicings, and aftertouch mode.
* `0x13`: `SET_ENC_MAP` (`<bank> <enc> <cc> <ch> <step> <min> <max>`) $\to$ Configures encoder CC assignments.
* `0x21`: `SET_CALIBRATION` $\to$ Sets strike/release thresholds, dynamic range, and velocity curves.
* `0x24`: `GET_SCALE_CFG` $\to$ Queries current scale engine settings.
* `0x25`: `SET_SCALE_CFG` (`<enabled> <root_note> <scale_type>`) $\to$ Enables/disables scale quantization, key root, and scale type.
* `0x70`: `SAVE_TO_FLASH` $\to$ Permanently commits in-memory settings to non-volatile flash.
* `0x7F`: `FACTORY_RESET` $\to$ Restores clean factory defaults and writes to flash.
