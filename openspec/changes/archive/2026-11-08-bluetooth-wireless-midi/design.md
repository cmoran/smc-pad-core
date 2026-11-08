## Context

The M-VAVE SMC-Pad controller contains a JieLi AC6966C (BR25) SoC equipped with a Bluetooth Low Energy 5.0 radio transceiver. The low-level BLE peripheral stack and GATT server implementation reside within `bsp/smcpad_core.o`.

In our clean-slate firmware:
- `include/ble_midi.h` and `src/ble_midi.c` implement the standard Apple/MMA BLE-MIDI packet framing (13-bit timestamps, packet chunking, and dual-output dispatch).
- However, physical radio activation, GATT profile registration, advertising control, and hardware notification transmission have not been bound into `src/app_main.c`.
- Physical Button 0 (panel order: Bluetooth) and physical button LED 0 (Bluetooth LED) are currently unhandled in `app_main.c`.

## Goals / Non-Goals

**Goals:**
- Provide full hardware Bluetooth Low Energy MIDI capability adhering to the Apple/MMA MIDI over Bluetooth LE specification.
- Expose physical Button 0 as the Bluetooth toggle (enabling advertising / turning off radio).
- Register the standard BLE-MIDI 128-bit Service UUID (`03B80E5A-EDE8-4B33-A751-6CE34EC4C700`) and Characteristic UUID (`7772E5DB-3868-4112-A1A9-F2669D106BF3`) using a static 180-byte ATT database.
- Drive physical Button LED 0 (`smcpad_led_set(0, ...)`):
  - Blink at 500 ms period when advertising (pairing mode, waiting for host).
  - Solid ON when host central is connected and subscribed.
  - OFF when Bluetooth is disabled or advertising is stopped.
- Connect `smcpad_ble_notify` to `g_ble_engine.send_fn` to transmit live MIDI messages wirelessly without dynamic memory allocation.

**Non-Goals:**
- BLE Central mode (the controller only operates as a BLE GATT Peripheral).
- Audio / A2DP (SMC-Pad clean-slate firmware is strictly a MIDI controller).

## Decisions

1. **Static BTstack ATT Database:**
   - *Choice:* Define the 180-byte ATT database as a static constant array in `src/app_main.c`.
   - *Rationale:* Zero runtime heap allocations (`malloc`/`free` strictly forbidden per firmware guardrails). The GATT profile is static and deterministic across all device runs.

2. **Button 0 for Bluetooth Power/Advertising Toggle:**
   - *Choice:* When Button 0 is pressed (`SMCPAD_EV_BUTTON_DOWN`, `hw_ev.index == 0`):
     - If Bluetooth is currently OFF:
       - Set device name to `"SMC-PAD"`.
       - Register callbacks (`read`, `write`, `event`) and profile via `smcpad_ble_begin()`, `smcpad_ble_set_profile()`, `smcpad_ble_set_adv()`.
       - Enable advertising via `smcpad_ble_advertise(1)`.
       - Mark `g_ble_active = true`.
     - If Bluetooth is currently ON:
       - Stop advertising via `smcpad_ble_advertise(0)`.
       - Disconnect if connected via `smcpad_ble_disconnect()`.
       - Mark `g_ble_active = false`.
       - Turn LED 0 off.

3. **LED 0 Indication State Machine:**
   - *Choice:* In `loop()`:
     - If `!g_ble_active`: `smcpad_led_set(0, 0)`.
     - Else if `smc_ble_midi_is_subscribed(&g_ble_engine)` (or `smcpad_ble_connection() && smcpad_ble_subscribed(12)`):
       - `smcpad_led_set(0, 1)` (Solid ON).
     - Else (advertising / waiting for connection):
       - Blink LED 0 with 500 ms period (`(app_get_millis() / 500) & 1`).

4. **Dual Dispatch Integration:**
   - *Choice:* `hal_ble_raw_send(const uint8_t *data, uint16_t len)` calls `smcpad_ble_notify(12, data, len)`.
   - *Rationale:* Characteristic handle 12 is the MIDI I/O value handle in our ATT database. Outgoing messages formatted by `ble_midi.c` are passed directly to `smcpad_ble_notify`.

## Risks / Trade-offs

- [Risk] GATT notification flood if radio TX queue overflows.
  → *Mitigation:* `ble_midi.c` checks MTU limit, and `app_main.c` checks `smcpad_ble_queued()` to ensure headroom before pushing packets.
- [Risk] Unsubscribed notifications dropping.
  → *Mitigation:* `smc_ble_midi_t` suppresses transmission until the host writes 0x0001 to the CCC descriptor (handle 13).
