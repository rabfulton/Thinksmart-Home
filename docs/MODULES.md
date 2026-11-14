# Optional modules

Set the `modules` booleans in `private/device.json` before installation, or edit
`/etc/thinksmart/device.json` and restart the dashboard on an installed device.

| Flag | Enables | Additional setup |
| --- | --- | --- |
| `assistant` | Voice settings/conversations, local wake word and orb | OpenAI credentials; optional audio/wake setup in [VOICE.md](VOICE.md) |
| `radio` | Curated stations, genre playback and worldwide station search | Internet streams; edit `/etc/thinksmart/radio-catalog.json` |
| `weather` | Home widget and forecast | Configure weather location/timezone |
| `news` | International/finance feeds and Hacker News/comments | Internet; cached daily, manual refresh |
| `chess` | Theme-aware board and local engine | Build/install Arasan for engine play |
| `bluetooth` | Phone speaker mode in Music | BlueZ/BlueALSA on the device |
| `vrm` | Victron energy/battery card and tools | Read-only VRM credentials |
| `navidrome` | Local music-library client | Server settings/credentials |
| `jellyfin` | Native film browser and playback | Server settings/credentials |

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
python3 tools/configure-device.py --disable assistant --disable news --disable chess \
  --disable bluetooth --disable vrm --disable navidrome --disable jellyfin
```

For a new module, update the configuration names/validation, native client,
launcher entry, dispatcher permissions and tool descriptions together. Keep
credentials out of QML and respect the app's existing audio/display ownership.
