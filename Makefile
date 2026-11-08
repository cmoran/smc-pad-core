# SMC-Pad Clean-Slate Open Firmware Makefile
# Targets:
#   make test             - Run host unit tests & E2E simulation (gcc/clang)
#   make firmware         - Build target firmware (JieLi pi32v2 cross-compiler)
#   make docker-firmware  - Build target firmware inside Docker container
#   make clean            - Clean build artifacts

CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -std=c99 -pedantic -O2 -Iinclude -DSMC_HOST_TEST
BUILD_DIR ?= build

# Host sources and objects
HOST_SRCS := src/ring_buffer.c src/encoder.c src/matrix.c src/pad_adc.c src/velocity_curve.c src/midi_dispatcher.c src/rgb_led.c
HOST_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/host_%.o,$(HOST_SRCS))

HOST_TESTS := $(BUILD_DIR)/test_ring_buffer \
              $(BUILD_DIR)/test_encoder \
              $(BUILD_DIR)/test_matrix \
              $(BUILD_DIR)/test_pad_adc \
              $(BUILD_DIR)/test_midi_dispatcher \
              $(BUILD_DIR)/test_rgb_led \
              $(BUILD_DIR)/test_e2e_pipeline

# JieLi Target cross-compiler setup
JL_TC ?= $(firstword $(wildcard tools/jl-toolchain/jieli-linux-toolchains-*))
JL_PB ?= $(firstword $(wildcard tools/jl-postbuild/jieli-linux-post-build-tools-*))

JL_CC := $(JL_TC)/common/bin/clang
JL_CFLAGS := -target pi32v2 -mcpu=r3 -integrated-as -Oz -fno-common -ffunction-sections -fdata-sections \
             -mllvm -enable-ipra=false -isystem $(JL_TC)/pi32v2/include -std=c99 -Wall -Wextra -Iinclude

TARGET_SRCS := $(HOST_SRCS) src/app_main.c
TARGET_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/target_%.o,$(TARGET_SRCS))

.PHONY: all test firmware docker-firmware clean

all: test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Host Test Rules
$(BUILD_DIR)/host_%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_ring_buffer: test/test_ring_buffer.c $(BUILD_DIR)/host_ring_buffer.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_encoder: test/test_encoder.c $(BUILD_DIR)/host_ring_buffer.o $(BUILD_DIR)/host_encoder.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_matrix: test/test_matrix.c $(BUILD_DIR)/host_ring_buffer.o $(BUILD_DIR)/host_encoder.o $(BUILD_DIR)/host_matrix.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_pad_adc: test/test_pad_adc.c $(BUILD_DIR)/host_ring_buffer.o $(BUILD_DIR)/host_velocity_curve.o $(BUILD_DIR)/host_pad_adc.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_midi_dispatcher: test/test_midi_dispatcher.c $(BUILD_DIR)/host_ring_buffer.o $(BUILD_DIR)/host_encoder.o $(BUILD_DIR)/host_midi_dispatcher.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_rgb_led: test/test_rgb_led.c $(BUILD_DIR)/host_rgb_led.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_e2e_pipeline: test/test_e2e_pipeline.c $(HOST_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

test: $(HOST_TESTS)
	@echo "=== Running SMC-Pad Host Test Suite ==="
	@$(BUILD_DIR)/test_ring_buffer
	@$(BUILD_DIR)/test_encoder
	@$(BUILD_DIR)/test_matrix
	@$(BUILD_DIR)/test_pad_adc
	@$(BUILD_DIR)/test_midi_dispatcher
	@$(BUILD_DIR)/test_rgb_led
	@$(BUILD_DIR)/test_e2e_pipeline
	@echo "=== All Host Tests Passed Successfully ==="

# JieLi Target Firmware Rules
$(BUILD_DIR)/target_%.o: src/%.c | $(BUILD_DIR)
	@if [ ! -d "$(JL_TC)" ]; then \
		echo "Error: JieLi toolchain not found. Run ./tools/setup_toolchain.sh first." >&2; exit 1; \
	fi
	$(JL_CC) $(JL_CFLAGS) -c $< -o $@

firmware: $(TARGET_OBJS)
	@echo "=== Compiled all target modules for JieLi pi32v2 ==="
	@ls -lh $(TARGET_OBJS)
	@echo "All target objects generated successfully in $(BUILD_DIR)/."

docker-firmware:
	@echo "Running Docker build helper..."
	./docker/run_docker_build.sh

clean:
	rm -rf $(BUILD_DIR)
