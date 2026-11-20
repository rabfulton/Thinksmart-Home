# Pages and applications

Open **Settings → Pages & modules**. Changes are staged until **Apply & restart**;
**Cancel changes** discards the draft. Applying restarts the dashboard and stops
playback/conversation, while SSH stays available.

## Add, move and remove

1. **Add application** lists only inactive applications. Choose one.
2. If it has both views, choose **Widget** or **Full page**. News, Notes, Chess,
   Markets, Recipes and IPTV support both. A widget is a small launcher that
   opens the full application; a full-page view fills its own dashboard page.
3. For a widget, choose an existing widget page with room or **New page**.
   Each page holds up to six cards. Full-page applications automatically get a
   dedicated new page; they cannot be inserted into another application's page.
4. **Open on request** enables an application without a permanent card or page.
   Sheila can open it when asked. It is still an active application.
5. **Apply & restart** saves the result.

Use **Placement** beside an active application to change its view or location.
Use **Remove application** to select one or more active apps, then **Remove
selected**. This disables their integrations and removes their placements while
keeping credentials and saved data. Empty pages left by removal are cleaned up;
Home always remains. Shared Music cards remain while Bluetooth or Navidrome still
uses them. Applications already active do not appear in Add.

**Pages** manages names, order and card order. Removing a card or page here leaves
its application enabled for opening on request. Existing layouts, including
manually configured duplicate placements, remain valid; the guided Placement
flow moves an application's placements instead of silently duplicating them.
Home can be empty; up to 24 pages are supported. Page tabs retain circular swipe
navigation. News retains vertical scrolling in either view.

## Opening an application once

Ask Sheila to open an enabled application, for example **“Open chess”**. The
shared Realtime/Live `open_application` tool opens its existing page, or a
transient expanded view when it has no page. No page or card is added and no
module is enabled. Back closes the expanded view; Recipes uses a horizontal swipe. Chess keeps its saved game,
and its engine runs only while the chess view is active. Opening a view does not
reset games, start media, or generate a new paid recipe.

The conversation can complete its acknowledgement and close normally after
opening the application. It does not pull you back to the previous screen when
the requested app is now open. Disabled apps are omitted from the tool's choices
and rejected by the native dispatcher. Add them in Settings first.

## One catalogue for modules and their views

Give every module a stable ID and a single registration describing:

- Human-readable name, description and setup requirements.
- Available widget/full-page view IDs and their native QML components.
- Service ownership, dependencies and start/stop policy.
- Native AI tool/schema contributions and allowed action IDs.
- Editable instruction files for Realtime, Live conversation and Live reasoning.

The existing IDs remain: assistant, radio, weather, news, chess, notes, bluetooth,
vrm, navidrome, jellyfin, iptv, markets, audiobooks, tvarchive and recipes. View IDs may differ: the energy widget belongs to
vrm; the video widget belongs to jellyfin; music can use either of two providers.
This relationship belongs in the catalogue rather than scattered UI conditions.

Each service has one instance. Widgets observe that instance; adding a second
view must not create another poller, player, microphone or engine. Keep existing
rules that pause animation while hidden, during navigation and during screen
sleep. Chess's engine follows the active page, not merely the presence of a
registered view. News retains its existing shared daily refresh/cache policy.

Module registration describes installed, trusted code. It does not load arbitrary
QML paths, shell commands or executable tools from a user-edited layout. A new
native integration still needs implementation, registration and a build. The
goal is to avoid editing several central UI and prompt lists for every addition.
UI-only modules contribute no AI tools until those tools actually exist.

## Persistent configuration

Keep `/etc/thinksmart/device.json` as the deployment defaults and location.
Store on-device customisation in one app-owned file:

`/var/lib/thinksmart-home/.config/ThinkSmart/Home/dashboard.json`

It contains module overrides and page placement together, so one atomic commit
cannot save half a layout change. No credentials or prompt contents go in this
file. An example format is:

```json
{
  "version": 1,
  "modules": {
    "news": true,
    "chess": true,
    "notes": true
  },
  "pages": [
    {
      "id": "home",
      "title": "Home",
      "kind": "widgets",
      "widgets": ["assistant", "weather", "energy", "radio", "music", "video"]
    },
    {"id": "chess", "title": "Chess", "kind": "module", "module": "chess"},
    {"id": "notes", "title": "Notes", "kind": "module", "module": "notes"},
    {"id": "news", "title": "News", "kind": "module", "module": "news"}
  ]
}
```

Page IDs are stable and separate from editable titles. Validate schema version,
known IDs, unique page IDs, supported view types, names and the six-widget limit.
An installation without this file retains its present layout and module choices.
Upgrades preserve the file, existing credentials, notes, schedules, themes,
household knowledge and personality selection.

Retain a last-known-good configuration and expose errors. A malformed saved
configuration must not unexpectedly enable a disabled microphone/integration:
recover the valid backup layout with optional modules stopped, or fall back to
a minimal Home/Settings layout. Add the wanted applications and Apply to recover. SSH remains the independent recovery route.

## AI instructions and tools follow enabled modules

Compose each new conversation from:

1. Core provider policy: conversation, delegation, interruptions, dismissal,
   honesty and handling untrusted tool results.
2. Selected personality and shared household context, with their existing
   provider-specific placement.
3. Instructions and capabilities from enabled modules only.

Give modules their own editable prompt directory, for example:

```text
/etc/thinksmart/assistant/modules/radio/realtime.md
/etc/thinksmart/assistant/modules/radio/live.md
/etc/thinksmart/assistant/modules/radio/backend.md
```

Realtime receives the radio operation and selection rules. Live's conversation
model receives short delegation guidance; its reasoning backend receives detailed
station selection/tool rules. Other modules follow the same provider-specific
pattern. Core policy is shared; module instructions are included or omitted,
rather than replacing the entire household/personality prompt when one module
changes. Reload edited module files at conversation boundaries, as current
prompt files are reloaded.

The same enabled catalogue drives instruction assembly, tool schemas,
advertised capabilities and the native dispatch permission check. Disabling
Radio must remove its station-selection rules and radio tool arguments as well
as stopping its UI/service; a residual generic tool description must not keep
advertising radio. The same rules apply to nested routine/schedule actions.
Server-side tool selection does not grant a disabled native capability.

Hiding or moving a widget does not alter the AI's capability set. Enabling a
module without its required setup must not claim successful access: expose its
setup state and only offer usable capabilities. Both speech APIs use the same
module catalogue, with their distinct prompting conventions.

The shipped system/live/backend policies contain core rules, with operational
module rules in the module files. When migrating an older installation, review
and preserve SSH-edited policies: back them up, move module-specific rules into
the matching files, and retain custom core instructions. Do not silently overwrite user changes or
claim that appending new blocks removes old module rules. Household and
personality files remain intact.

Disabling modules does not delete saved schedules. Validate every action against
the currently enabled capabilities when it executes and report unavailable
actions; do not run disabled operations or replay missed jobs unexpectedly.

## Extension points and checks

`app/modulecatalog.h` registers native module IDs, names, descriptions, widget and
full-page IDs, and action-prefix ownership. `dashboardconfig.h` validates and
loads configuration; `layouteditor.*` owns the draft and atomic save/restart.
`WidgetDeck.qml` contains reusable native card views; `Main.qml` loads configured
pages. Adding a view also needs its trusted QML component/resource registration.
Unknown layout IDs never load arbitrary source paths.

Native tool handlers and schemas remain typed C++ code. For new operations,
register their module ownership, implement their dispatcher/schema contribution,
and add only the needed provider-specific module instructions. UI-only modules
must not advertise tools that do not exist. The common catalogue filters existing
actions and nested scheduled/routine action schemas.

Module instruction files have compiled defaults. An SSH-edited file under
`/etc/thinksmart/assistant/modules/ID/{realtime,live,backend}.md` overrides its
compiled default. Missing local files use the compiled copy. Disabled modules
are not read, even if their files are malformed. The installer seeds missing
files and preserves edits. Keep each prompt under 32 KiB and each assembled
provider prompt under 64 KiB.

`layout-tests` covers schema boundaries, six-widget limits, draft/Cancel,
module persistence, fail-closed recovery and per-provider instruction filtering.
UI tests cover configurable page routing, editor use, keyboard naming, circular
swipes and hidden animation/engine behaviour. Realtime and Live tests use local
mock servers; no paid conversation is required.

For a step-by-step third-party module workflow, see [MODULE-DEVELOPMENT.md](MODULE-DEVELOPMENT.md).
