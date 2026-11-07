# SMC-Pad Clean-Slate Open Firmware Makefile
# Targets:
#   make test (Host unit tests & E2E simulation)
#   make clean

CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -std=c99 -pedantic -O2 -Iinclude
BUILD_DIR ?= build

SRCS := src/ring_buffer.c src/encoder.c src/matrix.c src/pad_adc.c src/velocity_curve.c
OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

TESTS := $(BUILD_DIR)/test_ring_buffer \
         $(BUILD_DIR)/test_encoder \
         $(BUILD_DIR)/test_matrix \
         $(BUILD_DIR)/test_pad_adc \
         $(BUILD_DIR)/test_e2e_pipeline

.PHONY: all test clean

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_ring_buffer: test/test_ring_buffer.c $(BUILD_DIR)/ring_buffer.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_encoder: test/test_encoder.c $(BUILD_DIR)/ring_buffer.o $(BUILD_DIR)/encoder.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_matrix: test/test_matrix.c $(BUILD_DIR)/ring_buffer.o $(BUILD_DIR)/encoder.o $(BUILD_DIR)/matrix.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_pad_adc: test/test_pad_adc.c $(BUILD_DIR)/ring_buffer.o $(BUILD_DIR)/velocity_curve.o $(BUILD_DIR)/pad_adc.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_e2e_pipeline: test/test_e2e_pipeline.c $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TESTS)
	@echo "=== Running SMC-Pad Core Test Suite ==="
	@$(BUILD_DIR)/test_ring_buffer
	@$(BUILD_DIR)/test_encoder
	@$(BUILD_DIR)/test_matrix
	@$(BUILD_DIR)/test_pad_adc
	@$(BUILD_DIR)/test_e2e_pipeline
	@echo "=== All SMC-Pad Core Tests Passed Successfully ==="

clean:
	rm -rf $(BUILD_DIR)
