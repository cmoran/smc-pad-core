## Why

The M-VAVE SMC-Pad hardware features an onboard Bluetooth Low Energy (BLE) radio (JieLi AC6966C / BR25 SoC) designed to enable wireless MIDI transmission to macOS, iOS, Android, Linux, and Windows hosts. While the clean-slate firmware already includes the Apple/MMA BLE-MIDI framing protocol engine (`ble_midi.c`), physical Bluetooth radio advertising, connection state management, the physical BT toggle button, and visual connection status via LED 5 need to be hooked up to the hardware runtime.

## What Changes

- Bind physical Button 0 (`BTN_SHIFT` / BT index) to toggle Bluetooth radio enable / advertising on and off.
- Integrate JieLi BSP BLE stack functions (`smcpad_ble_begin`, `smcpad_ble_advertise`, `smcpad_ble_connection`, `smcpad_ble_subscribed`, `smcpad_ble_mtu`, `smcpad_ble_notify`) into `src/app_main.c`.
- Connect the BLE-MIDI engine transmit callback to `smcpad_ble_notify` to send timestamped MIDI packets over BLE notifications.
- Synchronize BLE connection state, subscription state, and MTU dynamically into `g_ble_engine`.
- Drive Button LED 5 (`LED_BLUETOOTH`):
  - Slow blink (e.g. 500 ms interval) when Bluetooth is enabled and advertising (waiting for host central connection).
  - Solid on when a host central is connected and subscribed.
  - Off when Bluetooth is disabled or disconnected with advertising off.

## Capabilities

### New Capabilities
<!-- No new capabilities; modifying existing ble-midi capability -->

### Modified Capabilities
- `ble-midi`: Adds physical radio toggle via Button 0, LED 5 visual status indication (blinking advertising, solid connected, off disabled), and hardware stack hookup for live wireless MIDI transmission.

## Impact

- `src/app_main.c`: Hook up BLE BSP routines, link BLE notify callback, handle BT button events, update BLE connection/subscription state, and drive LED 5.
- Zero breaking changes to USB-MIDI, Pad ADC, Encoders, or SysEx protocol engines.
- `make test` and `make firmware` compile cleanly.
