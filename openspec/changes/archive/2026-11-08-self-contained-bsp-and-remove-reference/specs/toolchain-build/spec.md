## MODIFIED Requirements

### Requirement: Target compilation and packaging
The build system SHALL provide Makefile target rules to compile all core modules with `pi32v2-elf-gcc` targeting JieLi AC6966C, linked at entry point `0x1E08120` using self-contained BSP link assets in `bsp/` and an automatically compiled host packager `tools/smcpad_pack.c`.

#### Scenario: Building flashable firmware image
- **WHEN** `make firmware` is run with the JieLi toolchain present
- **THEN** an ELF and packaged binary `build/app.bin` suitable for DFU flashing are generated without any dependency on external reference folders.
