#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
test -x build/wake-arm64/thinksmart-wakeword
test -s build/wakeword/stage/sheila.txt
test -x build/home-arm64/thinksmart-home
runtime=build/wakeword/aarch64/sherpa-onnx-v2.0.1-linux-aarch64-shared-cpu-lib/lib
# Stage everything before interrupting the home app or its current audio owner.
tar -C build/wake-arm64 -cf - thinksmart-wakeword | ./tools/thinksmart-ssh 'install -d -m 755 /opt/thinksmart/staging/wake-runtime; tar -xf - -C /opt/thinksmart/staging/wake-runtime'
tar -C "$runtime" -cf - libsherpa-onnx-c-api.so libonnxruntime.so | ./tools/thinksmart-ssh 'tar -xf - -C /opt/thinksmart/staging/wake-runtime'
tar -C build/wakeword/stage -cf - . | ./tools/thinksmart-ssh 'install -d -m 755 /opt/thinksmart/staging/wake-model; tar -xf - -C /opt/thinksmart/staging/wake-model'
tar -C build/wakeword/licenses -cf - . | ./tools/thinksmart-ssh 'install -d -m 755 /opt/thinksmart/staging/wake-licenses; tar -xf - -C /opt/thinksmart/staging/wake-licenses'
./tools/thinksmart-ssh 'sh -s' <<'REMOTE'
set -eu
systemctl stop thinksmart-home.service
install -d -m 755 /usr/local/libexec /usr/local/lib/thinksmart-wakeword /usr/local/share/thinksmart/wakeword/model /usr/local/share/thinksmart/wakeword/licenses
install -m 755 /opt/thinksmart/staging/wake-runtime/thinksmart-wakeword /usr/local/libexec/thinksmart-wakeword
install -m 644 /opt/thinksmart/staging/wake-runtime/*.so /usr/local/lib/thinksmart-wakeword/
install -m 644 /opt/thinksmart/staging/wake-model/model/* /usr/local/share/thinksmart/wakeword/model/
install -m 644 /opt/thinksmart/staging/wake-model/sheila.txt /opt/thinksmart/staging/wake-model/sources.json /usr/local/share/thinksmart/wakeword/
install -m 644 /opt/thinksmart/staging/wake-licenses/* /usr/local/share/thinksmart/wakeword/licenses/
# Resolves the private RPATH without changing the machine's loader configuration.
/usr/local/libexec/thinksmart-wakeword --help >/dev/null
REMOTE
./tools/install-home.sh
