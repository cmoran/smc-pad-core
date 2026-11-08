# rgb-led Specification

## Purpose
TBD - created by archiving change 2026-11-07-rgb-led-and-runtime-midi. Update Purpose after archive.
## Requirements
### Requirement: WS2812 SPI Frame Encoding
The driver SHALL encode 24-bit RGB colors (GRB order) into 8-bit SPI patterns at 6 MHz, where binary 1 is represented by `0xFC` and binary 0 is represented by `0xC0`.

#### Scenario: Encoding standard color
- **GIVEN** an RGB color of `0x00FF00` (pure green)
- **WHEN** the color is converted into SPI transmission bytes
- **THEN** the green channel emits 8 bytes of `0xFC`, the red channel emits 8 bytes of `0xC0`, and the blue channel emits 8 bytes of `0xC0`.

### Requirement: Daisy-Chain Pad Mapping
The driver SHALL translate logical pad indices (0..15) to physical LED daisy-chain indices using the hardware mapping table `{11, 4, 2, 7, 10, 14, 6, 9, 0, 5, 15, 1, 13, 12, 8, 3}`.

#### Scenario: Setting color on physical Pad 0
- **GIVEN** logical Pad 0 is configured with color `0xFF0000`
- **WHEN** the frame buffer is assembled
- **THEN** physical LED chain index 11 is assigned `0xFF0000`.

### Requirement: Reset Gap and Frame Transmission
The driver SHALL append a trailing zero low-voltage latch pulse of at least 200 µs (162 zero bytes at 6 MHz) to latch the transmitted color data into the WS2812/SK6812 controllers.

#### Scenario: Frame latching
- **GIVEN** 16 pad colors are formatted into the DMA buffer
- **WHEN** the buffer is transmitted via SPI1 DMA
- **THEN** all 16 LEDs update simultaneously upon completion of the trailing reset gap without flickers or color drift.

### Requirement: Per-Bank RGB Color Palettes
The RGB LED subsystem SHALL support distinct resting (idle) and active (strike) color palettes for each of the 4 virtual banks, refreshing the entire 4x4 pad matrix immediately when the active bank changes.

#### Scenario: Switching pad bank refreshes palette
- **GIVEN** Bank 0 uses Dim Blue idle color and Bank 1 uses Dim Amber idle color
- **WHEN** the user cycles active Pad Bank from Bank 0 to Bank 1
- **THEN** all 16 pads on the matrix immediately transition to the Dim Amber idle color.

