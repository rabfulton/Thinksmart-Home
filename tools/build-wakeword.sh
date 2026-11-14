#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
./tools/fetch-wakeword.py
docker build -t thinksmart-audio-builder:debian13 -f tools/Dockerfile.audio tools
docker run --rm --network none --user "$(id -u):$(id -g)" --mount type=bind,src="$PWD",dst=/workspace \
  thinksmart-audio-builder:debian13 sh -ec '
    python3 tools/prepare-wakeword.py
    cmake -S app/audio -B build/wake-native -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DTHINKSMART_SHERPA_ROOT=/workspace/build/wakeword/x64/sherpa-onnx-v2.0.1-linux-x64-shared-lib \
      -DTHINKSMART_SHERPA_INCLUDE=/workspace/build/wakeword/include
    cmake --build build/wake-native --target thinksmart-wakeword --parallel 4
    cmake -S app/audio -B build/wake-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm64.cmake -DCMAKE_BUILD_TYPE=Release \
      -DTHINKSMART_SHERPA_ROOT=/workspace/build/wakeword/aarch64/sherpa-onnx-v2.0.1-linux-aarch64-shared-cpu-lib \
      -DTHINKSMART_SHERPA_INCLUDE=/workspace/build/wakeword/include
    cmake --build build/wake-arm64 --target thinksmart-wakeword --parallel 4
  '
# Optional private recordings are not part of the public source export.
fixtures=${THINKSMART_WAKE_FIXTURES:-private/wakeword/fixtures}
if [[ -f $fixtures/sheila.wav && -f $fixtures/unrelated.wav ]]; then
    for fixture in sheila unrelated; do
        ffmpeg -v error -y -i "$fixtures/$fixture.wav" -af adelay=1000 -ar 16000 -ac 1 -f s16le "build/wakeword/$fixture.s16"
    done
    docker run --rm --network none --user "$(id -u):$(id -g)" --mount type=bind,src="$PWD",dst=/workspace \
      thinksmart-audio-builder:debian13 python3 tools/check-wakeword.py
else
    echo 'Wake runtime built. Supply private fixtures or verify local Sheila detection on the device.'
fi
