# Administration

Connect using your own SSH destination/key, or use `./tools/thinksmart-ssh` after
configuring `private/ssh.env`. On Debian, the setup scripts expect `root`; the UI
runs as the unprivileged `thinksmart` account. SSH/Tailscale remain available
through display sleep and UI restarts while the device has network connectivity.

## Main files on the device

| Location | Purpose |
| --- | --- |
| `/etc/thinksmart/device.json` | Module flags and weather location/timezone |
| `/etc/NetworkManager/system-connections/*.nmconnection` | Your Wi-Fi profiles; root-owned, mode 600 |
| `/etc/thinksmart/assistant/` | `system.md`, `live.md`, `backend.md`, `household.md`, `personalities/*.md`, `wakeword.json` |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home/voice.json` | Private API configuration |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home/media.json` | Private Navidrome/Jellyfin connections |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home/vrm.json` | Private VRM connection |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home.conf` | Saved theme, volume, display, model/personality and per-model voice preferences |
| `/etc/thinksmart/radio-catalog.json` | Curated stations/genres |
| `/etc/thinksmart/stations.json` | Default/legacy radio URLs |
| `/usr/local/share/thinksmart/voice-previews/` | Optional local WAV previews |
| `/etc/asound.conf`, `/etc/thinksmart/shared-audio/` | Audio routing and PipeWire graph |
| `/etc/thinksmart/labwc/rc.xml` | Compositor/input configuration |
| `/root/.ssh/authorized_keys`, `/etc/ssh/sshd_config.d/` | SSH public keys and server overrides |
| `/var/lib/tailscale/` | Tailscale identity, managed by tailscaled |

Application credentials/prompts should be owned by `thinksmart:thinksmart`, mode
600. Device configuration reloads on restart. Prompts reload at each conversation;
Assistant Settings → Reload refreshes configuration/personality choices.
Stop the application before directly editing `Home.conf` so it cannot overwrite
your changes. Saved UI selections take priority over voice.json defaults.

## Commands on the device

```sh
systemctl status thinksmart-home.service --no-pager
journalctl -u thinksmart-home.service -b -n 100 --no-pager
systemctl restart thinksmart-home.service
thinksmart-home --command status
thinksmart-home --command wake
thinksmart-home --command screen-off
thinksmart-home --command assistant-disable
thinksmart-home --command assistant-enable
thinksmart-home --command stop
```

UI restart interrupts playback/conversations. SSH is a separate service.
Additional services are `thinksmart-power`, and optionally `thinksmart-pipewire`,
`thinksmart-wireplumber` and `thinksmart-audio-routing`.

## Wi-Fi and remote access

NetworkManager manages connections. Use `nmcli device status` and
`nmcli connection show` to identify your profiles. Edit the appropriate keyfile
with Vim or use NetworkManager's tools; keep passwords out of command history.
After file edits, `nmcli connection reload` reloads profiles; activating an edited
profile with `nmcli connection up YOUR_PROFILE` can drop the current SSH session.
Tailscale reconnects when Internet connectivity returns.

Multiple profiles can autoconnect, with a higher priority on the preferred WPA2
network and a lower priority on a hidden/open fallback. A hidden open profile
uses `hidden=true` with no Wi-Fi security section. NetworkManager usually keeps
a working connection until it drops; test fallback at the actual location.

SSH uses your authorized keys. Tailscale enrolment and any tailnet access rules
belong to your account. Useful checks: `tailscale status`, `tailscale ip -4` and
`systemctl status ssh tailscaled --no-pager`.

## Power and recovery

The power menu exposes restart and shutdown. On the tested hardware, normal
poweroff can reboot while power remains connected; the project's halt procedure
is a workaround. Wait for shutdown before unplugging. Display sleep is separate
from shutdown and keeps radio, wake detection and remote administration running.

Keep backups of your private configuration and this device's calibration and
identities. Shared-audio setup saves its rollback files under
`/opt/thinksmart/shared-audio-rollback/`. Application-specific update backups may
be under `/opt/thinksmart/backups/`.
