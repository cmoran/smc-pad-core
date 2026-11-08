## 1. Timing, BPM & Swing Engine

- [x] 1.1 Implement dynamic BPM interval calculation (30..300 BPM) in src/app_main.c
- [x] 1.2 Implement alternating swing shuffle calculation (50% to 70%) in src/app_main.c
- [x] 1.3 Implement Latch persistence state and toggle logic in src/app_main.c

## 2. Rotary Encoders Note Repeat Integration

- [x] 2.1 Add 2-detent step accumulator for Knob 1 (Rate) to eliminate rapid skipping
- [x] 2.2 Wire Knob 2 to Swing percentage with 2-detent step accumulator
- [x] 2.3 Wire Knob 3 to Tempo (BPM) adjustment (30..300)
- [x] 2.4 Wire Knob 4 to Latch toggle (On / Off)

## 3. Shift + Note Repeat 16-Pad Configuration Mode

- [x] 3.1 Map Pads 1-8 to Rate divisions with active LED highlight
- [x] 3.2 Map Pads 9-13 to Swing settings with active LED highlight
- [x] 3.3 Map Pad 14 to Latch toggle with active LED highlight
- [x] 3.4 Map Pad 16 to Tap Tempo with tap interval detection and pulse flash

## 4. Verification and Target Build

- [x] 4.1 Run host test suite (make test) and verify all tests pass
- [x] 4.2 Compile target firmware (make firmware) and verify build/app.bin output
