#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
mkdir -p docs/screenshots
docker run --rm --network none --user "$(id -u):$(id -g)" \
  --mount type=bind,src="$PWD",dst=/workspace \
  -e QT_QPA_PLATFORM=xcb -e QSG_RHI_BACKEND=opengl \
  -e LIBGL_ALWAYS_SOFTWARE=1 -e QT_QUICK_CONTROLS_STYLE=Basic -e LANG=C.UTF-8 \
  -e THINKSMART_PUBLIC_SCREENSHOTS=/workspace/docs/screenshots \
  thinksmart-qt-builder:debian13 sh -ec '
    Xvfb :99 -screen 0 1600x1400x24 -nolisten tcp -ac >/tmp/thinksmart-xvfb.log 2>&1 &
    render_xpid=$!
    trap "kill $render_xpid" EXIT
    for ready in 1 2 3 4 5 6 7 8 9 10; do
        test ! -S /tmp/.X11-unix/X99 || break
        sleep 0.2
    done
    DISPLAY=:99 build/home-native/home-ui-tests capturePublicScreenshots
  '
