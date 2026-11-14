#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
docker build -t thinksmart-builder:debian13 -f tools/Dockerfile tools
docker build -t thinksmart-qt-builder:debian13 -f tools/Dockerfile.qt tools
docker run --rm --user "$(id -u):$(id -g)" --mount type=bind,src="$PWD",dst=/workspace \
  thinksmart-qt-builder:debian13 sh -ec '
    cmake -S app -B build/home-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm64.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build build/home-arm64 --parallel 4
  '
