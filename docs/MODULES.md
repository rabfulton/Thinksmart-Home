# Optional modules

Open **Settings → Pages & modules** on the device. The Pages tab adds, names,
reorders and removes pages and places widgets; the Modules tab enables/disables
integrations independently of placement. **Apply & restart** saves an atomic
configuration and restarts the dashboard; it stops current playback/conversation,
then returns with the screen awake. Cancel discards the draft.

Widget pages hold up to six cards (three columns in landscape, two in portrait).
Cards retain their size when a page has empty slots. The TV widget opens its full
channel browser, like the Music and Video launchers.
Chess, Notes, News, Markets and IPTV provide full-page views. Home stays available and editable;
its widgets can all be removed. Module placements are retained while disabled.
Removing a placement does not disable its AI/background integration. The same
widget can be shown on different pages, sharing one native service.

On-device customisation is saved in
`/var/lib/thinksmart-home/.config/ThinkSmart/Home/dashboard.json`, mode 600.
It overrides module defaults in `/etc/thinksmart/device.json` without changing
location, credentials or that deployment file. Existing installs without an
editor file keep their original page order and six-card layout.

For deployment defaults, set the `modules` booleans in `private/device.json`
before installation, or edit `/etc/thinksmart/device.json` over SSH. A saved
on-device override takes precedence; restart after manual edits. See
[page configuration and development](PAGES-AND-MODULES.md) for the format and
module registrations.

| Flag | Enables | Additional setup |
| --- | --- | --- |
| `assistant` | Voice settings/conversations, local wake word and orb | OpenAI credentials; optional audio/wake setup in [VOICE.md](VOICE.md) |
| `radio` | Worldwide search, genre browsing and saved favourites | [Radio guide](RADIO.md); optional curated catalogue |
| `markets` | Watchlist, chart widget, price alerts and AI tools | No key required; [Markets guide](MARKETS.md). Free quotes may be delayed; disabled by default |
| `weather` | Home widget and forecast | Configure weather location/timezone |
| `news` | International/finance feeds and Hacker News/comments | Internet; cached daily, manual refresh |
| `chess` | Theme-aware board and local engine | Build/install Arasan for engine play |
| `notes` | Notes page, dictation, routines and schedule controls | Voice dictation additionally requires Assistant; saved notes work offline |
| `bluetooth` | Phone speaker mode in Music | BlueZ/BlueALSA on the device |
| `vrm` | Victron energy/battery card and tools | Read-only VRM credentials |
| `navidrome` | Local music-library client | Server settings/credentials |
| `jellyfin` | Native film browser and playback | Server settings/credentials |
| `iptv` | Live TV browser, saved streams and fullscreen libmpv playback | Optional, off by default; [IPTV guide](IPTV.md) |

Home is always available. Enabled News/Chess pages join the ring automatically;
one-page and two-page profiles work too. The Music card appears when Navidrome or
Bluetooth is enabled, with only the selected source tabs. Cards reflow when hidden.

Disabling a module removes its UI entry, stops its polling/engine/microphone
activity and removes its actions from advertised AI capabilities. The local tool
dispatcher also rejects disabled actions. Private VRM/media/voice configuration
is not loaded for disabled integrations. This is runtime module selection: the
current shared application binary still contains the code and common Qt/libmpv
dependencies. Optional engine/model assets can be omitted entirely.

For a simple radio/weather display:

```sh
python3 tools/configure-device.py --disable assistant --disable news --disable chess --disable notes \
  --disable bluetooth --disable vrm --disable navidrome --disable jellyfin
```

For a new module, update the configuration names/validation, native client,
launcher entry, dispatcher permissions and tool descriptions together. Keep
credentials out of QML and respect the app's existing audio/display ownership.

To develop a new integration, follow [MODULE-DEVELOPMENT.md](MODULE-DEVELOPMENT.md).
