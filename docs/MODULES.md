# Optional modules

Open **Settings → Pages & modules → Add application**. Choose an inactive app,
then a widget or full-page view where offered. Widgets can use an existing page
with a free slot or a new page; full-page views get a dedicated page. **Open on
request** keeps an application available to Sheila without a permanent placement.
Use **Placement** for active apps, **Remove application** to disable selected
apps and remove their cards/pages, or **Pages** to rename/reorder pages and cards.
Saved credentials and data are preserved. **Apply & restart** commits the draft;
**Cancel changes** discards it. See [the guided flow](PAGES-AND-MODULES.md).

Widget pages hold up to six cards (three columns in landscape, two in portrait).
Cards retain their size on sparse pages. News, Notes, Chess, Markets, Recipes and
IPTV offer widgets and full-page views; other media widgets open full browsers.
Home always remains editable. Removing just a card/page leaves its integration
active; removing the application disables it. Bluetooth and Navidrome share the
Music card. All views share one native service per module.

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
| `tvarchive` | Classic TV series/episodes, saved progress, random play and autoplay | [TV Archive guide](TV-ARCHIVE.md); no key; disabled by default |
| `audiobooks` | LibriVox widget, discovery, chapter playback and saved positions | [Audiobook guide](AUDIOBOOKS.md); no key; disabled by default |
| `radio` | Worldwide search, genre browsing and saved favourites | [Radio guide](RADIO.md); optional curated catalogue |
| `markets` | Watchlist, chart widget, price alerts and AI tools | No key required; [Markets guide](MARKETS.md). Free quotes may be delayed; disabled by default |
| `weather` | Home widget and forecast | [Location picker, condition icons and sunrise/sunset](WEATHER.md) |
| `news` | Launcher widget or full-page feeds, configurable sources and Hacker News/comments | Internet; cached daily, manual refresh; [source settings](NEWS.md) |
| `chess` | Launcher widget or full-page board with a local engine | Build/install Arasan for engine play |
| `notes` | Notes widget/page, dictation, routines and schedule controls | Voice dictation additionally requires Assistant; saved notes work offline |
| `bluetooth` | Phone speaker mode in Music | BlueZ/BlueALSA on the device |
| `vrm` | Victron energy/battery card and tools | Read-only VRM credentials |
| `navidrome` | [Music library](MUSIC.md), voice search/playback, saved queues and genre/decade browsing | Server settings/credentials |
| `jellyfin` | Native film browser and playback | Server settings/credentials |
| `iptv` | Live TV browser, saved streams and fullscreen libmpv playback | Optional, off by default; [IPTV guide](IPTV.md) |

Home is always available. The initial layout includes the enabled News/Chess/Notes
pages; Placement can turn them into widgets or make them available on request.
A new dedicated page is created only when Full page or New page is chosen. One-page and two-page
profiles work too. Navidrome and Bluetooth share the Music widget, with only the
selected source tabs. Widget pages retain six grid slots, including empty ones.

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

[Recipes / AI outputs](RECIPES.md) is an optional widget/full-page module and a
worked example of voice delegation, native Markdown, saved task lists and
adapting a recipe demonstration into a general AI text reader.

Music and Video server connections have native **Edit** screens in
**Settings → Pages & modules**. See [connection configuration](CONFIGURATION.md#credentials)
for account/token login, connection tests and private storage.

The Energy/VRM card’s stale badge follows its main battery SoC report (or the
latest available power report when SoC is absent). Each metric retains its own
measurement time in the expanded view, so an old/disconnected AC device does
not label a current battery report as stale. Unknown or old battery timestamps
still produce the stale badge. A successful cloud fetch alone never refreshes
measurement timestamps.
