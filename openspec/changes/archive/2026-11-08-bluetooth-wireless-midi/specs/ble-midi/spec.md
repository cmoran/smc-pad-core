## ADDED Requirements

### Requirement: Physical Bluetooth Toggle and Stack Binding
The firmware SHALL provide runtime activation and deactivation of the JieLi BLE peripheral stack and standard advertising via physical Button 0.

#### Scenario: Toggling Bluetooth on
- **GIVEN** Bluetooth is currently disabled
- **WHEN** physical Button 0 is pressed
- **THEN** the BLE controller initializes (`smcpad_ble_begin`), advertising is started (`smcpad_ble_advertise(1)`), and runtime state records Bluetooth as enabled.

#### Scenario: Toggling Bluetooth off
- **GIVEN** Bluetooth is currently enabled
- **WHEN** physical Button 0 is pressed
- **THEN** advertising is stopped (`smcpad_ble_advertise(0)`), the BLE-MIDI engine state is marked disconnected, and runtime state records Bluetooth as disabled.

### Requirement: Bluetooth Status Indicator LED
The firmware SHALL indicate Bluetooth radio state visually using button LED 5 (`LED_BLUETOOTH`).

#### Scenario: Advertising status indication
- **GIVEN** Bluetooth advertising is active and no host central is connected
- **WHEN** the main event loop executes
- **THEN** LED 5 flashes at a 500 ms interval to signal pairing mode.

#### Scenario: Connected status indication
- **GIVEN** a host central is connected and subscribed to the BLE-MIDI GATT characteristic
- **WHEN** the main event loop executes
- **THEN** LED 5 is illuminated solid ON.

#### Scenario: Disabled status indication
- **GIVEN** Bluetooth is disabled
- **WHEN** the main event loop executes
- **THEN** LED 5 is OFF.
