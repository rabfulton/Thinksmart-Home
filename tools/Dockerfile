FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture arm64 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    ca-certificates curl git openssh-client python3 python3-venv \
    debootstrap qemu-user-static binfmt-support pigz fdisk e2fsprogs \
    systemd udev dmsetup nftables kmod util-linux file unzip xz-utils \
    meson ninja-build python3-mako python3-yaml python3-packaging bison flex \
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu pkg-config dpkg-dev binutils \
    libwayland-bin libwayland-dev libdrm-dev:arm64 zlib1g-dev:arm64 \
    libzstd-dev:arm64 libexpat1-dev:arm64 libwayland-dev:arm64 \
    libwayland-egl-backend-dev:arm64 libglvnd-dev:arm64 \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
CMD ["bash"]
