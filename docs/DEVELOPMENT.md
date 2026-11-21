# Development

The application is C++17/Qt 6.8 Quick under `app/`, with QML views and a small
procedural shader. Audio helpers are under `app/audio/`. No device credentials
are needed to compile or run the mocked UI/protocol tests.

## Builders

```sh
./tools/build-home.sh
```

The base Dockerfile supplies Debian's ARM64 cross compiler; Dockerfile.qt adds
Qt and libmpv. Runtime drivers remain on the device. Optional audio/chess builders
are separate scripts and assets stay under ignored `build/`.

For native tests, using the same builder:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" \
  --mount type=bind,src="$PWD",dst=/workspace \
  thinksmart-qt-builder:debian13 sh -ec '
    cmake -S app -B build/home-native -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build/home-native --parallel 4
    QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software QT_QUICK_BACKEND=software \
      QT_QUICK_CONTROLS_STYLE=Basic ctest --test-dir build/home-native --output-on-failure
  '
```

Focused tests can be selected by QtTest function name, for example:

```sh
build/home-native/home-ui-tests moduleProfiles \
  deviceConfigurationChangesLocationAndRejectsInvalidFlags \
  dashboardPagesButtonsSwipesAndAppReturn \
  voiceSessionStreamsInterruptsAndDisables liveManagedToolsAndFullDuplex
```

Module creation and third-party contribution are covered in
[MODULE-DEVELOPMENT.md](MODULE-DEVELOPMENT.md). Stream compatibility and
measured decoder limitations are in [VIDEO-COMPATIBILITY.md](VIDEO-COMPATIBILITY.md).

`markets-tests` covers quote/session normalization, rolling crypto comparisons,
atomic watchlists, one-shot crossing alerts, replay/stale suppression, free-feed
backoff and disabled-module inactivity. Markets UI and both voice tool paths use
local fixtures in `home-ui-tests`. See [MARKETS.md](MARKETS.md).

`personal-tests` covers atomic personal-data edits, theme validation/undo,
schedule persistence, missed events, weekday/DST handling and allowed actions.
The UI tests exercise notes, touch deletion, scheduler execution and dictation
through local mock Realtime/Live servers. They create no paid sessions.

`network-tests` runs against an isolated D-Bus NetworkManager mock. The Wi-Fi UI
test uses synthetic access points and actual Qt Virtual Keyboard key taps in
both orientations. `thinksmart-keyboard-check` is a short standalone device
probe: a synthetic field and one programmatic keypress, with no Wi-Fi or voice
activity. The builder creates a user entry for the supplied `BUILDER_UID`/GID so
the private D-Bus test daemon can authenticate the normal unprivileged test user.

Run those inside the builder with the same offscreen environment. Voice tests
use a loopback mock WebSocket, dummy credentials and fake audio processes; they
do not contact OpenAI or capture a microphone. Public tests generate silent WAV
fixtures instead of relying on the developer's voice-preview collection.

## Public screenshots

`capturePublicScreenshots` renders the real UI with synthetic London weather,
energy readings, news fixtures, example recipes and synthetic market prices.
It starts no cloud conversation. Render with
Xvfb/OpenGL for the GPU orb, or offscreen/software for the Canvas fallback:

```sh
./tools/render-screenshots.sh
# Refresh only selected images:
THINKSMART_PUBLIC_SCREENSHOT_SET=recipes,markets,chess ./tools/render-screenshots.sh
```

Review the generated PNGs before publishing; use no real household data, server
artwork, account identifiers or microphone recordings.

## Prepare a public snapshot

```sh
python3 tools/export-public.py --output build/public/thinksmart-home
python3 tools/check-public.py build/public/thinksmart-home
```

The exporter uses an explicit source/tool/document allowlist and creates a clean
snapshot. It excludes private data, build/firmware images, historical personal
docs, recordings, preview collections, model weights and Git history. It does
not publish anything. The check scans the actual export, validates linked local
documentation/assets and flags credentials or forbidden file types/locations.

Commit and publish only that reviewed snapshot. Keep imported licence notices
and source provenance. Root MIT licensing applies to our original work; see
[THIRD_PARTY.md](../THIRD_PARTY.md) before distributing linked binaries.

News source presets and one/two-column preferences are covered by `news-tests`;
`newsColumnSettingsEditor` and `newsPanelsReaderAndReplies` in `home-ui-tests`
cover the editor, orientation, article reader, HN replies and vertical scrolling.
See [News configuration](NEWS.md).

`audiobooks-tests` uses a local HTTP fixture and playback event signals to check
chapter-aware resume, failed loads, EOF-only advancement, cancellation, discovery
caching and disabled-module inactivity. Audiobook UI and both speech protocols
have mocked tests in home-ui-tests; no book audio or paid calls are used.

`THINKSMART_ENABLE_AUDIOBOOKS_PROXY` is a CMake option, OFF by default. The
optional adapter works with a separately installed local HTTP proxy such as
WARP; no vendor software or registration data is shipped. Configure with
`-DTHINKSMART_ENABLE_AUDIOBOOKS_PROXY=ON` to include it. The build-home script
forwards additional CMake arguments and explicitly resets this option to OFF
unless overridden, even when reusing an existing build cache.

Run `audiobooks-tests` in both configurations when changing proxy support. OFF
checks that the environment setting has no effect. ON checks module-specific
routing of Archive artwork, discovery and unrelated requests staying direct,
invalid settings and proxy failure without direct fallback. Both configurations
also check the bundled 100-link list survives a failed update. The app,
audiobook tests and UI/voice mocks receive the
same compile definition. TLS verification remains enabled in either build.

`tvarchive-tests` checks derivative selection, stable IDs, season ordering, unsafe
paths, persistence, failed resumes, EOF-only advancement, autoplay, random scope,
cache failures and disabled-module inactivity. `tvArchiveBrowserAndProgressControls`
and `tvArchiveVoiceTools` in home-ui-tests cover both orientations and mocked
Realtime/Live integration. Tests use synthetic metadata and no TV streams.

`THINKSMART_ENABLE_TVARCHIVE_PROXY` is separately OFF by default. Test ON and OFF
when editing this adapter. The proxy tests check invalid configuration and failure
without direct fallback; unrelated networking must remain direct. Existing
Audiobooks proxy tests also cover the shared loopback proxy validator.

Weather location lookup uses local HTTP fixtures in `weather-radio-tests`, including
location persistence, separate caches, cancellation of an old forecast and local
sunrise/sunset times. `weatherIconsLocationAndSolarLayout` in `home-ui-tests`
checks WMO/day-night icon states, the location picker and card bounds in both
orientations. Set `THINKSMART_WEATHER_SCREENSHOTS` to an output directory to render
its synthetic forecast screens. No live geocoding or voice calls are used.

`python3 tools/test-appliance.py` checks the optional on-demand Archive proxy's
readiness handling and systemd socket forwarding against a local echo fixture.
It also checks that an open connection survives the idle timeout and that the
forwarder exits and can be activated again after clients disconnect. No vendor
client, microphone or external service is used. See [appliance setup](APPLIANCE.md).

Recipe storage/generation tests: `recipes-tests`; reader/task-list/settings checks:
`recipesReaderHistorySettingsAndTouchTasks`; mocked Realtime/Live tools:
`recipesVoiceTools`. See [Recipes](RECIPES.md) for files, API behaviour and adaptation.

`layout-tests` covers guided application placement/removal, shared Music providers,
full pages, capacity limits and atomic drafts. `layoutEditorDraftAndApply`,
`applicationWidgetsAndTemporaryViews` and `applicationOpenVoiceTools` cover the
new flow, News/Notes/Chess launcher cards and expanded views in both orientations.
The app-opening voice tests use local Realtime/Live mocks, verify disabled apps
are absent/rejected, preserve the layout and allow acknowledgement after navigation.

`library-tests` covers Navidrome authentication/queues and Jellyfin playback, plus
private connection edits, account-to-token login, cancellation and preserving
the other server configuration. `mediaConnectionSettingsEditor` in `home-ui-tests`
checks masked fields, cancellation, validation and keyboard-sized layouts in both
orientations. All use local fixtures, with no real server credentials.

Music browsing: `navidromeLazyLoadingFiltersAndSearch` in `library-tests` covers
append-only album/search paging, genre and year-range parameters, album return,
end-of-list and failed loads with private cached entries. `musicLibraryCompactBrowsing`
in `home-ui-tests` checks both orientations, stable scrolling, compact controls,
genre/decade selection, album Back navigation and search submission using local fixtures.

Music queues: `musicQueuePersistsReplaysAndClears` in `library-tests` checks
append-only selections, duplicates, browsing independence, completion versus
manual navigation, stale completion events, account separation, private atomic
persistence and failed clears. `musicQueueControls` in `home-ui-tests` exercises
track/album enqueue, automatic startup/radio handoff, append without restarting
active music, the Queue tab, bottom transport, replay and clear in both
orientations. A dummy media process is used, with no speaker or cloud calls.

`vrm-tests` also checks widget freshness with current battery/solar and retired
AC readings, old or unknown battery timestamps, and solar-only fallback. These
cache fixtures enter no network event loop and do not contact VRM.

`musicAgentSearchSelectionAndCancellation` in `library-tests` covers bounded
artist/album/song discovery, typed IDs, queue replacement versus append, genre
filtering, malformed IDs, network failures and cancelled lookups preserving the
queue. `musicVoiceTools` exercises Realtime, Live delegation and legacy Realtime
audio handoff through mock WebSockets and a dummy player. It also checks that
ending a conversation discards a late playback result. `moduleSpeechSchemas`
and `layout-tests` verify disabled-module tools and prompt filtering. No paid
speech calls or real microphone tests are used.

`voiceDismissal` covers explicit dismissal, false activation, dialogue no longer
addressed to Sheila and clear conversation completion through both Realtime/Live
mock protocols. `liveSilenceDoesNotHoldMediaOrIdleSession` now exercises both
providers and checks the actual eight-second quiet cutoff and return to local
wake listening, including silent Live output packets. No cloud calls are made.

`memory-tests` covers bounded UTF-8 Markdown persistence, exact patches, external
SSH edits, malformed-file preservation, confirmed reset scope, locking, private
permissions, symlinks, credential-pattern rejection and disabled-module gating.
`memoryVoiceTools` tests both mock voice protocols across successive sessions:
save/patch, load the next wake, reject an unconfirmed reset, clear, then verify
the next session excludes the erased fact. `assistantMemoryResetButton` covers
cancel/confirm in both orientations with local temporary files.
