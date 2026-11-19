# Optional appliance logging and on-demand Archive proxy

These settings suit a mostly static installation. They are opt-in and do not
change the app, graphics/audio drivers, Wi-Fi profiles or system default route.
Saved configuration, credentials, prompts, notes, schedules, listening progress
and device registrations remain on disk.

## Low-write logging

`tools/install-appliance.py --logging` installs:

- A warning/error-only system journal in RAM, limited to approximately 8 MiB,
  with rate limiting and no syslog/console forwarding. Journals disappear at
  reboot. The filename `zz-thinksmart-appliance.conf` sorts after the base
  Debian image's `thinksmart.conf`; check the effective configuration rather
  than assuming a numbered drop-in wins.
- Tailscale's supported `TS_NO_LOGS_NO_SUPPORT=true` option, which disables its
  diagnostic uploads and on-disk diagnostic ring buffer. This also opts out of
  Tailscale support that needs those diagnostics. Its persistent identity and
  remote SSH access are retained.
- Quieter Qt logging on the next normal dashboard restart.

Existing archived log files are left alone. This reduces routine logging writes;
it does not make the filesystem read-only or remove application cache/progress
writes. The installer refuses an active rsyslog/syslog-ng sink so an agent can
review it instead of silently leaving another disk logger running.

## On-demand WARP for Audiobooks / TV Archive

First install and register Cloudflare's client separately, in **proxy mode** as
described in [Audiobooks](AUDIOBOOKS.md). This project bundles no WARP software,
account data or registration. The app must be built with the desired module's
proxy flag, and its existing module-specific proxy setting must point to
`http://127.0.0.1:40000`. Other modules retain their direct connections.

`tools/install-appliance.py --archive-proxy` then:

1. Moves WARP's backend proxy to `127.0.0.1:40001`, preserving its registration.
2. Enables `thinksmart-archive-proxy.socket` on `127.0.0.1:40000`. A connection
   starts WARP, waits up to 18 seconds for it to connect, then forwards bytes
   using systemd's native `systemd-socket-proxyd`. There is no persistent Python
   process or polling timer. Startup on the tested connection took about three
   seconds; poor connectivity can take longer or fail with the module's normal
   network error. A later request can retry.
3. Stops the forwarding process and WARP two minutes after all client
   connections close. An open media tunnel, including one retained by a paused
   player, keeps them alive; this timeout never cuts an active stream.
4. Disables eager WARP startup and the optional completed first-boot initializer.
   Its routine diagnostic journal output is filtered, and its diagnostic file
   directory is a private, bounded 32 MiB tmpfs, discarded when the daemon stops.
   `/var/lib/cloudflare-warp` remains persistent. `LogsDirectory=` is cleared
   because its implicit bind mount otherwise hides this RAM filesystem;
   `LOGS_DIRECTORY` still tells the vendor daemon where to write its diagnostics.

No proxy process or WARP daemon is resident between uses; PID1 keeps only the
listening socket. Covers/catalogue requests to Archive can also wake it. A
loopback port probe counts as a connection, so use `systemctl` to inspect idle
state instead of repeatedly probing port 40000. The proxy has no LAN listener.

## Install from the host

Inspect the actual device, current WARP mode, service drop-ins and credentials
first. Wait until playback/conversations are stopped. Configure your own SSH
destination as in [Configuration](CONFIGURATION.md), then stage only our sources:

```sh
tar -cf - tools/install-appliance.py tools/thinksmart-warp-ready.py \
  tools/test-appliance.py app/deploy/appliance |
  ./tools/thinksmart-ssh 'umask 077; install -d -m700 /opt/thinksmart/staging/appliance; tar -xf - -C /opt/thinksmart/staging/appliance'
./tools/thinksmart-ssh 'python3 /opt/thinksmart/staging/appliance/tools/test-appliance.py'
./tools/thinksmart-ssh 'systemd-run --unit=thinksmart-appliance-install --collect --property=Type=oneshot /usr/bin/python3 /opt/thinksmart/staging/appliance/tools/install-appliance.py --logging --archive-proxy'
```

Omit either optional flag if unwanted. No package installation or app rebuild is
performed. Requires Debian's Python 3 and systemd with socket-proxyd idle-exit
support (systemd 246+, tested on 257). An existing custom on-demand setup must
be rolled back/reviewed before installing again.

The Tailscale restart can briefly disconnect SSH. The installation is detached
from SSH and saves a root-only backup under `/opt/thinksmart/backups/`. A
five-minute rollback timer protects remote access. Reconnect immediately and read:

```sh
cat /run/thinksmart-appliance-install.json
systemctl is-active ssh tailscaled thinksmart-home thinksmart-archive-proxy.socket
systemctl is-enabled warp-svc thinksmart-initialize-warp thinksmart-archive-proxy.socket
systemd-analyze cat-config systemd/journald.conf
```

After confirming access and health, stop the **exact** `rollbackTimer` named in
the JSON report with `systemctl stop TIMER_NAME`. If access fails, allow five
minutes for automatic restoration. For a later manual rollback, run:

```sh
python3 /opt/thinksmart/backups/YOUR-BACKUP/install-appliance.py \
  --rollback /opt/thinksmart/backups/YOUR-BACKUP
```

Verify an HTTPS Archive request through port 40000 without disabling TLS
verification. On paths with the previously observed large-key-share TLS issue,
`curl --curves X25519 --proxy http://127.0.0.1:40000 https://archive.org/robots.txt`
uses the same compact TLS group as the module adapters. Check WARP's mount from
inside its namespace while it is running:

```sh
nsenter -t "$(systemctl show warp-svc -p MainPID --value)" -m -- \
  findmnt /var/log/cloudflare-warp
```

Expect `tmpfs`. Confirm both services become inactive after connections close and
the idle period passes; keep the socket enabled. Confirm the module can reconnect
afterward. Record your device, installed options, checks and backup path in your
private `admin.md`. Reboot/startup behaviour is configured persistently; perform
a physical reboot check when convenient.

For temporary troubleshooting, review/remove the appliance journal drop-in and
raise logging before reproducing a fault, then restore it. Routine diagnostics
will otherwise be sparse and limited to the current boot. Local checks:

```sh
python3 tools/test-appliance.py
```

These tests use fake WARP responses and a loopback echo backend. They exercise
bounded readiness failure, socket activation, forwarding, connection retention,
idle exit and repeated activation without paid APIs or microphones.
