# TV Archive

An optional widget for the [classic British TV collection on Internet Archive](https://archive.org/download/classic-british-tv-pack).
It opens a native series/episode browser and plays episodes fullscreen through
the existing libmpv renderer. No account or API key is required.

Enable `tvarchive` in **Settings → Pages & modules**, then add its widget to an
existing page. It is disabled by default. It does not create a separate dashboard
page automatically. Disabled modules perform no catalogue requests and expose no
assistant actions.

## Browsing and playback

The catalogue is built from the Archive item's file metadata. Folder names become
series, season/episode numbers are extracted from filenames, and duplicate video
encodings are combined. The smallest MP4/M4V up to 720p is preferred; higher
resolution MP4 and original MKV/AVI files are fallbacks where necessary. MP4 is a
container, so actual codec compatibility still depends on the file. Playback uses
the device's H.264/HEVC hardware path where supported, with a software fallback.
Legacy MPEG-4 Part 2/Xvid recordings use software decoding in the current
player, even when their container is MKV. See [video compatibility](VIDEO-COMPATIBILITY.md).

Search the series list, tap a series, optionally filter its season and tap an
episode. **Continue** resumes that series' saved episode and position. The widget
continues the most recently watched series. **Random** chooses an episode from
the open series, or the whole collection when no series is open. It avoids an
immediate repeat when there is more than one choice.

**Autoplay** is a persistent toggle, initially off. With it on, a confirmed end
of episode starts the next episode in that series; the end of a series stops.
Random playback with autoplay continues randomly in the same scope. Interviews,
films and other unnumbered extras remain available to play but are kept out of
automatic next-episode playback from the main series. Stops,
errors and failed connections do not advance. Reboot never starts playback.
Subtitles start off; enable an available track from the fullscreen player’s
Subtitles menu. The fullscreen player provides pause, seeking, overall volume
and back to the browser.

Catalogue normalization runs in the background to keep touch/playback responsive.
The catalogue is cached locally; opening the widget checks for an update if it
is absent or over 90 days old. **Update** refreshes manually. Failed updates
preserve the last usable catalogue. No full episodes are downloaded for offline
viewing; playback requires a connection and remote files may become unavailable.

## Optional local proxy

Some ISPs filter Internet Archive. A separately installed WARP proxy, as described
in [Audiobooks](AUDIOBOOKS.md), can also serve this module. WARP itself, its
registration and vendor software are not part of this repository.

TV Archive's own adapter is compiled out by default. To include adapters for both
modules:

```sh
./tools/build-home.sh -DTHINKSMART_ENABLE_AUDIOBOOKS_PROXY=ON -DTHINKSMART_ENABLE_TVARCHIVE_PROXY=ON
```

After installing the build, add a service drop-in with your local proxy endpoint:

```ini
[Service]
Environment=THINKSMART_TVARCHIVE_PROXY=http://127.0.0.1:40000
```

Reload systemd and restart the app. The setting accepts only a loopback HTTP
proxy with an explicit port. TV catalogue and playback requests use this proxy;
unrelated modules keep their existing routing. TLS certificate verification stays
enabled. Proxied catalogue requests use a compact X25519 TLS key share for
compatibility with constrained tunnel paths; TLS 1.3 remains available. An unavailable configured proxy reports an error without falling back
to the filtered direct connection. The independent proxy service can remain
connected when the widget is disabled; manage that service separately.

## Progress and administration

- Progress and autoplay: `~thinksmart/.local/share/ThinkSmart/Home/tvarchive/state.json`.
- Replaceable catalogue: `~thinksmart/.cache/ThinkSmart/Home/tvarchive/catalogue.json`.
- Assistant rules: `/etc/thinksmart/assistant/modules/tvarchive/{realtime,live,backend}.md`.

Progress uses stable episode IDs derived from the original filename, so switching
to a smaller derivative does not reset it. Atomic private JSON writes checkpoint
about every 15 seconds, on pause/stop, source changes and normal exit. A failed
resume cannot replace a saved position with zero. Corrupt progress is left
untouched and playback is blocked until repaired. Upgrades preserve these files.

Both speech API paths support:

```json
{"action":"tvarchive.list","search":"Fawlty"}
{"action":"tvarchive.list","id":"SERIES_ID_FROM_LIST"}
{"action":"tvarchive.status"}
{"action":"tvarchive.refresh"}
{"action":"tvarchive.play","id":"EPISODE_ID_FROM_LIST"}
{"action":"tvarchive.resume"}
{"action":"tvarchive.resume","id":"SERIES_ID_FROM_LIST"}
{"action":"tvarchive.random","id":"SERIES_ID_FROM_LIST"}
{"action":"tvarchive.autoplay","enabled":true}
```

Lists return up to 20 entries; search narrows episode titles. IDs must come from
the loaded catalogue. Playback accepts no arbitrary URL. An accepted request
indicates an attempt, not proof of playback: use `media.status` for the actual
player state. Standard media pause/play/stop/volume actions apply.

Remote metadata and programmes retain their owners' terms. No video, artwork or
complete remote catalogue is bundled in the source release. This client is
original MIT code.
