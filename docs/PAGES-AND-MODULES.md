# Configurable pages and module-owned AI integration

The on-device **Settings → Pages & modules** editor configures installed native
modules and their views. See [MODULES.md](MODULES.md) for everyday use. This guide
describes the configuration and extension points.

## User-facing behaviour

Settings provides a **Pages & modules** editor, accessible from the existing gear
regardless of which modules are enabled. It has two views:

- **Pages:** add, name, reorder or remove pages. A widget page contains up to six
  widgets, arranged in three columns in landscape and two in portrait. Widgets
  can be added, removed and reordered. A full-page view contains one module,
  such as Chess, Notes or News, using its existing interface.
- **Modules:** enable or disable installed integrations independently of their
  placement. Show whether an integration needs credentials or an optional
  runtime in its description. Enabling an integration does not supply missing
  dependencies. Assistant's separate master switch is preserved.

The editor uses touch-friendly move buttons and a widget picker. Qt Virtual Keyboard supplies page naming. The top bar uses a bounded horizontally scrollable page selector. Swipes keep their current
direction and circular navigation.

The first page remains an accessible Home entry; its widgets can be edited.
Allow an empty page with an **Add widgets** action. Keep at least one page (maximum 24) and
keep Settings independent of layout content. Full-page modules are not squeezed
into arbitrary grid cells. Future modules may explicitly supply both a widget
and a full-page view.

Removing a placement does not disable its integration. This permits an assistant
or scheduled action to use a module that has no visible widget. A widget may be
shown on more than one page, backed by one shared service; reject duplicates
within a single page. Disabling a module makes its placements unavailable,
without deleting their configuration or user data. Re-enabling restores them.
TV is normally a launcher widget opening its channel browser; older full-page
placements remain supported. Cards keep their size on partially filled pages.
For Music, its widget is available when either Navidrome or Bluetooth is enabled.

The editor works on a draft. **Cancel** discards it. **Apply & restart dashboard**
validates and saves it atomically, then restarts the application through the
existing session supervisor. This interrupts playback/conversation, but does
not reboot the device or interrupt SSH. Begin with this explicit apply boundary;
fully live reconfiguration needs separate lifecycle work.

## One catalogue for modules and their views

Give every module a stable ID and a single registration describing:

- Human-readable name, description and setup requirements.
- Available widget/full-page view IDs and their native QML components.
- Service ownership, dependencies and start/stop policy.
- Native AI tool/schema contributions and allowed action IDs.
- Editable instruction files for Realtime, Live conversation and Live reasoning.

The existing IDs remain: assistant, radio, weather, news, chess, notes, bluetooth,
vrm, navidrome, jellyfin and iptv. View IDs may differ: the energy widget belongs to
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
a minimal Home/Settings layout. Review the switches and Apply to recover. SSH remains the independent recovery route.

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
