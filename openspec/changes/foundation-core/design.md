## Context

The M-VAVE SMC-Pad is an ultra-compact USB/BLE MIDI controller powered by the JieLi AC6966C SoC (BR25 family, pi32v2 32-bit core, 24 MHz crystal, integrated BLE radio and USB 2.0 full-speed controller).
The hardware components include:
- **16 Velocity-Sensitive Pads**: Arranged in two groups of 8, routed through two HCF4051 analog multiplexers whose common outputs feed ADC Channel 3 (PA4) and ADC Channel 4 (PA6). Channel selection S0, S1, S2 is shared across PB2, PB3, PC4.
- **8 Rotary Encoders & 10 Buttons**: 16 columns driven by two cascaded 74HC595 serial shift registers (PB6 SRCLK, PB7 SER, PB4 RCLK, PB5 /OE). The 8 encoder quadrature pairs return on PB0 (Sense 0), and 10 function buttons return on PC1 (Sense 1).
- **10 Button Status LEDs**: Multiplexed via 3 active-low high-side phases (PA3, PC2, PC3) and 4 active-low low-side sinking segments (PA5, PA2, PA0, PA1). *Crucial constraint*: Phase 2 on PC3 physically pulls down Sense 1 on PC1 when driven, corrupting button reads unless driving and sensing are strictly interleaved.
- **16 Pad RGB LEDs**: Chained WS2812B/SK6812 style LEDs connected to PB1 driven via SPI1 DMA.

## Goals / Non-Goals

**Goals:**
- **Zero Dynamic Allocation**: 100% static allocation for all data queues, state machines, and configuration blocks.
- **Lock-Free Pipeline**: SPSC (Single-Producer Single-Consumer) ring buffers separating interrupt-driven hardware scan routines from MIDI dispatch.
- **Crosstalk & Ghosting Elimination**:
  - Time-sliced interleaved scheduling ensuring Button LED phase outputs are disabled during button/encoder matrix sense reads.
  - Dedicated settling delays on 4051 multiplexer switching prior to ADC sampling.
- **Deterministic Encoder Decoding**: Table-driven Gray code state machine transitions to eliminate skipped or reversed clicks.
- **Multi-Curve Velocity Engine**: Real-time conversion of peak ADC force values into MIDI velocity (0–127) using selectable curves (Linear, Logarithmic/Soft, Exponential/Hard, and Fixed).

**Non-Goals:**
- Sequencers, arpeggiators, scales, or onboard synth engines (firmware is strictly a high-performance, transparent MIDI controller).
- Overwriting or modifying the factory bootloader flash sectors.

## Decisions

### 1. Lock-Free SPSC Ring Buffer Architecture
- **Decision**: Use a power-of-two capacity ring buffer with head and tail index masks.
- **Rationale**: Scanning routines running inside periodic timer ISRs produce raw input change events. The main loop consumes these events and translates them into MIDI packets (USB-MIDI and BLE-MIDI). A lock-free SPSC queue eliminates mutex/interrupt-disable overhead.
- **Alternatives Considered**: Direct MIDI dispatch inside ISRs (causes excessive ISR latency and jitter under heavy bus loads).

### 2. Time-Sliced Matrix and Button-LED Multiplexing
- **Decision**: Schedule matrix scanning and button-LED multiplexing on alternating sub-millisecond ticks.
- **Rationale**: Since driving PC3 low pulls PC1 low through board capacitance/parasitic paths, matrix sense sampling on PC1 will falsely register button presses if PC3 is active.
- **Alternatives Considered**: Reading buttons during active LED phases with software compensation (proved unreliable and prone to ghosting).

### 3. ADC Sampling with Settling Delay and Peak Capture
- **Decision**: When switching HCF4051 channels (PB2, PB3, PC4), allow a 5 µs settling delay before triggering the ADC conversion on PA4 and PA6. When a pad exceeds the noise threshold, track the peak ADC value over a 3 ms window before generating a MIDI `Note-On`.
- **Rationale**: Analog multiplexers and piezo/velostat pads have capacitive lag. Peak capture prevents premature low-velocity trigger on pad impact strike.

### 4. Table-Driven Gray Code Rotary Decoder
- **Decision**: Index an 8-state transition table with `(previous_state << 2) | current_state`.
- **Rationale**: Eliminates mechanical contact bounce, double-stepping, and back-stepping without arbitrary delay loops.

## Risks / Trade-offs

- **[Risk] ADC scan latency under high sampling rates**: Scanning 16 pads too fast could starve CPU cycles.
  - *Mitigation*: Dual-multiplexer simultaneous sampling on ADC ch3 and ch4 halves the required multiplexer cycle steps from 16 to 8.
- **[Risk] Pin interference between PC3 (LED phase 2) and PC1 (Button sense)**:
  - *Mitigation*: Hard guarantee in scheduler that `PORTC.OUT` clears phase bits before reading `PORTC.IN`.
- **[Risk] Memory corruption of JieLi bootloader partition**:
  - *Mitigation*: Strict linker script layout matching the flash address space (`0x1E00120` VMA / app partition).
