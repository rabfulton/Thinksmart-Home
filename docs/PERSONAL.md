# Notes, themes and local actions

The built-in assistant can save notes, edit colour themes, save named routines,
and create timers or scheduled actions. These changes need no compiler, coding
agent or shell access. The Notes dashboard page has Notes, Routines and Schedules
sections. Delete entries by touch; run routines and toggle schedules there too.

Try:

- “Make a dark plum theme with amber highlights.”
- “Save a note: remember to order coffee.”
- “Read my notes” or “Delete the note about coffee.”
- “Save an evening routine that selects autumn and plays jazz at 30 percent.”
- “Run my evening routine.”
- “Play BBC Radio 4 at nine on weekdays.”
- “Stop the radio in half an hour.”
- “Remind me to check the oven in twenty minutes.”

**Dictate** starts a voice session restricted to saving one note. Dictated commands
are note content, not instructions to control the device. It uses the selected
voice API and requires the assistant to be enabled and configured. Stop cancels
the current session; a note already saved remains saved. Regular spoken requests
to Sheila can also create, read, update and delete notes.

## Files and editing over SSH

The installer uses `/var/lib/thinksmart-home/personal/`, owned by `thinksmart`:

| Path inside that directory | Contents |
| --- | --- |
| `themes/*.json` | One complete palette per file; filename equals its `id` |
| `themes/*.json.previous` | Previous palette for voice undo |
| `notes.json` | Saved text, optional titles, creation/update times |
| `routines.json` | Named sequences of supported actions |
| `schedules.json` | Timing, enable state, next execution and last outcome |

The three collection files have the form `{"version":1,"items":[...]}`. They are
created when needed. Files use mode 600 and directories 700. No note text is
written to routine diagnostic logs. Back up this directory to retain your data.
Installation upgrades preserve it, including SSH-edited palettes.

Edits reload automatically. Use valid JSON and preferably save through a temporary
file followed by an atomic rename. For bulk edits to schedules/routines, stop
`thinksmart-actions.service` first and avoid simultaneous assistant/UI edits.
The app refuses to overwrite malformed collection files; fix them over SSH.
`THINKSMART_PERSONAL_DIR` overrides the location for development.

## Themes

The six original palettes are plain JSON under `app/themes/` and seeded on device
installation. Built-in resource copies remain as fallbacks. A theme file contains
`version: 1`, a lowercase filename `id`, and `colors` with these twelve keys:

```text
background surface raised border ink muted faint
accent tint accentInk secondary danger
```

Colours are `#RRGGBB`. Assistant edits can specify a base palette and only the
colours to change. Main text and accent-button text must retain at least 4.5:1
contrast. Existing-theme edits keep one previous version; ask to undo that theme.
Creating or selecting a theme changes the display immediately unless save used
`activate: false`. Custom palettes appear in the existing theme-button cycle.

## Routines and schedules

Allowed steps are radio playback/random genre, media play/pause/stop/volume,
theme selection, screen sleep/wake and a local alarm with text. There are at most
12 steps per routine. No shell commands, scripts, executable QML or arbitrary file
writes are accepted. Disabled modules remain unavailable to scheduled actions.

An example saved routine is:

```json
{
  "id": "evening",
  "title": "Evening",
  "steps": [
    {"action": "theme.apply", "id": "autumn"},
    {"action": "media.volume", "percent": 30},
    {"action": "radio.random", "genre": "jazz"}
  ]
}
```

Schedules contain either `routineId` or inline `steps`. A scheduled routine uses
its current steps when it runs; remove referencing schedules before deleting
that routine. Prefer curated radio IDs for schedules, since directory search
results and external streams can become unavailable.

Timing can be a one-off future `at` (ISO 8601 with Z/offset), a relative
`afterSeconds` supplied to the creation tool, or recurring local `time` (`HH:mm`)
with `weekdays` (Monday=1 through Sunday=7) and an IANA `timezone`. The configured
weather timezone is the default. The service stores `nextAt` in UTC, recalculates
recurrences in the specified timezone, and runs only once during an autumn clock
repetition. An event in the spring clock gap moves forward with the clock change.

`thinksmart-actions.service` is a small independent local scheduler, supervised
by systemd. It does not use an AI API or microphone. It can execute while the
screen sleeps and while the assistant is disabled. Saved schedules continue
until individually disabled or deleted. It needs the dashboard to accept actions;
after a brief dashboard restart, events up to 60 seconds late can still run.
Older events are marked missed. Execution is claimed on disk before requesting
actions, so a service restart does not replay an event. An interrupted request
can fail without retry; inspect the status in Schedules.

Alarms wake the display and show a dismiss button, stop existing media and sound
a local two-tone alert for up to one minute. They use the overall speaker volume
and do not override mute. Add a volume step if you want a specific alarm level.
Radio schedules request playback; a successful request does not guarantee that
a remote stream is available. Scheduling cannot power on a shut-down device.

## Local JSON interface

The same validated operations are available to the assistant and local control
socket. For example:

```sh
thinksmart-home --command '{"action":"notes.add","data":{"text":"Order coffee"}}'
thinksmart-home --command '{"action":"schedule.list"}'
thinksmart-home --command '{"action":"schedule.save","data":{"title":"Sleep timer","afterSeconds":1800,"steps":[{"action":"media.stop"}]}}'
```

`system.capabilities` lists available operations. The separate `personal_action`
voice tool advertises schemas and usage instructions to both Realtime and Live.
Notes are supplied to the cloud model when requested or dictated; they are not
automatically inserted into every conversation.
