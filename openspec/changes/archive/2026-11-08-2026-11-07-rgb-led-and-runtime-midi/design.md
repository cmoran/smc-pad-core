# Design: Pad RGB LED Driver & Runtime MIDI Integration

## Hardware Interfaces & Registers

### 1. SPI1 Pad LED Pipeline
* **Pin:** `PB1` (Port B Pin 1), mapped to SPI1 Data Out (DO) on Port A (`JL_IOMAP->CON1 &= ~BIT(4)`).
* **Clock Configuration:**
  * Base clock: `clk_get("spi")` (or `clk_get("lsb")`).
  * Desired baud rate: `6,000,000` Hz (6 MHz).
  * Divider register: `SPI1_BAUD = clk / 6000000 - 1`.
  * Control register: `SPI1_CON = 0x4021` (Master mode, 8-bit, MSB first, DMA enabled).
* **WS2812 Timing via SPI Byte Encoding:**
  * SPI Bit Period: $1 / 6\text{ MHz} \approx 166.7\text{ ns}$.
  * Byte length: $8 \times 166.7\text{ ns} \approx 1.33\ \mu\text{s}$ (matches WS2812 $1.25\ \mu\text{s} \pm 150\text{ ns}$ cycle).
  * Binary `0`: `0xC0` (`11000000b`) $\to$ High $333\text{ ns}$, Low $1000\text{ ns}$.
  * Binary `1`: `0xFC` (`11111100b`) $\to$ High $1000\text{ ns}$, Low $333\text{ ns}$.
* **DMA Buffer Layout:**
  * Total size: 548 bytes statically allocated, 4-byte aligned.
  * Prefix: 2 zero bytes.
  * 16 LEDs $\times$ 24 bytes = 384 bytes (GRB order).
  * Trailing reset gap: $548 - 386 = 162$ zero bytes ($162 \times 8 / 6\text{ MHz} = 216\ \mu\text{s}$ low pulse, exceeding WS2812/SK6812 $50\ \mu\text{s}$ reset latch requirement).

### 2. Pad Daisy-Chain Mapping
The LEDs are wired in a physical serpentine daisy chain across the 4x4 matrix:
```c
static const uint8_t pad_to_led_map[16] = {
    11,  4,  2,  7,
    10, 14,  6,  9,
     0,  5, 15,  1,
    13, 12,  8,  3
};
```

### 3. Runtime MIDI Application Pipeline
```
Timer / Systick (1 kHz)
   │
   ├──> smc_matrix_scan_step()     [16 columns, 10 buttons, 8 encoders]
   │       └── Push Button / Encoder events to Ring Buffer
   │
   ├──> smc_pad_adc_scan_all()     [Dual 4051 ADC sampling, PA4/PA6]
   │       └── Dynamic Baseline filter + Strike Window
   │       └── On Strike: smc_velocity_calculate() -> Push Note-On
   │       └── On Release: Push Note-Off
   │
Main Loop:
   ├──> USB Task (tud_task())
   ├──> smc_ring_buffer_pop()
   │       ├── Note-On: tud_midi_stream_write() + smc_rgb_led_set(pad, COLOR_HIT)
   │       ├── Note-Off: tud_midi_stream_write() + smc_rgb_led_set(pad, COLOR_IDLE)
   │       └── CC: tud_midi_stream_write()
   └──> smc_rgb_led_update() (if dirty)
```

## Static Memory Allocations
* RGB LED DMA Buffer: `uint8_t smc_rgb_dma_buf[548] __attribute__((aligned(4)));`
* Pad Color State Table: `uint32_t smc_pad_colors[16];`
* Zero dynamic allocations (`malloc`/`free` strictly absent).
