#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
docker build -t thinksmart-audio-builder:debian13 -f tools/Dockerfile.audio tools
docker run --rm --network none --user "$(id -u):$(id -g)" --mount type=bind,src="$PWD",dst=/workspace \
  thinksmart-audio-builder:debian13 sh -ec '
    cmake -S app/audio -B build/audio-native -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/audio-native --parallel 4
    build/audio-native/thinksmart-audio-lab --self-test
    cmake -S app/audio -B build/audio-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm64.cmake -DCMAKE_BUILD_TYPE=Release
    cmake --build build/audio-arm64 --parallel 4
    espeak-ng -v en-gb -s 145 -w build/audio-reference.wav "This is the ThinkSmart echo cancellation test. The speaker is playing a local test voice. Please speak naturally while this voice continues. You can say a sentence, count a few numbers, or describe the weather. The echo canceller should reduce the speaker sound while keeping your voice. This test uses no cloud service."
  '
ffmpeg -v error -y -i build/audio-reference.wav -ar 48000 -ac 1 -f s16le build/audio-reference.s16
