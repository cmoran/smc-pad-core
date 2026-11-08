## Context

The repository currently relies on `reference/open-smc-pad/` for 5 specific files during firmware linking and packaging:
1. `sdk.ld` (Linker script)
2. `app_template.bin` (Firmware binary header template)
3. `smcpad_core.o` (JieLi BR25 SoC runtime core)
4. `libsmcpad_native.a` (JieLi native static library)
5. `smcpad_pack` (Host ELF-to-DFU packager tool)

The reference directory is 1.4 GB because it includes the entire vendor SDK and full git history.

## Goals / Non-Goals

**Goals:**
- Eliminate the 1.4 GB `reference/` directory completely.
- Make the repository self-contained by placing the ~2 MB of link assets in `bsp/`.
- Compile `smcpad_pack` on-the-fly from C source in `tools/smcpad_pack.c` during `make firmware`.
- Preserve 100% build reproducibility and flash compatibility.

**Non-Goals:**
- Rewriting JieLi proprietary Bluetooth controller and low-level USB PHY drivers at this time.

## Decisions

### 1. Dedicated `bsp/` Directory
- **Choice:** Create `bsp/` containing `sdk.ld`, `app_template.bin`, `smcpad_core.o`, and `libsmcpad_native.a`.
- **Rationale:** Separates board-level vendor linking artifacts from project source code (`src/` and `include/`).

### 2. Host Pack Tool Compiled from Source
- **Choice:** Store `tools/smcpad_pack.c` and compile it with `$(HOST_CC)` into `build/smcpad_pack`.
- **Rationale:** Avoids committing precompiled host binaries, ensuring portability across Linux distributions (x86_64, aarch64).

## Risks / Trade-offs

- **[Risk]** Removing `reference/` loses historical reference files for hardware register debugging.
  - **Mitigation:** All hardware registers, GPIOs, ADC multiplexers, and boot addresses have already been documented and codified in `include/smc_hw.h` and the test suite.
