# SMC-Pad Clean-Slate Open Firmware - Docker Build Environment
# Platform: linux/amd64 (JieLi toolchain binaries are x86_64 ELF)
FROM --platform=linux/amd64 debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        make bash coreutils file xxd python3 ca-certificates curl xz-utils \
    && rm -rf /var/lib/apt/lists/*

ENV TOOLCHAIN_URL="https://jl-update.oss-cn-shenzhen.aliyuncs.com/jieli-linux-toolchains-20260730.1.tar.xz" \
    TOOLCHAIN_SHA256="a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd" \
    POSTBUILD_URL="https://jl-update.oss-cn-shenzhen.aliyuncs.com/jieli-linux-post-build-tools-20260923.1.tar.xz" \
    POSTBUILD_SHA256="6949d48c4cd82e274d7943d4ca67f214593fdb4f699ef9f51c2ecc78125ab739"

WORKDIR /opt
RUN mkdir -p /opt/jl-toolchain /opt/jl-postbuild \
    && curl -fSL -o tc.tar.xz "$TOOLCHAIN_URL" \
    && echo "$TOOLCHAIN_SHA256 tc.tar.xz" | sha256sum -c \
    && curl -fSL -o pb.tar.xz "$POSTBUILD_URL" \
    && echo "$POSTBUILD_SHA256 pb.tar.xz" | sha256sum -c \
    && tar -xJf tc.tar.xz -C /opt/jl-toolchain \
    && tar -xJf pb.tar.xz -C /opt/jl-postbuild \
    && rm -f tc.tar.xz pb.tar.xz

# Add pi32v2 compiler to PATH
ENV PATH="/opt/jl-toolchain/bin:/opt/jl-postbuild/bin:${PATH}"

WORKDIR /work
CMD ["make", "firmware"]
