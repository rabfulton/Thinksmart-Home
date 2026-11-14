#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
docker build -t thinksmart-shared-audio-builder:debian13 -f tools/Dockerfile.shared-audio tools
docker run --rm --network none --user "$(id -u):$(id -g)" --mount type=bind,src="$PWD",dst=/workspace thinksmart-shared-audio-builder:debian13 sh -ec '
cmake -S app/audio -B build/shared-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm64.cmake -DCMAKE_BUILD_TYPE=Release -DTHINKSMART_SHERPA_ROOT=/workspace/build/wakeword/aarch64/sherpa-onnx-v2.0.1-linux-aarch64-shared-cpu-lib -DTHINKSMART_SHERPA_INCLUDE=/workspace/build/wakeword/include
cmake --build build/shared-arm64 --target thinksmart-shared-audio thinksmart-voice-audio --parallel 4
'
