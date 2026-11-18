# Development

The application is C++17/Qt 6.8 Quick under `app/`, with QML views and a small
procedural shader. Audio helpers are under `app/audio/`. No device credentials
are needed to compile or run the mocked UI/protocol tests.

## Builders

```sh
./tools/build-home.sh
```

The base Dockerfile supplies Debian's ARM64 cross compiler; Dockerfile.qt adds
Qt and libmpv. Runtime drivers remain on the device. Optional audio/chess builders
are separate scripts and assets stay under ignored `build/`.

For native tests, using the same builder:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" \
  --mount type=bind,src="$PWD",dst=/workspace \
  thinksmart-qt-builder:debian13 sh -ec '
    cmake -S app -B build/home-native -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/home-native --parallel 4
    QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software QT_QUICK_BACKEND=software \
      QT_QUICK_CONTROLS_STYLE=Basic ctest --test-dir build/home-native --output-on-failure
  '
```

Focused tests can be selected by QtTest function name, for example:

```sh
build/home-native/home-ui-tests moduleProfiles \
  deviceConfigurationChangesLocationAndRejectsInvalidFlags \
  dashboardPagesButtonsSwipesAndAppReturn \
  voiceSessionStreamsInterruptsAndDisables liveManagedToolsAndFullDuplex
```

Module creation and third-party contribution are covered in
[MODULE-DEVELOPMENT.md](MODULE-DEVELOPMENT.md). Stream compatibility and
measured decoder limitations are in [VIDEO-COMPATIBILITY.md](VIDEO-COMPATIBILITY.md).

`markets-tests` covers quote/session normalization, rolling crypto comparisons,
atomic watchlists, one-shot crossing alerts, replay/stale suppression, free-feed
backoff and disabled-module inactivity. Markets UI and both voice tool paths use
local fixtures in `home-ui-tests`. See [MARKETS.md](MARKETS.md).

`personal-tests` covers atomic personal-data edits, theme validation/undo,
schedule persistence, missed events, weekday/DST handling and allowed actions.
The UI tests exercise notes, touch deletion, scheduler execution and dictation
through local mock Realtime/Live servers. They create no paid sessions.

`network-tests` runs against an isolated D-Bus NetworkManager mock. The Wi-Fi UI
test uses synthetic access points and actual Qt Virtual Keyboard key taps in
both orientations. `thinksmart-keyboard-check` is a short standalone device
probe: a synthetic field and one programmatic keypress, with no Wi-Fi or voice
activity. The builder creates a user entry for the supplied `BUILDER_UID`/GID so
the private D-Bus test daemon can authenticate the normal unprivileged test user.

Run those inside the builder with the same offscreen environment. Voice tests
use a loopback mock WebSocket, dummy credentials and fake audio processes; they
do not contact OpenAI or capture a microphone. Public tests generate silent WAV
fixtures instead of relying on the developer's voice-preview collection.

## Public screenshots

`capturePublicScreenshots` renders the real UI with synthetic London weather,
energy readings and news fixtures. It starts no cloud conversation. Render with
Xvfb/OpenGL for the GPU orb, or offscreen/software for the Canvas fallback:

```sh
./tools/render-screenshots.sh
```

Review the generated PNGs before publishing; use no real household data, server
artwork, account identifiers or microphone recordings.

## Prepare a public snapshot

```sh
python3 tools/export-public.py --output build/public/thinksmart-home
python3 tools/check-public.py build/public/thinksmart-home
```

The exporter uses an explicit source/tool/document allowlist and creates a clean
snapshot. It excludes private data, build/firmware images, historical personal
docs, recordings, preview collections, model weights and Git history. It does
not publish anything. The check scans the actual export, validates linked local
documentation/assets and flags credentials or forbidden file types/locations.

Commit and publish only that reviewed snapshot. Keep imported licence notices
and source provenance. Root MIT licensing applies to our original work; see
[THIRD_PARTY.md](../THIRD_PARTY.md) before distributing linked binaries.
