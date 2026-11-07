# AGENTS.md — SMC-Pad Clean-Slate Open Firmware

This document defines the project scope, hardware architecture constraints, development rules, and the strict **OpenSpec** workflow required for all AI agents and contributors working in this repository.

---

## 1. Project Mission & Core Tenets

The objective is to create a **clean-slate, open-source firmware** for the **M-VAVE SMC-Pad** MIDI controller, powered by the JieLi microcontroller family (JL AC69xx / related SoC).

### Core Principles

1. **Clean-Slate Implementation (NOT an `open-smc-pad` Fork):**
   * The `open-smc-pad` project serves **strictly as living hardware and reverse-engineering documentation** (GPIO pinouts, ADC channels, timer configurations, boot initialization, and BLE/USB descriptors).
   * Do not inherit the internal Groovebox architecture, sequencers, or scale engines from `open-smc-pad`.
   * The target is a **high-performance, low-latency, "dumb" MIDI controller**: reliable, predictable, and transparent data pipeline.

2. **Reversibility & Stock Firmware Compatibility:**
   * The device bootloader, flash partition layout, and memory boundaries must remain fully intact so users can revert to the official M-VAVE stock firmware at any time.
   * Flashing procedures and memory layouts must respect hardware safety limits.

3. **Radical Improvements over Stock Firmware:**
   * **Robust Debouncing & Scanning:** Replace noisy stock polling with a deterministic Gray code state machine for rotary encoders and software debouncing for buttons.
   * **Pad Sensitivity & Velocity Calibration:** Custom ADC read pipeline for force/pressure sensing (piezo/velostat matrix) featuring noise floor suppression, crosstalk elimination, and multiple velocity curves (Linear, Logarithmic, Exponential, Fixed).
   * **Open MIDI Standards & SysEx:** Eliminate dependency on the closed *MidiSuite* utility. Implement a documented, open SysEx protocol allowing bidirectional configuration via WebMIDI browsers and DAW extensions (e.g., Reaper ReaScripts).

---

## 2. System Architecture

The firmware runs a **unidirectional, reactive data pipeline**:

```
[Hardware Inputs]
   ├── 16x Velocity Pads (ADC Matrix Scan) ──> [Noise Floor & Crosstalk Reject] ──> [Velocity Curve Engine] ──┐
   ├── 8x Rotary Encoders (GPIO Quadrature) ──> [Gray Code State Machine]         ──> [Bank / Value Mapper]    ──┼──> [MIDI Event Dispatcher]
   └── Function Buttons (GPIO)             ──> [Short/Long Press Debounce]       ──> [Bank & Shift Manager]   ──┘           │
                                                                                                                           ├──> USB-MIDI Out
                                                                                                                           └──> BLE-MIDI Out

[Incoming MIDI Stream] (USB / BLE)
   ├── SysEx Parser (Config & Bank Dump) ──> [Internal State / Non-Volatile Flash Config]
   └── CC / Note-On (RGB LED Feedback)   ──> [RGB LED Matrix Driver]
```

---

## 3. Key Subsystems

### A. Input Processing & Filtering
* **Pads (ADC Scan):**
  * Dynamic thresholding / noise floor rejection.
  * Peak-velocity sampling window (~2–5 ms) before firing `Note-On`.
  * Hysteresis between `Note-On` and `Note-Off` to avoid phantom re-triggers on release.
* **Rotary Encoders:**
  * Quadrature decoding using state-transition tables (Gray code) to eradicate skipped or double-stepped values.
  * Optional rotation velocity acceleration.
* **Function & Shift Keys:**
  * Dedicated combinations (`Shift + Pad`, `Shift + Encoder`) for rapid bank switching without navigating menus.

### B. Virtual Banks & Memory Mapping
* Support for **4 virtual banks** across the 8 physical encoders (32 virtual CCs) and 16 pads.
* Persistent non-volatile memory layout in flash EEPROM emulation, respecting flash endurance and wear limits.

### C. Open SysEx Configuration Protocol
* Uses educational/development manufacturer header (`0x7D`) or defined open ID.
* Supported commands:
  * `GET / SET_PAD_CURVE` (Linear, Soft, Hard, Fixed)
  * `GET / SET_THRESHOLD_AND_GAIN`
  * `GET / SET_ENCODER_MAP` (CC#, MIDI Channel, Step Resolution)
  * `GET / SET_PAD_MAP` (Note#, MIDI Channel, Pressure Mode)
  * `GET / SET_RGB_COLOR` (Pad color mapping and idle states)
  * `SAVE_TO_FLASH` / `FACTORY_RESET`

### D. Visual Feedback (RGB LED Matrix)
* **Local Mode:** Immediate visual response on pad strikes or encoder turns.
* **DAW/Host Mode:** Direct MIDI IN control (`Note-On` velocity = color index or dedicated CC), enabling custom host scripts (Reaper, trackers).

---

## 4. OpenSpec Development Workflow (MANDATORY)

All features, refactors, bugfixes, and code changes in this repository **must follow the OpenSpec specification-driven development workflow**. No agent or contributor should generate or modify implementation code without an approved change proposal and delta specification.

### The OpenSpec Lifecycle

```
/opsx:explore ──► /opsx:propose ──► Review & Validate ──► /opsx:apply ──► /opsx:verify ──► /opsx:archive
  (Ideation)       (Spec / Tasks)      (Human Feedback)     (Implementation)   (Testing)       (Merged to Spec)
```

1. **Explore (`/opsx:explore <query>`):**
   * Use to examine registers, reverse-engineered headers from `open-smc-pad`, and architectural tradeoffs without modifying or creating files.
2. **Propose (`/opsx:propose <feature-name>`):**
   * Creates a dedicated change directory under `openspec/changes/<change-name>/`.
   * Must include:
     * `proposal.md`: Problem statement, scope, and non-goals.
     * `specs/<domain>/spec.md`: Delta specifications using `ADDED`, `MODIFIED`, or `REMOVED` requirement blocks with explicit scenarios (`Given`, `When`, `Then`).
     * `design.md`: Register mappings, hardware timer dependencies, data structs, and memory allocations.
     * `tasks.md`: An atomic, checkable task checklist.
   * **Rule:** Do NOT write code in this step. Stop and await human review.
3. **Validate:**
   * Run `openspec validate <change-name>` in the terminal to verify formatting.
   * If adjustments are required, update via `/opsx:update` before touching code.
4. **Apply (`/opsx:apply <change-name>`):**
   * Implement only the tasks defined in `tasks.md`.
   * Update task checkboxes incrementally as items are finished.
5. **Verify & Archive (`/opsx:verify` & `/opsx:archive`):**
   * Verify hardware behavior and timing constraints.
   * Once validated, archive the change: the delta specs merge into `openspec/specs/`, and the change directory is moved to `openspec/changes/archive/`.

---

## 5. Coding & Firmware Guardrails

* **C Standard:** Strict C99 or C11. Avoid non-standard compiler extensions unless directly required by the JieLi toolchain.
* **Zero Dynamic Allocations:** `malloc` and `free` are strictly forbidden inside the main execution loop, ADC scan loops, and interrupt handlers. All state structs must be statically allocated.
* **Lock-Free Pipeline:** MIDI messages must be buffered using lock-free ring buffers between interrupt service routines (ISRs) and the main event loop.
* **Bootloader Safety:** Never emit code that touches or overrides flash sectors belonging to the JieLi factory bootloader.
* **Traceable Engineering Notes:** When setting hardware registers or masks identified through `open-smc-pad`, provide concise comments citing the JieLi register name and functional purpose.