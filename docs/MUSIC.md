# Music / Navidrome

The Music card opens a native album browser and, when enabled, a separate Phone
tab for Bluetooth speaker mode. Configure the server through **Settings →
Pages & modules → Music / Navidrome → Edit**; see [connection settings](CONFIGURATION.md#credentials).
A reachable Tailscale server works as well as a local server.

The compact control row combines browsing, search and reload. Browse by:

- Newest albums, album title A–Z or artist A–Z.
- Recently played, most played, highest rated or server favourites.
- Genre, using tags returned by your server.
- Decade, using album year tags, with newer releases first within that decade.

Genre and decade are separate browsing modes. The standard
[OpenSubsonic album-list API](https://opensubsonic.github.io/docs/endpoints/getalbumlist2/)
provides genre **or** year-range lists, rather than a combined genre/year filter.
Albums with missing genre or year tags may not appear in these selections.
Listening-history, rating and favourite lists reflect the server's metadata;
this client does not currently submit scrobbles, edit ratings or mark favourites.
[Navidrome documents its supported endpoints and playback-history behaviour](https://www.navidrome.org/docs/developers/subsonic-api/).

Tap an album for its tracks; Back returns to the previous selection. Tapping a
track appends just that track to your queue. If the queue is stopped, playback
starts automatically, switching away from radio or another source. If music is
already playing or connecting, the new track joins the end without interruption;
adding to a paused music queue resumes it.
**Queue album** appends every track from the open album, in order. Repeated
selections are allowed. Search looks for tracks across the library and clears
the current browse filter; Back can return to the previous view. Done on the
keyboard submits and closes it.

**Queue**, beside Library and Phone, shows the saved track list. **Play** starts
at the selected track (or the beginning for a new/finished queue). Tap a queue
entry to play from there. The bottom Previous/Next buttons navigate the queue;
tracks advance automatically at the end of a song and stop after the last one.
Browsing and library loading do not replace or block the queue. **Play again**
starts from the first track; played entries stay available. **Clear** empties
the queue and stops its music if it is the current source; radio and Bluetooth
playback are left alone. The bottom Play button can start a queued selection
while Music is open, or restart the current music track after Stop.

Queue metadata and the selected index survive restarts, separately for each
server/account, in `~thinksmart/.local/share/ThinkSmart/Home/music/queue-*.json`
(mode 600; directory 700). No passwords, stream URLs or audio are stored there.
Queues are limited to 2,000 tracks. Writes happen on queue edits and track/index
changes, with unchanged data skipped; progress updates do not write to disk.
Restoring a queue after a restart does not start playback automatically; adding
new tracks or choosing Play does.

Lists fetch 60 entries at a time and append as you approach the bottom. They
retain scroll position, stop at the end, and keep loaded entries after a network
failure. Reload retries from the first page. Albums and search results are
privately cached for use when the server is unavailable; audio itself is streamed,
not downloaded for offline playback. Opening the browser refreshes its current
selection. Disabled Navidrome does not fetch genres, albums or streams.

Library cache files are under `~thinksmart/.cache/ThinkSmart/Home/library-*.json`
(mode 600), separated by server/account, browsing mode, genre, decade and search.
There is no periodic library polling. Your server credentials stay in the private
`media.json`, separate from the cache. Bluetooth and Navidrome share the existing
Music widget and speaker controls.

## Voice and local actions

When Assistant and a configured Navidrome module are enabled, **Realtime and
GPT Live** expose `music_action`. Try “play some Nina Simone”, “play the album
Kind of Blue”, “add this album to the queue”, or “skip this track”. Named artists,
albums and songs use Navidrome; broad “some music/put on the radio” requests keep
the radio default unless you ask for your library. These are new conversation
tools, so finish any existing conversation before testing after an upgrade.

Search returns up to five artists, albums and songs per request, with typed IDs
and metadata. The assistant can inspect an artist’s albums or an album’s tracks,
then select something suitable without making you operate the touchscreen.
**Play** replaces the queue with the selection; **enqueue** appends using the
same automatic-start behaviour as touch. An accepted request means submitted,
not independently confirmed audible playback. Library searches do not change
your currently displayed browse selection.

| Native action | Purpose |
| --- | --- |
| `music.search` | Search `search` text across artists, albums and songs |
| `music.artist` / `music.album` | List a returned artist’s albums or album’s tracks using `id` |
| `music.browse` | Album lists by `mode`, optional `genre` or `decade` |
| `music.genres` | List genre tags, optionally filter with `search` |
| `music.play` | Replace/play returned `id` plus `type` (`song`/`album`); without ID, start the saved queue |
| `music.enqueue` | Append returned `id` plus `type` |
| `music.queue` | Read the saved queue and selected index |
| `music.next` / `music.previous` | Navigate queued tracks |
| `music.replay` / `music.clear` | Replay or clear the queue |

List actions accept `limit` (1–10) and `offset` (0–2000). Use only IDs returned
by these actions; the native client rejects invented IDs and arbitrary URLs.
Responses contain no credentials or authenticated stream addresses. Native calls
use `thinksmartctl` JSON over the local control socket, for example:

```sh
thinksmartctl '{"action":"music.search","search":"Nina Simone","limit":5}'
thinksmartctl '{"action":"music.queue"}'
```

Disabled or unconfigured music advertises no music tools. Reads do not start
playback, and cancelled/ended conversations cannot start a delayed music request.
On installations without shared audio, Realtime defers playback until the brief
acknowledgement ends and voice releases the speaker. There is no background
library polling or new paid model call for these tools. New prompts live under
`/etc/thinksmart/assistant/modules/navidrome/{realtime,live,backend}.md`; existing
household, personality and core policies are preserved on upgrades.
