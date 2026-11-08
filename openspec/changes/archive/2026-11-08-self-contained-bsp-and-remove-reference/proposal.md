## Why

The `reference/open-smc-pad` directory is 1.4 GB in size, containing the entire third-party JieLi SDK, git history, and intermediate files. The actual target build only needs ~2 MB of link assets (the linker script, packaging tool, firmware template header, and low-level runtime core). Keeping this 1.4 GB dependency bloats the repository and violates the clean-slate architecture principle.

## What Changes

- **Create Self-Contained BSP Directory (`bsp/`):**
  - Copy `sdk.ld`, `app_template.bin`, `smcpad_core.o`, and `libsmcpad_native.a` into `bsp/`.
- **Integrate Packaging Tool Source into `tools/`:**
  - Copy `smcpad_pack.c` into `tools/` and update `Makefile` to compile `build/smcpad_pack` automatically from source on the host.
- **Update Build System (`Makefile`):**
  - Direct all linker script, template, runtime library, and pack tool paths to `bsp/` and `tools/`.
- **Remove 1.4 GB `reference/` Directory:**
  - Completely delete `reference/open-smc-pad/`.
- **Update Documentation:**
  - Update `docs/BUILDING.md` and `README.md` to reflect the self-contained build structure.

## Capabilities

### Modified Capabilities
- `toolchain-build`: The target firmware build system MUST be fully self-contained using `bsp/` and `tools/` without any reference or dependency on the `reference/` directory.

## Impact

- `Makefile`: References to `reference/open-smc-pad/...` replaced with `bsp/...` and `tools/...`.
- `tools/smcpad_pack.c`: Host packager source added and built into `build/smcpad_pack`.
- `bsp/`: Added linker script and precompiled AC6966C core runtime libraries (~2 MB total).
- `reference/`: 1.4 GB directory removed.
