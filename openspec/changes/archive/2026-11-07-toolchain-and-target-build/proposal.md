## Why

To execute the clean-slate SMC-Pad firmware on the physical JieLi AC6966C (BR25 / pi32v2) SoC, contributors need deterministic cross-compilation toolchains. Providing both a native Linux x86_64 toolchain installer and an isolated Docker build environment gives developers flexibility while guaranteeing reproducible ELF and binary output without host environment contamination.

## What Changes

- Add native toolchain downloader and verifier script (`tools/setup_toolchain.sh`) that fetches the official JieLi `pi32v2` Linux toolchain with SHA-256 integrity validation.
- Add Docker build configuration (`docker/Dockerfile` and `docker/run_docker_build.sh`) allowing containerized compilation.
- Update `Makefile` with `make firmware` (native) and target compilation rules linking our core drivers with target entry point `src/app_main.c`.
- Add documentation (`docs/BUILDING.md`) detailing prerequisites, native workflow, Docker workflow, and build artifacts.

## Capabilities

### New Capabilities
- `toolchain-build`: Cross-compilation toolchain environment (native and Docker), target compilation rules, and firmware packaging for JieLi AC6966C.

### Modified Capabilities
<!-- None -->

## Impact

- **Affected Systems**: `Makefile`, `tools/`, `docker/`, `docs/`, `src/app_main.c`.
- **Target Hardware**: JieLi AC6966C (pi32v2 core).
- **Host Requirements**: Linux x86_64, `tar`, `xz`, `curl` or `wget`, optional Docker for containerized workflow.
