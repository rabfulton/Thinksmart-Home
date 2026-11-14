#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
test -x build/chess-engine-arm64/arasanx
tar -C build/chess-engine-arm64 -cf - . | ./tools/thinksmart-ssh 'install -d -m 755 /opt/thinksmart/chess-staging; tar -xf - -C /opt/thinksmart/chess-staging'
tar -C app -cf - assets/chess/LICENSE third_party/chess/LICENSE third_party/chess/SOURCE | ./tools/thinksmart-ssh 'tar -xf - -C /opt/thinksmart/chess-staging'
./tools/thinksmart-ssh 'sh -s' <<'REMOTE'
set -eu
install -d -m 755 /usr/local/libexec/thinksmart-arasan
install -m 755 /opt/thinksmart/chess-staging/arasanx /usr/local/libexec/thinksmart-arasan/arasanx
install -m 644 /opt/thinksmart/chess-staging/arasanv11-20261105.nnue /usr/local/libexec/thinksmart-arasan/
install -m 644 /opt/thinksmart/chess-staging/LICENSE /opt/thinksmart/chess-staging/Fathom-LICENSE /opt/thinksmart/chess-staging/SOURCE /usr/local/libexec/thinksmart-arasan/
install -d -m 755 /usr/local/share/doc/thinksmart-home/chess
install -m 644 /opt/thinksmart/chess-staging/assets/chess/LICENSE /usr/local/share/doc/thinksmart-home/chess/pieces-LICENSE
install -m 644 /opt/thinksmart/chess-staging/third_party/chess/LICENSE /usr/local/share/doc/thinksmart-home/chess/library-LICENSE
install -m 644 /opt/thinksmart/chess-staging/third_party/chess/SOURCE /usr/local/share/doc/thinksmart-home/chess/library-SOURCE
REMOTE
