## 1. Setup Self-Contained BSP & Tools

- [x] 1.1 Create bsp/ directory and copy sdk.ld, app_template.bin, smcpad_core.o, and libsmcpad_native.a
- [x] 1.2 Copy smcpad_pack.c into tools/smcpad_pack.c

## 2. Makefile & Build Integration

- [x] 2.1 Add rule in Makefile to compile build/smcpad_pack from tools/smcpad_pack.c using host compiler
- [x] 2.2 Update target link and packaging paths in Makefile to reference bsp/ and tools/

## 3. Verify & Remove Reference Folder

- [x] 3.1 Run make firmware and make test to verify builds succeed
- [x] 3.2 Remove the 1.4 GB reference/ directory
- [x] 3.3 Re-verify make firmware and make test from a clean state
