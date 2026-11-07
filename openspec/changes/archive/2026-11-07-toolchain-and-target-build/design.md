## Context

The JieLi AC6966C microcontroller uses JieLi's proprietary 32-bit CPU core architecture (`pi32v2`). Official compilation requires the `pi32v2-elf-*` toolchain and post-build packaging utilities. Developers need dual build avenues: a fast native setup for Linux x86_64 environments and an isolated Docker environment for zero-host-dependency builds.

## Goals / Non-Goals

**Goals:**
- **Automated Native Toolchain Setup**: A self-contained script (`tools/setup_toolchain.sh`) that fetches, validates (SHA-256), and unpacks the official `pi32v2` toolchain and post-build tools into `tools/jl-toolchain` and `tools/jl-postbuild`.
- **Reproducible Docker Build Container**: A lightweight Debian-based `Dockerfile` with the toolchain and dependencies, with helper script `docker/run_docker_build.sh`.
- **Target Makefile Integration**: `make firmware` and `make TARGET=jieli` rules that invoke `pi32v2-elf-gcc` with `-std=c99 -Oz` and link at `0x1E08120`.
- **Target Entry Point**: `src/app_main.c` providing hardware initialization, Timer1 1 kHz ISR scheduling, and bootloader crash-counter clearing (`0x31AD8`).
- **Comprehensive Documentation**: `docs/BUILDING.md` documenting both Native and Docker build paths.

**Non-Goals:**
- Rebuilding the JieLi compiler from source (proprietary toolchain; official vendor binaries used).

## Decisions

### 1. Vendor Tarball Verification
- **Decision**: Hardcode official SHA-256 hashes in `tools/setup_toolchain.sh`:
  - Toolchain (`a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`)
  - Post-build tools (`6949d48c4cd82e274d7943d4ca67f214593fdb4f699ef9f51c2ecc78125ab739`)
- **Rationale**: Guarantees bit-for-bit supply chain security before executing any downloaded binaries.

### 2. Docker Workspace Mounting
- **Decision**: The Docker container mounts the project workspace as a volume (`-v $(pwd):/work`) and runs non-root where feasible.
- **Rationale**: Keeps build artifacts in the local workspace without requiring files to be committed inside image layers.

## Risks / Trade-offs

- **[Risk] Host architecture non-x86_64 (e.g. Apple Silicon or ARM64 Linux)**:
  - *Mitigation*: The Docker image is specified with `--platform=linux/amd64` using QEMU emulation so ARM64 developers can build transparently.
