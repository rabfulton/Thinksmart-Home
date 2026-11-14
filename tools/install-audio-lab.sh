#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
test -x build/audio-arm64/thinksmart-audio-lab
test -s build/audio-reference.s16
# Runtime plan is one small DSP package on the current image; no sound-server
# migration or change to the kernel, Mesa, mixer, or ALSA configuration.
./tools/thinksmart-ssh 'apt-get install -y --no-install-recommends libspeexdsp1 libwebrtc-audio-processing-1-3'
tar -C build/audio-arm64 -cf - thinksmart-audio-lab | ./tools/thinksmart-ssh 'install -d -m 755 /opt/thinksmart/staging; tar -xf - -C /opt/thinksmart/staging'
cat build/audio-reference.s16 | ./tools/thinksmart-ssh 'cat > /opt/thinksmart/staging/audio-reference.s16'
./tools/thinksmart-ssh 'install -d -m 755 /usr/local/libexec /usr/local/share/thinksmart/audio-lab; install -m 755 /opt/thinksmart/staging/thinksmart-audio-lab /usr/local/libexec/thinksmart-audio-lab; install -m 644 /opt/thinksmart/staging/audio-reference.s16 /usr/local/share/thinksmart/audio-lab/reference.s16; ldd /usr/local/libexec/thinksmart-audio-lab | grep "not found" && exit 1; true'
