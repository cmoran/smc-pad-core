# Building SMC-Pad Clean-Slate Open Firmware

This guide covers building the SMC-Pad firmware for the JieLi AC6966C microcontroller using either the **Native Linux Toolchain** or the **Containerized Docker** environment.

---

## 1. Method A: Native Linux Toolchain (Recommended for Linux x86_64)

The native toolchain runs directly on your host machine without container overhead.

### Prerequisites
* Linux x86_64
* Standard build utilities: `make`, `curl`, `tar`, `xz-utils`, `sha256sum`
* A C99 host compiler (`gcc` or `clang`) for running unit tests

### Setup
Run the automated installer script:
```bash
./tools/setup_toolchain.sh
```
This script downloads the official JieLi `pi32v2` toolchain and post-build utilities from JieLi's servers, verifies their SHA-256 integrity, extracts them into `tools/jl-toolchain` and `tools/jl-postbuild`, and configures helper wrappers in `tools/bin/`.

### Compiling
* **Run Host Unit Tests & Simulation:**
  ```bash
  make test
  ```
* **Build Target Microcontroller Objects (pi32v2):**
  ```bash
  make firmware
  ```

---

## 2. Method B: Docker Container Build

If you prefer not to download the toolchain into your local workspace, or if you are on macOS / ARM64 Linux, you can compile inside an isolated Docker container with `linux/amd64` emulation.

### Prerequisites
* Docker installed and running

### Step 1: Build the Container Image
```bash
docker build -t smc-pad-builder -f docker/Dockerfile .
```

### Step 2: Compile the Target Firmware
Mount your workspace volume and trigger the build:
```bash
docker run --rm -v "$(pwd):/work" smc-pad-builder make firmware
```
Or execute the helper script:
```bash
./docker/run_docker_build.sh
```

---

## 3. Flashing to Physical Hardware

### Connecting the SMC-Pad
Connect the SMC-Pad to your PC via USB. Verify that Linux detects the device:
```bash
lsusb | grep -i jieli
# Output: ID 4353:4b4d Jieli Technology SINCO
```

### Initial Bootloader Installation (From Stock Firmware)
To install the non-destructive USB DFU bootloader without opening the controller:
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install mido python-rtmidi
python3 tools/stock_installer/install_from_stock.py
```
* The script communicates with the stock firmware over USB-MIDI, installs the bootloader into free flash, and prompts you to power-cycle the controller once.
* Upon reboot, the SMC-Pad will enumerate as standard USB DFU (`1209:0002`).

### Fast Firmware Updates (via USB DFU)
Once the DFU bootloader is installed, flash new builds in ~14 seconds using `dfu-util`:
```bash
dfu-util -d 1209:0002 -a 0 -D build/app.bin
```
