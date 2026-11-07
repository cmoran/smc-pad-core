## 1. Native Toolchain Setup

- [x] 1.1 Implement `tools/setup_toolchain.sh` with curl, sha256sum verification, and extraction logic
- [x] 1.2 Test and run `tools/setup_toolchain.sh` to install the toolchain into `tools/jl-toolchain` and `tools/jl-postbuild`

## 2. Docker Build Container

- [x] 2.1 Implement `docker/Dockerfile` based on `debian:bookworm-slim` with build dependencies and toolchain setup
- [x] 2.2 Implement `docker/run_docker_build.sh` script providing the exact command line for running the Docker build

## 3. Target Firmware Entry & Build Rules

- [x] 3.1 Implement target entry point in `src/app_main.c` (Timer1 1 kHz ISR, GPIO initialization, bootloader counter handshake)
- [x] 3.2 Update `Makefile` with `make firmware` rule using `pi32v2-elf-gcc` and post-build packaging
- [x] 3.3 Validate target compilation and generate `build/app.bin`

## 4. Documentation

- [x] 4.1 Write comprehensive `docs/BUILDING.md` covering Native and Docker workflows, plus DFU flashing steps
