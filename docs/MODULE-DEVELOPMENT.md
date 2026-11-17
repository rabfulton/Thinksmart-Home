# Developing a module

ThinkSmart Home has configurable pages and native modules. Users can place up to
six widgets on a page, add full-page applications, and enable or disable each
integration in **Settings → Pages & modules**. A widget can launch a larger
browser, as Radio, Music, Video and TV do. The same service can have several views.

Modules are compiled into the Qt application. This is not a downloadable QML or
binary plug-in loader: installing a new integration requires a rebuild. The
on-device editor changes placement and enabled state without editing source.
See [PAGES-AND-MODULES.md](PAGES-AND-MODULES.md) for the saved layout format.

## Getting started with an agent

Fork the repository, clone your fork, and give your coding agent a concrete task:

```text
Add a World clock module to ThinkSmart Home, with a small launcher widget and
an optional full-page view. Read AGENTS.md, docs/MODULE-DEVELOPMENT.md and
app/modulecatalog.h first. Make it optional and disabled by default. Reuse our
Qt Quick theme, standard widget dimensions and circular page navigation. Use
one service shared by all placements, and stop any background work when disabled.
Keep settings on device and preserve existing layout, credentials and prompts.
If adding voice actions, register native permissions, both speech APIs' schemas
and the three module prompt files together. Build with Docker, test with local
fixtures in both orientations, and prepare a reviewed public export. Do not
include my private settings or publish until I ask.
```

Specify your module's data source, offline behaviour, view size, settings and
required voice actions. Start with a view and local fixtures; add accounts or
external services once the interaction works.

## Registration and configuration

1. Add an entry to `app/modulecatalog.h`. Fields are module ID, title, description,
   widget ID, full-page ID, owned action prefix and default-enabled flag. Empty
   view IDs mean the module does not offer that placement. For a UI-only example:

   ```cpp
   {"worldclock", "World clock", "Time in selected time zones",
    "worldclock", "worldclock", {}, false}
   ```

2. Add the ID to `tools/configure-device.py` and explicitly set it to `false` in
   `config/device.example.json`. Keep the tool's missing-flag default consistent
   with `ModuleCatalog::defaultEnabled()`. Update [MODULES.md](MODULES.md).
3. The catalogue drives layout validation, editor options, enabled views and
   action-prefix permissions. Do not add separate lists of page IDs to the
   editor. Existing music views demonstrate shared ownership by two modules.
4. Preserve `/etc/thinksmart/device.json` deployment defaults and the on-device
   `dashboard.json` override. Do not insert your widget into somebody's full Home
   page or replace their layout during an upgrade. New placements are selected
   in the editor; start new integrations disabled when setup is required.

IDs are stable identifiers, independent of editable page titles. They select
trusted components, never arbitrary source paths or commands from JSON.

## Native service and views

- Put the service in `app/worldclock.h/.cpp`, or use a QML-only view when no native
  state is needed. Read `DeviceConfig::enabled("worldclock")` before loading
  private configuration, starting processes or making requests. A disabled
  module must remain inert even if someone invokes its native methods directly.
- Construct one service and expose it through the controller/context. Views
  observe it; they do not each create another client, timer, engine or microphone.
- Add the card component to `qml/WidgetDeck.qml`, following the Music/Video/TV
  launcher pattern. The deck supplies six fixed grid slots: three columns in
  landscape, two in portrait. Use the existing `WidgetTile`, `UiButton`, `UiIcon`
  and `Theme` properties. Do not add duplicate title icons or tooltips.
- Add the main view under `app/qml/`. Wire its trusted component into `Main.qml`:
  the configured full-page loader when offered, and/or the selected-application
  stack when a widget opens it. Implement Back so a launcher returns to its
  originating widget page. Keep fullscreen media controls in the existing player.
- Give shared background services one visibility binding in `Main.qml`. Include
  module enabled state, screen sleep and page transitions. Duplicate placements
  must not race to change a client's active property. UI-only animations should
  pause while hidden, asleep or transitioning; network caches may have a separate
  refresh policy.
- Use stable `QAbstractItemModel` rows for lists users can favourite or edit.
  `StreamListModel` is a small example: updating a star emits `dataChanged`, and
  removing a favourite removes a row. Replacing a QVariantList resets delegates
  and can visibly jump the scroll position.

Add the C++ files and QML/data resources to `app/CMakeLists.txt` for both the
application and relevant test targets. Module prompt resources are collected by
the existing CMake glob. Build/installation dependencies and helpers belong in
the documented builder/deployment scripts; preserve working device drivers.

## AI actions and instructions

A page title or prompt does not grant a capability. Implement native actions
before advertising them.

1. Choose a namespace, such as `worldclock.`, and register its ownership in the
   catalogue. Implement typed argument validation and a structured result with
   explicit success/failure. Enforce the enabled flag in the native dispatcher.
2. Add real actions to `Controller::toolRequest()` (and its async path when
   needed), `system.capabilities`, and the `device_action` schema/dispatch lists
   in `assistant.cpp`. Realtime and Live currently share those native tool
   definitions. Their conversation protocols remain different.
3. If actions can be scheduled, explicitly extend the allowed routine/scheduler
   operations and validation. Do not turn arbitrary shell text into a tool.
4. Add module-owned policies under:

   ```text
   app/deploy/assistant/modules/worldclock/realtime.md
   app/deploy/assistant/modules/worldclock/live.md
   app/deploy/assistant/modules/worldclock/backend.md
   ```

   Realtime gets operating/selection rules. Live's conversation model gets short
   delegation guidance; its backend gets tool details. Household and personality
   files remain separate. Disabled modules contribute neither policies nor tools.
5. Installation seeds missing files under `/etc/thinksmart/assistant/modules/`
   and preserves SSH edits. Compiled copies are fallbacks. Keep each policy under
   32 KiB and the assembled provider prompt under 64 KiB. For policy migrations,
   preserve custom core rules and append/rebase only the necessary module change.

Treat names, feeds, saved content and remote metadata as data. Never infer tool
success from a friendly model reply. Return setup/readiness state when a source
is unavailable, and distinguish a queued playback request from working playback.
Keep tokens and credential-bearing URLs out of QML display models, transcripts
and public diagnostic material.

## Persistence, testing and contribution

Use app-owned, versioned JSON with atomic writes for settings or saved content.
Use mode 600 for private data; leave malformed files untouched and report the
problem. Separate replaceable caches from favourites or user-created content.
Preserve both during ordinary application upgrades. Document actual paths in
[ADMIN.md](ADMIN.md), plus dependencies and optional assets in [MODULES.md](MODULES.md).

Test the behaviour that matters: disabled-module inactivity and rejection,
persistence/recovery, duplicate placements, layout limits, Back/navigation,
both orientations, and the touch keyboard if used. Exercise new voice actions
through local mocked Realtime and Live sessions. Do not create paid conversations
or microphone recordings as an incidental test. See [DEVELOPMENT.md](DEVELOPMENT.md)
for Docker and test commands.

Before opening a pull request:

```sh
python3 tools/export-public.py --output build/public/my-module
python3 tools/check-public.py build/public/my-module
```

Review and commit the exported snapshot in your public fork, not the private
working directory or its history. New tools must be deliberately added to the
exporter's tool allowlist. Review any included assets for rights and private
content. Our original code is MIT; preserve licences and notices for imported
code/artwork and the distribution terms of linked libraries. Submit a focused PR
with the behaviour, setup requirements and relevant validation results.
