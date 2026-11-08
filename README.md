# SMC-Pad Core — Clean-Slate Open Firmware

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build & Test](https://img.shields.io/badge/Host%20Tests-Passing-brightgreen.svg)]()
[![Target: JieLi BR25](https://img.shields.io/badge/SoC-JieLi%20AC6966C%20(pi32v2)-blue.svg)]()

A high-performance, low-latency, clean-slate open-source firmware for the **M-VAVE SMC-Pad** MIDI controller, powered by the JieLi AC6966C (BR25 family) SoC.

---

## 🌟 Acknowledgments & Credits

This project stands on the shoulders of brilliant reverse-engineering efforts:

* **[Federico Pellegrin (fedepaj/groovebox)](https://github.com/fedepaj/groovebox):** The foundational pioneer who broke new ground by demonstrating how to run custom, bare-metal C applications on the proprietary JieLi microcontroller family, providing the inspiration and proof-of-concept for open JieLi development.
* **[open-smc-pad Project & Contributors](https://github.com/open-smc-pad):** For invaluable reverse-engineering documentation of the M-VAVE SMC-Pad hardware, including GPIO pinouts, ADC multiplexer scanning registers, WS2812 SPI mappings, timer channels, and DFU bootloader boundaries.

---

## 🎯 Project Mission

The goal of **SMC-Pad Core** is to transform the M-VAVE SMC-Pad from a noisy, closed, and frustrating controller into a rock-solid, ultra-responsive, professional-grade MIDI instrument:

1. **Clean-Slate Architecture:** Written completely from scratch as a high-performance "dumb" MIDI controller. No sequencers, grooveboxes, or scale layers running on the MCU—just a blazing-fast, transparent data pipeline with sub-millisecond latency.
2. **Zero Stuck Notes & Dynamic Drift Tracking:** Continuous baseline tracking filters thermal drift and pad matrix flex, eradicating the phantom stuck notes common in stock firmware.
3. **Deterministic Rotary Encoders:** Quadrature scanning using Gray-code transition state machines and detent accumulators to eliminate skipped, double-stepped, or jumpy knob adjustments.
4. **Complete Note Repeat Engine:** Dynamic tempo scaling (30–300 BPM), swing/shuffle (50%–66%), hands-free latch mode, tap tempo, and gated duty cycles to prevent polyphony overload.
5. **Open SysEx & WebMIDI Configurator:** No closed *MidiSuite* or proprietary apps required. Built around a documented open SysEx standard (`0x7D`) with an included browser-based WebMIDI configurator.
6. **100% Reversible:** Flash layouts and bootloader partitions are fully preserved. You can revert to stock factory firmware at any time via DFU.

---

## 🏗️ System Architecture

The firmware runs a unidirectional, non-blocking, zero-allocation pipeline:

```
[Hardware Inputs]
   ├── 16x Velocity Pads (ADC Scan MUX)  ──> [Baseline Drift Tracking] ──> [Velocity Curve Engine] ──┐
   ├── 8x Rotary Encoders (Quadrature)   ──> [Gray Code State Machine]  ──> [Bank / Value Mapper]    ──┼──> [MIDI Event Dispatcher]
   └── Panel Buttons (Matrix Scan)       ──> [Debounce & Shift State]  ──> [Bank & Preset Manager]  ──┘           │
                                                                                                                  ├──> USB-MIDI Out (Class Compliant)
                                                                                                                  └──> BLE-MIDI Out (Apple/Android standard)

[Incoming MIDI Stream] (USB / BLE)
   ├── SysEx Parser (0x7D Open Protocol) ──> [Config Engine / Flash Persistence]
   └── CC / Note Feedback (Host Mode)    ──> [WS2812 RGB LED Matrix]
```

### Firmware Guardrails
- **C Standard:** Strict C99 with `-Wall -Wextra -Werror -pedantic`.
- **Zero Heap Allocations:** Zero `malloc`/`free`. All ring buffers, queue frames, and states are statically allocated.
- **Lock-Free Pipeline:** SPSC ring buffers decouple hardware scanning loops from MIDI transport dispatchers.

---

## 🎛️ Hardware Controls & Shortcuts Guide

### Normal Performance Mode
| Control | Action | Function |
| :--- | :--- | :--- |
| **BT** | Press (Button 0) | Toggles **Bluetooth LE MIDI** on/off (Blinks: pairing, Solid: connected, Off: disabled) |
| **Pads 1–16** | Strike / Release | Velocity-sensitive Note-On / Note-Off with Aftertouch |
| **Knobs 1–8** | Turn | High-resolution 360° Rotary Encoders (MIDI CC) |
| **PAD BANK** | Press (Button 1) | Toggles between **Bank A** (LED off) and **Bank B** (LED on) |
| **KNOB BANK** | Press (Button 2) | Toggles between **Knob Bank A** (LED off) and **Knob Bank B** (LED on) |
| **LEFT Arrow** | Press (Button 3) | Transpose Octave Down (-12 semitones) |
| **RIGHT Arrow**| Press (Button 4) | Transpose Octave Up (+12 semitones) |
| **NOTE REPEAT**| Press (Button 9) | Toggle Note Repeat on/off (Button LED indicates state) |

---

### Shift Layer Shortcuts (`Shift` + Control)
Holding the **SHIFT** button (Button 8) engages the visual **Shift Status Dashboard** across the 4x4 matrix (matching the physical MPC layout with Pad 1 at bottom-left and Pad 16 at top-right):

```
Top:    [ PAD 13 ] [ PAD 14 ] [ PAD 15 ] [ PAD 16 ]   --> Transpose (Semi -, Semi +, Octave -, Octave +)
        [ PAD 9  ] [ PAD 10 ] [ PAD 11 ] [ PAD 12 ]   --> Velocity Curve (Linear, Soft, Hard, Fixed 127)
        [ PAD 5  ] [ PAD 6  ] [ PAD 7  ] [ PAD 8  ]   --> Preset 1-4 (Bank B, PAD BANK LED on)
Bottom: [ PAD 1  ] [ PAD 2  ] [ PAD 3  ] [ PAD 4  ]   --> Preset 1-4 (Bank A, PAD BANK LED off)
```

- **Shift + Pads 1–4:** Select Presets 1–4 on Bank A.
- **Shift + Pads 5–8:** Select Presets 1–4 on Bank B (mirrors pads 1–4 with Bank B active).
- **Shift + Pads 9–12:** Switch Velocity Curves:
  - `Pad 9`: Linear
  - `Pad 10`: Soft (high sensitivity)
  - `Pad 11`: Hard (dynamic range for heavy playing)
  - `Pad 12`: Fixed Full Velocity (always 127)
- **Shift + Pad 13 / 14:** Transpose Semitone Down (-1) / Up (+1).
- **Shift + Pad 15 / 16:** Transpose Octave Down (-12) / Up (+12).
- **Shift + Pad 15 + Pad 16 (Dual Press):** Reset all transposition to neutral (0).
- **Shift + Left / Right Arrow:** Decrement / Increment MIDI Output Channel (Channels 1–16).
- **Shift + Stop (Button 6):** **MIDI Panic** — Sends All-Notes-Off across all 16 MIDI channels.

---

### Note Repeat Engine

#### 1. Real-Time Knob Control (Hold `Note Repeat` or `Shift + Note Repeat`)
- **Knob 1 (Rate):** Smooth 2-click detent selector stepping through:
  `1/4` $\to$ `1/4t` $\to$ `1/8` $\to$ `1/8t` $\to$ `1/16` $\to$ `1/16t` $\to$ `1/32` $\to$ `1/32t`.
- **Knob 2 (Swing):** 2-click detent selector adjusting swing percentage (`50%` straight, `54%`, `58%`, `62%`, `66%`).
- **Knob 3 (Tempo):** Dynamically scales base BPM from **30 to 300 BPM**.
- **Knob 4 (Latch):** Clockwise turns Latch ON; counter-clockwise turns Latch OFF.

#### 2. Note Repeat Configuration Mode (`Shift` + `Note Repeat`)
```
Top:    [ PAD 13: Swing 5 (66%) ] [ PAD 14: Latch Toggle ] [ PAD 15: Sync ]   [ PAD 16: Tap Tempo ]
        [ PAD 9:  Swing 1 (50%) ] [ PAD 10: Swing 2 (54%)] [ PAD 11: Swing 3] [ PAD 12: Swing 4 (62%) ]
        [ PAD 5:  Rate 1/16     ] [ PAD 6:  Rate 1/16t   ] [ PAD 7:  Rate 1/32] [ PAD 8:  Rate 1/32t   ]
Bottom: [ PAD 1:  Rate 1/4      ] [ PAD 2:  Rate 1/4t    ] [ PAD 3:  Rate 1/8 ] [ PAD 4:  Rate 1/8t    ]
```

- **Pads 1–8:** Direct Rate division buttons (`1/4` to `1/32t`).
- **Pads 9–13:** Direct Swing buttons (`50%` to `66%`).
- **Pad 14:** Latch toggle (illuminates bright green when active).
- **Pad 15:** Sync mode toggle.
- **Pad 16:** **Tap Tempo** — Tap rhythmically to set BPM; flashes bright white on strike.

---

## 🌐 WebMIDI Configurator

No installation or proprietary driver required. Open [`tools/web-configurator.html`](tools/web-configurator.html) in any Chromium-based browser (Chrome, Chromium, Brave, Edge):

1. Double click [`tools/web-configurator.html`](tools/web-configurator.html) or serve it locally.
2. Click **Connect SMC-Pad** and allow MIDI / SysEx access.
3. Visually customize:
   - Note mappings and channels for all 4 banks.
   - Rotary encoder CC assignments and limits.
   - Velocity curves and pad sensitivity thresholds.
   - Real-time strike & turn visualizers.
4. Click **Save to Flash** to store settings permanently in non-volatile flash memory.

---

## 📡 Wireless Bluetooth MIDI (BLE-MIDI)

The firmware natively advertises standard Apple/MMA MIDI over Bluetooth LE as device name `SMC-PAD`.

### Visual Status (BT LED)
- **Slow Blink (~500 ms period):** Bluetooth active & advertising (pairing mode, ready for host connection).
- **Solid ON:** Host central connected and subscribed to MIDI notifications.
- **OFF:** Bluetooth radio disabled. Press physical **BT** button (Button 0) to re-enable.

### Connecting by Operating System:
- **macOS:** Open **Audio MIDI Setup** $\to$ Window $\to$ Show MIDI Studio $\to$ Click the Bluetooth icon $\to$ Find **SMC-PAD** $\to$ Click **Connect**.
- **iOS / iPadOS:** Open any MIDI-capable app (GarageBand, AUM, Cubasis, Moog apps) $\to$ Settings $\to$ Bluetooth MIDI Devices $\to$ Select **SMC-PAD**.
- **Windows 10 / 11:** Settings $\to$ Bluetooth & devices $\to$ Add device $\to$ Bluetooth $\to$ Select **SMC-PAD** (or connect via free utility like **MIDIberry** / **Cakewalk**).
- **Linux:** Connect via `bluetoothctl` and route MIDI via PipeWire or ALSA BLE-MIDI:
  ```bash
  bluetoothctl
  scan on
  pair <MAC_ADDRESS>
  connect <MAC_ADDRESS>
  ```
  The device will appear under `aconnect -l` or PipeWire/JACK patchbays as a standard MIDI device.

---

## 🛠️ Building & Flashing

### Prerequisites

- GCC / Clang (for compiling host unit tests and packaging tool).
- `make` and `dfu-util`.
- JieLi `pi32v2` toolchain (provided under `tools/` or downloaded via script).

### 1. Run Host Test Suite
```bash
make test
```
Runs comprehensive unit tests for the ring buffer, Gray code quadrature encoder, ADC scanning matrix, anti-stuck velocity engine, SysEx protocol, BLE-MIDI packetization, and E2E pipeline simulation.

### 2. Compile Target Firmware
```bash
make firmware
```
Compiles and packages `build/app.bin` ready for flashing.

### 3. Flash to SMC-Pad
1. Hold down the **PAD BANK** (or bootloader entry button) while powering on or connecting the USB-C cable to enter DFU mode.
2. Run:
```bash
dfu-util -d 1209:0002 -a 0 -D build/app.bin
```
3. Power cycle the device. You will be greeted by the center-outward spiral boot animation!

---

## 🔄 Reverting to Stock Firmware

The JieLi factory bootloader and partition table are left untouched. To revert to the official M-VAVE factory firmware at any time:
1. Enter DFU mode.
2. Flash the stock factory binary using `dfu-util` or the official M-VAVE update utility.

---

## 📜 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.
All reverse-engineered hardware insights are cited and shared freely in the spirit of open firmware.
