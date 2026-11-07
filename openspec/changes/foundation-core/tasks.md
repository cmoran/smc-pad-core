## 1. Project Skeleton & Core Types

- [x] 1.1 Establish C99 directory structure (`include/`, `src/`, `test/`) with freestanding headers
- [x] 1.2 Define hardware memory definitions and pinout constants citing JieLi register addresses
- [x] 1.3 Implement statically allocated SPSC lock-free ring buffer in `include/ring_buffer.h` and `src/ring_buffer.c`
- [x] 1.4 Write unit test harness for ring buffer verification (enqueue, dequeue, full/empty boundaries)

## 2. Encoder Gray Code Engine

- [x] 2.1 Implement Gray code state transition lookup table in `include/encoder.h` and `src/encoder.c`
- [x] 2.2 Implement 4-bank 32-virtual-encoder mapping and bounds clamping logic
- [x] 2.3 Write host unit tests verifying zero skipped states and bounce rejection

## 3. Matrix & LED Multiplexing Architecture

- [x] 3.1 Implement shift register clocking driver for 74HC595 outputs in `include/matrix.h` and `src/matrix.c`
- [x] 3.2 Implement integrator-based debouncer for 10 function buttons
- [x] 3.3 Implement 3x4 button-LED phase-segment multiplexer with non-interfering time-slicing (disabling phases during PC1 reads)
- [x] 3.4 Write host unit tests for matrix bit-scanning and debouncing state machine

## 4. Dual-Mux ADC & Velocity Engine

- [x] 4.1 Implement HCF4051 8-channel selection driver with 5 µs settling delay in `include/pad_adc.h` and `src/pad_adc.c`
- [x] 4.2 Implement dual-channel ADC sampling for Channel 3 (PA4) and Channel 4 (PA6)
- [x] 4.3 Implement peak-detection sampling window (3 ms) and release hysteresis
- [x] 4.4 Implement precomputed velocity curve tables (Linear, Soft, Hard, Fixed) in `include/velocity_curve.h` and `src/velocity_curve.c`
- [x] 4.5 Write host unit tests verifying curve calculations and threshold transitions

## 5. Build System & Simulation Harness

- [x] 5.1 Create Makefile supporting both host native simulation/tests (gcc/clang) and cross-compilation target rules
- [x] 5.2 Validate end-to-end simulated pipeline test running on host
