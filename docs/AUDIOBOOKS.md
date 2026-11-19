# Audiobooks

Enable Audiobooks in **Settings → Pages & modules** and add its widget to an
existing widget page. It is optional and disabled by default for public installs.
No account, server or API key is needed. Books stream through the existing mpv
and speaker-volume path; entire books are not downloaded automatically.

The initial book is **Moby Dick, or the Whale**, by Herman Melville, read by
**Stewart Wills**, [LibriVox recording 753](https://librivox.org/moby-dick-by-herman-melville/).
It includes the introductory material and 44 audio parts. Use the chapter list
to jump to a particular part.

## Discovery

- **My books:** books you start or save, with the current/last book first. The
  initial Moby-Dick recommendation appears here too.
- **Top 100:** [Brendan St. John's recommendations](https://www.bstjohn.net/audiobooks/),
  ranked using Project Gutenberg download popularity matched to LibriVox editions.
  Filter by genre to browse. The source's generated date is shown; popularity does
  not establish narrator quality. A compact snapshot of 100 available catalogue
  links is bundled with source attribution, so browsing works immediately without
  downloading a list. It contains titles, authors and reference metadata, not
  recordings or cover art. Entries without a playable catalogue link are skipped.
  Update fetches the source's public structured list and caches the result.
- **Search:** title or author search through the [LibriVox API](https://librivox.org/api/info),
  20 results at a time with explicit More. Versions and languages differ; select
  the recording you want.
- **Monthly picks:** books linked from the
  [LibriVox category RSS feed](https://librivox.org/category/monthly-picks/feed/).
  Choose a dated post to browse its books. Older posts are not presented as this
  month's selections.

Each book opens recording details, narrator/language, description and chapters.
The widget has Resume/Pause and −30s/+30s controls, with more space for the title
instead of a decorative book icon. Resume immediately shows Loading while the
stream connects and becomes available again after a failed load or Stop. The
expanded player also has skip buttons and a seek slider. In landscape, book
information and compact controls sit on the left, with an independently scrolling
chapter list on the right. Portrait stacks them vertically. Seeking works while
playing or paused. Start/Resume plays from its saved location. Chapter selection starts that part
from the beginning. The full description appears below the controls; longer
descriptions scroll within the information panel. A single button switches
between Save and Remove. Restart and removal require touch
confirmation; removing a book also removes its saved position.

The bundled Top 100 does not trigger an automatic network request. After a
successful manual update, its downloaded list is cached for a week. Picks are
cached for a day; Update requests an earlier refresh. Failed updates keep the
bundled/cached list. The catalogue is queried only while browsing/searching
or resolving a selected book. There is no bulk scan of LibriVox's catalogue.
Failed updates retain cached lists. Publisher/API availability can vary.

Recordings and cover art are hosted by Internet Archive. If your ISP blocks
Archive, LibriVox browsing may still work while audio fails to load. A VPN must
carry the device's traffic, for example through your router or a reachable
Tailscale exit node; a VPN confined to a browser does not carry the player's
traffic. Encrypted DNS can help with DNS filtering, but cannot bypass an IP or
TLS hostname block. Keep the original HTTPS hostnames and certificate checks;
hardcoded numeric URLs are not a reliable workaround.

### Optional proxy for Audiobooks only

Local-proxy integration is **compiled out by default**. To include our optional
adapter, build with:

```sh
./tools/build-home.sh -DTHINKSMART_ENABLE_AUDIOBOOKS_PROXY=ON
```

For a manual CMake build, pass the same `-DTHINKSMART_ENABLE_AUDIOBOOKS_PROXY=ON`
option when configuring. This defines `THINKSMART_ENABLE_AUDIOBOOKS_PROXY` for
the app and its tests. The default OFF build ignores the proxy environment
setting and uses direct connections. Cover caching and HTTPS verification work
in both builds. Enabling the option does not install or start any VPN/proxy.

`THINKSMART_AUDIOBOOKS_PROXY=http://127.0.0.1:40000` sends this module's
Internet Archive cover downloads and audio player through a local HTTP proxy.
LibriVox API/catalogue/RSS requests and the Top 100 feed connect directly: an
Archive block does not require proxying those unblocked discovery sources, and
some of them may reject or stall connections through a proxy.
It does not set a global Qt, system or process-wide HTTP proxy. Other modules,
including Radio, IPTV, Jellyfin and Assistant, keep their normal connections.
The endpoint must be HTTP on localhost/127.0.0.1/::1 with an explicit port and
no credentials. An invalid setting blocks audiobook requests; an unavailable
proxy fails the request rather than falling back directly. Audio uses HTTPS
certificate verification and Debian's CA bundle, including when proxied.

Cloudflare's free consumer WARP client offers
[local proxy mode](https://developers.cloudflare.com/warp-client/warp-modes/#local-proxy).
The command-line registration creates the device registration; no website login
or paid WARP+ subscription is required. Use its official
[signed package repository](https://pkg.cloudflareclient.com/) for Debian ARM64.
Inspect APT's proposed dependencies before installing and preserve the device's
working drivers. WARP remains optional and is not installed by the app builder.
It may help with filtering by some ISPs, but is not a guaranteed fix for every
network restriction. This repository distributes only our optional proxy adapter
and setup instructions: **no Cloudflare software, packages, registration state
or account data are bundled**. Install WARP separately from Cloudflare under its
own terms if you choose to use it; other compatible local HTTP proxies work too.

Configure WARP in **proxy mode before connecting**. Check the installed client's
`warp-cli --help`, `warp-cli mode --help` and `warp-cli proxy --help` for its
current commands. On the client tested here (2026.8.2115.0), initial setup was:

```sh
sudo warp-cli --accept-tos mode proxy
sudo warp-cli --accept-tos proxy port 40000
sudo warp-cli --accept-tos registration new  # Once, for a new registration only
sudo warp-cli --accept-tos connect
```

Preserve an existing registration rather than replacing it. Do not use
full-device WARP or DNS-only mode for this setup.
First test an Archive HTTPS download through the loopback proxy with `curl`;
do not disable TLS verification. Keep registration keys in WARP's private state,
outside the repository.

Pass the setting only to the launcher with a systemd drop-in, for example
`/etc/systemd/system/thinksmart-home.service.d/audiobooks-proxy.conf`:

```ini
[Service]
Environment=THINKSMART_AUDIOBOOKS_PROXY=http://127.0.0.1:40000
```

Then reload systemd and restart `thinksmart-home.service`. The audiobook player
receives its proxy only when an audiobook starts; switching to other media starts
an ordinary player. Cover images are cached locally through the module's own
network manager, avoiding QML's shared image downloader. Remove the drop-in and
restart to return Audiobooks to direct connections. The independent WARP proxy
service remains connected by default. The optional [appliance setup](APPLIANCE.md)
starts it on demand and stops it after two minutes without client connections,
with its routine diagnostic files held in RAM.

## Listening positions

The widget shows the last playback book and offers Start, Resume or Pause.
Positions are stored by **recording ID and chapter ID**, not just the book title.
They are checkpointed during playback (about every 15 seconds), on pause/stop,
on source changes and during a normal app exit. Chapter completion advances
automatically; stop, errors and aborted loads do not. Failed resumes do not
overwrite saved positions with zero. Finished books offer Play again. Reboot
does not autoplay.

Private storage uses versioned JSON and atomic writes:

- Shelf/positions: `~thinksmart/.local/share/ThinkSmart/Home/audiobooks/library.json`.
- Replaceable metadata: `~thinksmart/.cache/ThinkSmart/Home/audiobooks/`.
- Module rules: `/etc/thinksmart/assistant/modules/audiobooks/{realtime,live,backend}.md`.

The library survives upgrades. Malformed library files are left untouched and
playback is blocked until repaired, so positions are not silently overwritten.
Up to 100 books can be kept; recording caches are bounded separately. Disabled
Audiobooks does not load the shelf or contact providers. Runtime disabling keeps
the code and saved data.

## Assistant and native tools

Both speech API paths expose module-scoped audiobook actions. Example requests
for `thinksmartctl`:

```json
{"action":"audiobooks.list"}
{"action":"audiobooks.status"}
{"action":"audiobooks.search","search":"Moby Dick","category":"title"}
{"action":"audiobooks.search","search":"Melville","category":"author"}
{"action":"audiobooks.play","id":"lv:753"}
{"action":"audiobooks.resume"}
{"action":"audiobooks.next"}
{"action":"audiobooks.previous"}
{"action":"audiobooks.save","id":"lv:753"}
{"action":"audiobooks.remove","id":"lv:753"}
```

Search returns up to five candidates to the assistant. Playback accepts known
recording IDs, never arbitrary URLs; catalogue links resolve the exact edition
before retrieving tracks. Accepted/preparing is not proof of audio playback;
use `media.status`. General media pause/play/stop and overall volume apply too.
Module policies cover editions, language, positions and dated discovery lists.
Disabling the module removes these actions and policies.

Remote metadata, descriptions, recordings, cover art and ranking data retain
their source terms. The bundled factual link index credits its source in
`app/data/audiobooks/SOURCE`; website code, prose, download counts and Guardian
list tags are excluded. No recordings or cover images are shipped in this
repository. Our client implementation is MIT.
