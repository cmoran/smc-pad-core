# Proposal: Pad RGB LED Driver & Runtime MIDI Application Integration

## Problem Statement
1. **RGB LED Clocking Bug:** In the reference firmware (`open-smc-pad`), the SPI1 baud rate was calculated as `clk / (2 * LED_SPI_HZ) - 1`, halving the SPI clock to ~3 MHz instead of 6 MHz. This caused zero-bit pulses (`0xC0`) to stretch from 333 ns to 667 ns, triggering WS2812/SK6812 '1'-bit thresholds and corrupting colors (e.g. green turning into cyan) while causing downstream daisy-chain LEDs to drop out.
2. **Missing Active MIDI Runtime:** The device is currently running the interactive diagnostic bring-up console (`smcpad_bringup.c`), which only listens for serial commands over `/dev/ttyACM0`. It does not execute an active scanning and MIDI dispatch loop. As a result, DAWs and soft synths (such as Surge XT) receive no MIDI input, and buttons/encoders/pads do not produce user feedback.

## Scope
1. **Pad RGB LED Driver (`rgb_led`):**
   * Accurate SPI1 baud rate configuration at 6 MHz using `clk / LED_SPI_HZ - 1`.
   * WS2812/SK6812 pulse generation via SPI DMA (`0xFC` for 1, `0xC0` for 0, GRB order, >= 200 µs low reset gap).
   * Hardware daisy-chain mapping (`stock_pad_to_led` table).
   * Local visual feedback states (idle pad colors vs active hit colors) and host control mode.
2. **Runtime Application Loop (`app_main` / `runtime`):**
   * Connect all implemented core subsystems:
     * Matrix scanning (16 columns, 10 buttons with debouncing, 8 encoders with Gray code state machine).
     * Dual HCF4051 ADC scanning (16 pads) with dynamic auto-zero baseline calibration and peak velocity calculation.
     * Velocity curve mapping (Linear, Soft, Hard, Fixed).
     * Button LED 3×4 time-sliced multiplexing.
     * Pad RGB LED updates.
     * MIDI dispatcher output routed to the USB-MIDI transmit endpoint (`tud_midi_stream_write` / `tud_midi_packet_write`).
   * Function button handling (Shift, Bank selection, Panic trigger).

## Non-Goals
* Internal groovebox sequencing, synthesizer audio generation, or arpeggiators (strict adherence to clean "dumb" controller tenet).
* BLE-MIDI pairing implementation in this specific change (USB-MIDI prioritized first; BLE will follow once USB pipeline is rock solid).
* Dynamic allocations (all buffers, state structs, and queues are strictly static).
