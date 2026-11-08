## 1. Declarations & BLE Peripheral Stack Bindings

- [x] 1.1 Add JieLi BSP BLE function prototypes and callback types to `src/app_main.c`
- [x] 1.2 Define static BTstack ATT database and advertising/scan response packets for Apple/MMA MIDI service

## 2. Event Dispatching & Hardware Control

- [x] 2.1 Wire physical Button 0 (`hw_ev.index == 0`) to toggle Bluetooth advertising and connection state
- [x] 2.2 Implement `hal_ble_raw_send` callback linking `g_ble_engine` to `smcpad_ble_notify` on characteristic handle 12
- [x] 2.3 Implement LED 0 feedback state machine in `loop()` for pairing blink, solid connected, and off states
- [x] 2.4 Handle incoming BLE-MIDI write events in GATT write callback

## 3. Verification & Validation

- [x] 3.1 Execute host unit test suite (`make test`)
- [x] 3.2 Build target firmware binary (`make firmware`) and verify binary packaging
