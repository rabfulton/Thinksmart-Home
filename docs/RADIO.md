# Radio

The Radio widget opens on **Favourites**. The left-hand categories provide
Favourites, All stations, Folk, Jazz, Rock and Classical; choosing a discovery
category browses Radio Browser without starting playback. Typing a name searches
the worldwide directory. Press Search or keyboard Done to search immediately;
Done also closes the keyboard. Requests are debounced while typing.

Tap a station row to play. Tap its star to save or remove a favourite. Stars
update in place without resetting the result list. More stations fetches another
page when available. Radio 4 and both Houses of Parliament are initial favourites
when marked `pinned` in the installed curated catalogue. There is no separate
pinned-station bar above the list. Removing a favourite does not delete the
curated station or its stable playback ID.

The browser requests up to 50 results at a time. AI discovery still defaults to
five candidates by country, language, tag or name. It uses the same directory
client, server discovery/failover and cached searches. Search does not start
playback. Network or station failures are reported; a directory entry is not a
guarantee of the station's current programme or availability.

Saved favourites live independently of the replaceable search cache in:

```text
~thinksmart/.local/share/ThinkSmart/Home/radio/favourites.json
```

The versioned file stores stable station IDs and stream metadata, is written
atomically with mode 600, and is preserved on upgrades. Saved directory stations
remain known after caches expire and after restarting. A corrupt favourites file
is left untouched and cannot be overwritten through the UI until repaired.

The curated catalogue is `/etc/thinksmart/radio-catalog.json`; initial favourites
come from its `pinned` flags only when no favourites file exists. Parliament's
`commons`/`lords` entries retain their live/latest-recording resolvers instead of
saving expiring playback URLs. The earlier Radio 4 address in
`/etc/thinksmart/stations.json` is honoured when loading the catalogue.

The AI can use `radio.favourites`, `radio.save` and `radio.remove` with returned
station IDs. `radio.list` retains the curated catalogue; `radio.search` discovers
worldwide candidates, and `radio.play` plays a known curated, saved or cached ID.
Volume and playback controls remain shared with other media. Disabling Radio
removes its tools/policies and stops UI discovery. Editable provider policies
are under `/etc/thinksmart/assistant/modules/radio/`.

Directory source: [Radio Browser API](https://api.radio-browser.info/).
