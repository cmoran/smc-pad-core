# toolchain-build Specification

## Purpose
TBD - created by archiving change toolchain-and-target-build. Update Purpose after archive.
## Requirements
### Requirement: Native toolchain setup script
The system SHALL provide a script `tools/setup_toolchain.sh` that downloads and unpacks the official JieLi `pi32v2` Linux toolchain and post-build tools with SHA-256 integrity verification.

#### Scenario: Clean native toolchain download and verification
- **WHEN** `tools/setup_toolchain.sh` is executed on a Linux x86_64 system
- **THEN** it downloads the vendor tarballs, verifies their SHA-256 checksums, extracts them into `tools/jl-toolchain/` and `tools/jl-postbuild/`, and tests the compiler binary with `--version`

### Requirement: Containerized Docker build environment
The repository SHALL provide a `docker/Dockerfile` and execution instructions allowing users to compile the firmware in an isolated environment.

#### Scenario: Docker container build execution
- **WHEN** a user builds the container image and runs `docker/run_docker_build.sh`
- **THEN** the target firmware `app.bin` is built inside the container and placed in the project `build/` directory

### Requirement: Target compilation and packaging
The build system SHALL provide Makefile target rules to compile all core modules with `pi32v2-elf-gcc` targeting JieLi AC6966C, linked at entry point `0x1E08120` using self-contained BSP link assets in `bsp/` and an automatically compiled host packager `tools/smcpad_pack.c`.

#### Scenario: Building flashable firmware image
- **WHEN** `make firmware` is run with the JieLi toolchain present
- **THEN** an ELF and packaged binary `build/app.bin` suitable for DFU flashing are generated without any dependency on external reference folders.

### Requirement: Dual-method build documentation
The repository SHALL include `docs/BUILDING.md` documenting prerequisites, native setup, Docker setup, and flashing procedures.

#### Scenario: Contributor following documentation
- **WHEN** a contributor opens `docs/BUILDING.md`
- **THEN** they find clear step-by-step instructions for both the Native Toolchain workflow and the Docker container workflow

