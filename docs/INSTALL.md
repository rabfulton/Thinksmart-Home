# Installation

Supported target: Lenovo ThinkSmart View CD-18781Y, ARM64 Debian 13, working
device kernel/firmware and Mesa. Development and device testing used the Himax
panel variant. Check another panel/device variant against upstream documentation.

The host can run Arch/Manjaro or another Linux distribution with Docker, Git,
OpenSSH and Python 3.11+. FFmpeg is needed for optional audio preparation. The
host does not need a Debian desktop installation. Builders use Debian containers.

## For installation agents

Start with [AGENTS.md](../AGENTS.md), [configuration](CONFIGURATION.md) and the
[module list](MODULES.md). The README's copyable prompt is the short handoff;
use this guide to carry it through to a working device.

1. **Discover:** inspect the host tools and the device's current OS, model/panel,
   storage and working graphics/audio. Use existing local setup records where
   available. Ask for missing SSH access, location/timezone and service details;
   help the user choose modules and their placement before installing components.
2. **Prepare:** choose the existing-Debian or stock-firmware path below. Create
   private configuration from templates, explicitly review every module flag,
   and build with Docker. Keep the existing device drivers. For stock firmware,
   verify the image, physical target and calibration handling before requesting
   approval to flash; preparing a build does not authorise flashing it.
3. **Install and verify:** use the repository scripts, deploy only the selected
   optional helpers/assets, and preserve current settings and prompts. On an
   upgrade, inspect saved on-device module overrides and page layout as well as
   the host configuration. Reuse the user's pages for new widgets. Record a
   rollback for changes to an existing install.
4. **Hand over:** verify the relevant checks in step 5, document any unverified
   hardware/services, and save actual module choices, paths, access instructions
   and rollback in ignored `admin.md`. Reference private credential files without
   copying their contents. Do not run cloud voice/microphone tests incidentally.

The app, services and third-party accounts are separate setup steps. A compiled
launcher does not establish that Bluetooth audio, hardware video decoding,
server authentication or local wake-word assets work on a particular device.

## 1. Establish the base system

**Already running Debian with working SSH, graphics and audio:** continue below.

**Stock firmware:** start with [m0s4ik/thinksmart-view-debian](https://github.com/m0s4ik/thinksmart-view-debian)
and its [installation guide](https://github.com/m0s4ik/thinksmart-view-debian/blob/main/docs/install.md).
The revision used during development was
`0b481a39d1fd0c223efa1851bc79f3df48782450`; consult that revision's hardware and
installation instructions as well as upstream updates for your device variant.
[rickx/ubuntu-thinksmart](https://github.com/rickx/ubuntu-thinksmart) is another
device/bootstrap reference, with its own per-directory licences.

This repository does not ship the developer's firmware image or automated
repartitioning script. That image contains private device identities and is not
portable. A setup agent must inspect your actual GPT, panel and calibration,
prepare your own configuration/SSH keys and verify the built image. Preserve
calibration independently of whether you choose a complete old-firmware backup.
Review the verified image and target before flashing. Each new device needs its
own host keys, machine identity and Tailscale enrolment.

## 2. Choose modules and establish SSH

```sh
mkdir -p private
chmod 700 private
cp config/ssh.env.example private/ssh.env
chmod 600 private/ssh.env
python3 tools/configure-device.py
```

Edit `private/ssh.env` for your device/key and `private/device.json` for your
modules and weather location. See [CONFIGURATION.md](CONFIGURATION.md) for
initial host-key verification and [MODULES.md](MODULES.md) for dependencies.
The example enables several integrations for compatibility; turn off those you
do not want. IPTV, Markets, Audiobooks, TV Archive and Recipes start disabled. A voice API key is needed only if
you choose cloud voice, not for the dashboard or the free radio/weather/market feeds.

```sh
./tools/thinksmart-ssh 'uname -m; cat /etc/os-release'
```

Use an administrative account permitted to install services; the provided
deployment scripts currently expect root. They run the actual app as `thinksmart`.

## 3. Build the launcher

Ensure Docker is running and your host account can use it:

```sh
docker info
./tools/build-home.sh
```

This builds the base and Qt builder images, then cross-compiles into
`build/home-arm64/`. No device is written during the build. See
[DEVELOPMENT.md](DEVELOPMENT.md) for native checks.

## 4. Install runtime packages and optional components

On the Debian device, preserve its existing device-specific Mesa/kernel/audio
packages. A setup agent should inspect installed dependencies before adding
packages; use `apt-get --no-remove` so a driver replacement is not silently accepted.

The launcher needs labwc, seatd, mpv/libmpv, ALSA utilities and Qt 6.8 runtimes:
Core, GUI, Quick, QuickControls2, Network, DBus, WebSockets and OpenGL; the QML
Quick/Controls/Layouts/Window/WorkerScript modules; and Qt SVG plugins for chess.
The embedded keyboard also requires `qml6-module-qtquick-virtualkeyboard` and
`qt6-virtualkeyboard-plugin`, matching the installed Qt version. NetworkManager
provides Wi-Fi connectivity; the installer adds the narrow `thinksmart-network`
helper for the unprivileged dashboard. See [Wi-Fi settings](WIFI.md).
The exact package names/build versions must match the installed Debian image.
`ldd build/home-arm64/thinksmart-home` is not a host-side cross-architecture
runtime check; use `ldd` on the staged binary on the device.

- **Chess:** `./tools/build-chess-engine.sh`, then `./tools/install-chess-engine.sh`.
- **Bluetooth:** requires working BlueZ and a `bluealsa.service`/`bluealsa-aplay`
  installation. The launcher installer checks for that service when selected;
  disable Bluetooth if it is not needed.
- **Voice:** follow [VOICE.md](VOICE.md) before enabling conversations.
- **VRM/media:** create the selected private JSON files from the templates. Deploy
  them with `tools/install-vrm-config.sh` and `tools/install-media-config.sh`.
- **Radio, weather, news, Markets, IPTV and Audiobooks:** no account/API key is needed for
  their built-in public sources. Configure weather location, enable the wanted
  modules and place their views through the page editor. Free market quotes may
  be delayed; see [Markets](MARKETS.md) for polling and alert behaviour.
  [Audiobooks](AUDIOBOOKS.md) documents LibriVox discovery, saved positions and
  an optional build-time proxy adapter for connections affected by ISP filtering.

## 5. Install and check the launcher

```sh
./tools/install-home.sh
./tools/thinksmart-ssh 'systemctl status thinksmart-home.service --no-pager'
./tools/thinksmart-ssh 'thinksmart-home --command status'
```

The installer deploys `private/device.json`, creates the unprivileged service
account, seeds missing prompts and enables automatic graphical startup. Existing
prompt edits and station catalogues are preserved; your selected device config
is deliberately updated. Inspect local credential files before using credential
deployment scripts, which replace the corresponding private device JSON.

After installation, Settings → Pages & modules customises the layout and module
selection on-device. Existing on-device overrides and prompt edits survive later
installs. Add widgets to suitable existing pages, or create pages according to
the user's preferences. Hiding a widget does not disable its background service;
use the module switch for that. See [modules](MODULES.md).

The display starts asleep. Touch or press a volume key to wake. Check landscape
and portrait touch alignment, sound, volume buttons, screen sleep and a reboot.
Confirm that SSH reconnects after reboot. AI conversation tests can incur API
charges and transmit microphone audio; use local mocks for development.
Verify each selected integration against its actual source, including a saved
favourite or setting surviving an app restart. Separate verified playback from
a request merely being accepted. Mark any unavailable servers or hardware paths
as unverified rather than claiming a complete test.

For ongoing access, enrol the device in your own Tailscale account using its CLI.
OpenSSH over the tailnet works independently of the launcher; no shared Tailscale
state or auth key is included here. See [ADMIN.md](ADMIN.md).
