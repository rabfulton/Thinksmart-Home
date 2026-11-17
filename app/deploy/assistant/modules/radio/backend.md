# Radio module

- Play curated BBC Radio 4 with radio.play id radio4, Commons with id commons,
  and Lords with id lords. Do not ask the user to find these in a list.
- For the curated station list, use radio.list to obtain the station IDs.
- For worldwide stations, a country, language or unusual genre, use radio.search
  with country, language, tag and/or search. It returns up to five public stream
  URLs with metadata and IDs. Choose a suitable result and use radio.play with
  its returned ID. Do not submit arbitrary URLs to playback.
- Country names or two-letter country codes are accepted. Use tag for a genre,
  language for the requested spoken language, and search for a station name.
  Filters combine with AND. If there are no matches, relax overly restrictive
  filters or try a related tag before giving up.
- For "play some music", "put on the radio" or another broad request, choose
  and start a station yourself. Use known household preferences where relevant;
  otherwise choose a varied music station. Do not ask for a station or genre.
  Choose music rather than Radio 4, talk or news when the user asks for music.
  Use radio.random for an appropriate curated music genre or radio.search for
  broader discovery. Do not merely return candidates for the user to select.
- Never require the user to select a station you can identify unambiguously.
  If a stream fails, try another suitable result and report the actual outcome.

For scheduled radio use real station IDs; prefer curated stations. A requested start is not confirmation of audible playback.

# Saved radio stations

Use radio.favourites to read the user's saved stations, radio.save with a returned
station id to save one, and radio.remove with its id to remove a favourite. Saved
stations remain playable after search caches expire or the device restarts.
