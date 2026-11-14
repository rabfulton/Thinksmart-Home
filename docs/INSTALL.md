# Installation

Supported target: Lenovo ThinkSmart View CD-18781Y, ARM64 Debian 13, working
device kernel/firmware and Mesa. Development and device testing used the Himax
panel variant. Check another panel/device variant against upstream documentation.

The host can run Arch/Manjaro or another Linux distribution with Docker, Git,
OpenSSH and Python 3.11+. FFmpeg is needed for optional audio preparation. The
host does not need a Debian desktop installation. Builders use Debian containers.

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

The display starts asleep. Touch or press a volume key to wake. Check landscape
and portrait touch alignment, sound, volume buttons, screen sleep and a reboot.
Confirm that SSH reconnects after reboot. AI conversation tests can incur API
charges and transmit microphone audio; use local mocks for development.

For ongoing access, enrol the device in your own Tailscale account using its CLI.
OpenSSH over the tailnet works independently of the launcher; no shared Tailscale
state or auth key is included here. See [ADMIN.md](ADMIN.md).
