## 1. RGB Driver Enhancements

- [x] 1.1 Implement smc_rgb_led_render_shift_status in src/rgb_led.c and declare in include/rgb_led.h
- [x] 1.2 Implement smc_rgb_led_boot_animation in src/rgb_led.c and declare in include/rgb_led.h
- [x] 1.3 Add unit test in test/test_rgb_led.c verifying boot animation sequence and shift status layout colors

## 2. Shift Status Dashboard Integration

- [x] 2.1 Update src/app_main.c on Shift Button 8 down to display Shift status view
- [x] 2.2 Update src/app_main.c on Shift Button 8 up to restore active bank palette
- [x] 2.3 Refresh Shift status view whenever bank, curve, or transpose changes while Shift is held

## 3. Boot Animation Hardware Integration

- [x] 3.1 Call boot animation sequence in setup() before setting initial resting bank colors
- [x] 3.2 Verify clean delay timing on target hardware and host simulation

## 4. Verification and Target Build

- [x] 4.1 Run host test suite (make test) and verify all test suites pass
- [x] 4.2 Compile target firmware (make firmware) and verify build/app.bin output
