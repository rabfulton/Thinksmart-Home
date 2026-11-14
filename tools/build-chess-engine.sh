#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
engine_revision=99adddfbc73c4f51c8f9c068439d0844863dd2e9
if [ ! -d build/arasan-src/.git ]; then
    git clone --no-checkout https://github.com/jdart1/arasan-chess.git build/arasan-src
    git -C build/arasan-src checkout --detach "$engine_revision"
fi
test "$(git -C build/arasan-src rev-parse HEAD)" = "$engine_revision"
git -C build/arasan-src submodule update --init --depth 1 src/syzygy
# Debian's builder supplies GNU bfd; upstream hard-codes the optional gold linker.
python3 - <<'PY'
from pathlib import Path
p=Path('build/arasan-src/src/CMakeLists.txt')
p.write_text(p.read_text().replace('add_link_options("-fuse-ld=gold")','add_link_options("-fuse-ld=bfd")'))
PY
docker run --rm --network none --user "$(id -u):$(id -g)" --mount type=bind,src="$PWD",dst=/workspace \
    thinksmart-qt-builder:debian13 sh -ec '
    cmake -S build/arasan-src/src -B build/arasan-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DSYZYGY_TBS=ON -DOLD=ON -DCMAKE_CXX_FLAGS=-I/workspace/build/arasan-src/src
    cmake --build build/arasan-native --target arasanx-64-old --parallel 4
    cmake -S build/arasan-src/src -B build/arasan-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DSYZYGY_TBS=ON -DNEON=OFF -DOLD=ON -DCMAKE_TOOLCHAIN_FILE=/workspace/app/cmake/arm64.cmake "-DCMAKE_CXX_FLAGS=-mcpu=cortex-a53 -I/workspace/build/arasan-src/src"
    cmake --build build/arasan-arm64 --target arasanx-64-old --parallel 4
    for target in native arm64; do
        mkdir -p build/chess-engine-$target
        binary=arasanx-64-old
        cp build/arasan-$target/$binary build/chess-engine-$target/arasanx
        cp build/arasan-src/network/arasanv11-20261105.nnue build/chess-engine-$target/
        cp build/arasan-src/LICENSE build/chess-engine-$target/LICENSE
        cp build/arasan-src/src/syzygy/LICENSE build/chess-engine-$target/Fathom-LICENSE
        printf "%s\n" "https://github.com/jdart1/arasan-chess/tree/99adddfbc73c4f51c8f9c068439d0844863dd2e9" > build/chess-engine-$target/SOURCE
    done
    '
