#!/usr/bin/env bash
# SMC-Pad Native JieLi Toolchain Installer for Linux x86_64
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

TOOLCHAIN_URL="https://jl-update.oss-cn-shenzhen.aliyuncs.com/jieli-linux-toolchains-20260730.1.tar.xz"
TOOLCHAIN_SHA256="a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd"

POSTBUILD_URL="https://jl-update.oss-cn-shenzhen.aliyuncs.com/jieli-linux-post-build-tools-20260923.1.tar.xz"
POSTBUILD_SHA256="6949d48c4cd82e274d7943d4ca67f214593fdb4f699ef9f51c2ecc78125ab739"

TC_DIR="${ROOT_DIR}/tools/jl-toolchain"
PB_DIR="${ROOT_DIR}/tools/jl-postbuild"
BIN_DIR="${ROOT_DIR}/tools/bin"

mkdir -p "${TC_DIR}" "${PB_DIR}" "${BIN_DIR}"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TMP_DIR}"' EXIT

# Download if not already extracted
TC_CC="$(find "${TC_DIR}" -path "*/pi32v2/bin/cc" 2>/dev/null | head -n 1 || true)"
if [[ -z "${TC_CC}" ]]; then
    echo "=== Downloading JieLi pi32v2 Toolchain ==="
    curl -fSL -o "${TMP_DIR}/tc.tar.xz" "${TOOLCHAIN_URL}"
    echo "${TOOLCHAIN_SHA256}  ${TMP_DIR}/tc.tar.xz" | sha256sum -c

    echo "=== Extracting Toolchain ==="
    tar -xJf "${TMP_DIR}/tc.tar.xz" -C "${TC_DIR}"
fi

PB_BIN="$(find "${PB_DIR}" -name "isd_download" 2>/dev/null | head -n 1 || true)"
if [[ -z "${PB_BIN}" ]]; then
    echo "=== Downloading JieLi Post-Build Tools ==="
    curl -fSL -o "${TMP_DIR}/pb.tar.xz" "${POSTBUILD_URL}"
    echo "${POSTBUILD_SHA256}  ${TMP_DIR}/pb.tar.xz" | sha256sum -c

    echo "=== Extracting Post-Build Tools ==="
    tar -xJf "${TMP_DIR}/pb.tar.xz" -C "${PB_DIR}"
fi

# Locate binaries
CC_BIN="$(find "${TC_DIR}" -path "*/pi32v2/bin/cc" | head -n 1)"
LD_BIN="$(find "${TC_DIR}" -path "*/pi32v2/bin/ld" | head -n 1)"
OBJCOPY_BIN="$(find "${TC_DIR}" -path "*/common/bin/objcopy" | head -n 1)"
ISD_BIN="$(find "${PB_DIR}" -name "isd_download" | head -n 1)"
FWADD_BIN="$(find "${PB_DIR}" -name "fw_add" | head -n 1)"

# Create robust wrappers
cat <<EOF > "${BIN_DIR}/pi32v2-cc"
#!/usr/bin/env bash
exec "${CC_BIN}" "\$@"
EOF
chmod +x "${BIN_DIR}/pi32v2-cc"

cat <<EOF > "${BIN_DIR}/pi32v2-ld"
#!/usr/bin/env bash
exec "${LD_BIN}" "\$@"
EOF
chmod +x "${BIN_DIR}/pi32v2-ld"

cat <<EOF > "${BIN_DIR}/pi32v2-objcopy"
#!/usr/bin/env bash
exec "${OBJCOPY_BIN}" "\$@"
EOF
chmod +x "${BIN_DIR}/pi32v2-objcopy"

cat <<EOF > "${BIN_DIR}/isd_download"
#!/usr/bin/env bash
exec "${ISD_BIN}" "\$@"
EOF
chmod +x "${BIN_DIR}/isd_download"

cat <<EOF > "${BIN_DIR}/fw_add"
#!/usr/bin/env bash
exec "${FWADD_BIN}" "\$@"
EOF
chmod +x "${BIN_DIR}/fw_add"

echo "=== Toolchain Ready in ${BIN_DIR} ==="
"${BIN_DIR}/pi32v2-cc" --version
echo "Wrappers ready in ${BIN_DIR}."
