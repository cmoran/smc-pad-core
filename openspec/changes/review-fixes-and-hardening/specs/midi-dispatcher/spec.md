# Delta Spec: MIDI Dispatcher

## ADDED Requirements

### Requirement: Per-Pad Pressure Mode Selection
`EVENT_PAD_PRESSURE` MUST honor per-pad `pressure_mode` from the active configuration even while the pad is actively sounding.

#### Scenario: Pad configured for Polyphonic Key Pressure
- **Given** a pad with `pressure_mode = 1` in `smc_config_t` and global `dispatcher->pressure_mode = 0`
- **When** the pad is struck and subsequently receives `EVENT_PAD_PRESSURE`
- **Then** the dispatcher emits Polyphonic Key Pressure (`0xA0 | ch, note, pressure`)

### Requirement: Pad Toggle Trigger Mode
Pads configured with `mode = 1` (Toggle) MUST latch Note-On on the first strike, ignore physical release, and emit Note-Off on the second strike.

#### Scenario: Toggling a latched pad
- **Given** a pad with `mode = 1`
- **When** the pad is struck, released, and struck a second time
- **Then** the first strike emits Note-On, the release emits nothing, and the second strike emits Note-Off

### Requirement: Spec-Compliant MIDI RX Stream Parsing
The MIDI RX parser MUST process status bytes that abort SysEx and MUST clear running status on System Common messages (`0xF1..0xF6`).

#### Scenario: Note-On interrupting an incomplete SysEx stream
- **Given** an active SysEx stream without `0xF7`
- **When** `0x90, 60, 100` arrives
- **Then** SysEx is aborted and `host_note_cb` receives Note 60, Velocity 100
