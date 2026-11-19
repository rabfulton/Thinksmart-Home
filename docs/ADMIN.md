# Administration

Connect using your own SSH destination/key, or use `./tools/thinksmart-ssh` after
configuring `private/ssh.env`. On Debian, the setup scripts expect `root`; the UI
runs as the unprivileged `thinksmart` account. SSH/Tailscale remain available
through display sleep and UI restarts while the device has network connectivity.

## Main files on the device

| Location | Purpose |
| --- | --- |
| `/etc/thinksmart/device.json` | Deployment module defaults and default weather location/timezone |
| `~thinksmart/.config/ThinkSmart/Home/weather.json` | Saved weather location override; see [Weather](WEATHER.md) |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home/dashboard.json` | On-device module overrides and page/widget layout; backup in `.bak` |
| `/etc/NetworkManager/system-connections/*.nmconnection` | Your Wi-Fi profiles; root-owned, mode 600 |
| `/etc/thinksmart/assistant/` | `system.md`, `live.md`, `backend.md`, `household.md`, `personalities/*.md`, `modules/*/*.md`, `wakeword.json` |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home/voice.json` | Private API configuration |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home/media.json` | Private Navidrome/Jellyfin connections |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home/vrm.json` | Private VRM connection |
| `/var/lib/thinksmart-home/.config/ThinkSmart/Home.conf` | Saved theme, volume, display, model/personality and per-model voice preferences |
| `/etc/thinksmart/radio-catalog.json` | Curated stations/genres |
| `/etc/thinksmart/stations.json` | Default/legacy radio URLs |
| `/var/lib/thinksmart-home/personal/` | Editable themes, notes, routines and schedules; see [personal actions](PERSONAL.md) |
| `/usr/local/share/thinksmart/voice-previews/` | Optional local WAV previews |
| `/etc/asound.conf`, `/etc/thinksmart/shared-audio/` | Audio routing and PipeWire graph |
| `/etc/thinksmart/labwc/rc.xml` | Compositor/input configuration |
| `/root/.ssh/authorized_keys`, `/etc/ssh/sshd_config.d/` | SSH public keys and server overrides |
| `/var/lib/tailscale/` | Tailscale identity, managed by tailscaled |

Application credentials/prompts should be owned by `thinksmart:thinksmart`, mode
600. Device configuration reloads on restart. Settings → Pages & modules saves the
app-owned dashboard override and restarts the dashboard without a device reboot. Prompts reload at each conversation;
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
Additional services are `thinksmart-power`, `thinksmart-actions` (local scheduling),
`thinksmart-network` (Wi-Fi settings), and optionally `thinksmart-pipewire`,
`thinksmart-wireplumber` and `thinksmart-audio-routing`.

## Wi-Fi and remote access

For touch configuration, open **Settings → Wi-Fi settings**. The embedded Qt
keyboard supports network names and passwords; saved profiles expose automatic
connection and priority. See [Wi-Fi settings and keyboard](WIFI.md) for details
and helper administration. Existing NetworkManager keyfiles remain authoritative.

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

## IPTV

See [IPTV.md](IPTV.md) for browsing, saved/custom streams and local tool commands.
Favourites and the last selection are private in
`~thinksmart/.local/share/ThinkSmart/Home/iptv/favourites.json`; the replaceable
directory is `~thinksmart/.cache/ThinkSmart/Home/iptv/catalogue.json`. Module
policies live in `/etc/thinksmart/assistant/modules/iptv/`.

## Radio favourites

Saved radio stations: `~thinksmart/.local/share/ThinkSmart/Home/radio/favourites.json`
(mode 600), separate from `~thinksmart/.cache/ThinkSmart/Home/radio-browser/`.
See [RADIO.md](RADIO.md) for initial favourites, discovery and AI actions.

## Markets

Watchlist/focus/price alerts: `~thinksmart/.config/ThinkSmart/Home/markets.json`.
Cache: `~thinksmart/.cache/ThinkSmart/Home/markets.json`. Module policies:
`/etc/thinksmart/assistant/modules/markets/{realtime,live,backend}.md`.
[Markets](MARKETS.md) describes free-feed timing and alert eligibility.

## News preferences

Column sources: `~thinksmart/.config/ThinkSmart/Home/news.json` (mode 600).
Edit through Settings → Pages & modules → Modules → News → Edit. Existing
World & finance / Hacker News choices are preserved. [News guide](NEWS.md).

## Audiobooks

Shelf/positions: `~thinksmart/.local/share/ThinkSmart/Home/audiobooks/library.json`.
Cache: `~thinksmart/.cache/ThinkSmart/Home/audiobooks/`. Module rules:
`/etc/thinksmart/assistant/modules/audiobooks/{realtime,live,backend}.md`.
[Audiobook guide](AUDIOBOOKS.md). No credentials or separate audio service needed.

Optional local-proxy support is compiled out by default. Build with
`-DTHINKSMART_ENABLE_AUDIOBOOKS_PROXY=ON`, then supply the module-only
`THINKSMART_AUDIOBOOKS_PROXY` setting through a launcher service drop-in if
needed. The [proxy guide](AUDIOBOOKS.md#optional-proxy-for-audiobooks-only)
covers WARP as one possible ISP-filtering workaround, installation scope and
removal. Any proxy service and its registration state are managed separately.

TV Archive keeps episode/series progress and autoplay in
`~thinksmart/.local/share/ThinkSmart/Home/tvarchive/state.json` and a replaceable
catalogue in `~thinksmart/.cache/ThinkSmart/Home/tvarchive/catalogue.json`. Preserve
the former during upgrades. Its optional local proxy is configured independently
with `THINKSMART_TVARCHIVE_PROXY`; see [TV Archive](TV-ARCHIVE.md).
