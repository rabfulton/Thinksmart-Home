# Configuration

Prepare your local settings without contacting a device:

```sh
python3 tools/configure-device.py --disable vrm --disable navidrome --disable jellyfin
```

This copies `config/device.example.json` to ignored `private/device.json`.
Edit the copy to select modules and set your weather name, latitude, longitude
and IANA timezone. `true`/`false` values are JSON booleans. Missing module flags
default to enabled for compatibility, except IPTV, Markets, Audiobooks and TV Archive which default to disabled. The example explicitly lists every module.

```sh
python3 tools/configure-device.py --config private/device.json --check
```

The installer deploys this non-secret file to `/etc/thinksmart/device.json`.
Changing it requires an app restart. Device-side edits in Settings → Pages &
modules save a separate `dashboard.json` override; it takes precedence over the
deployment module defaults and is preserved on upgrades. Invalid configuration disables optional
modules and displays an error, leaving SSH available to fix the file.

## SSH

Copy `config/ssh.env.example` to `private/ssh.env`. Supply your device's LAN or
tailnet address, administrative user and the path to your client SSH key. The
helper supports `THINKSMART_HOST`, `THINKSMART_USER`, `THINKSMART_SSH_KEY`,
`THINKSMART_KNOWN_HOSTS`, `THINKSMART_SSH_PORT` and optional
`THINKSMART_HOST_KEY_ALIAS`.

Verify the device's SSH fingerprint through your installation process. Establish
the initial trusted connection using your chosen known-hosts file; subsequent
helper connections require an existing match:

```sh
mkdir -p private
chmod 700 private
ssh -i ~/.ssh/id_ed25519 -o UserKnownHostsFile=private/known_hosts root@YOUR_DEVICE
./tools/thinksmart-ssh 'hostname'
```

## Credentials

Create only the files needed by your selected modules:

| Local file | Template | Installed file |
| --- | --- | --- |
| `private/voice.json` | `config/voice.example.json` | `~thinksmart/.config/ThinkSmart/Home/voice.json` |
| `private/media.json` | `config/media.example.json` | `~thinksmart/.config/ThinkSmart/Home/media.json` |
| `private/vrm.json` | `config/vrm.example.json` | `~thinksmart/.config/ThinkSmart/Home/vrm.json` |

These local files are ignored. Use mode 600 and fill credentials in your editor,
not chat. Device files are owned by `thinksmart:thinksmart`, mode 600.
Navidrome/Jellyfin and VRM do not require an OpenAI key. Disabling Assistant stops
its microphone and API activity. OpenAI keys are needed only for cloud voice.

`voice.json` supplies defaults; saved UI model and per-model voice selections
take priority. Supported model IDs come from `VoiceAssistant::models()`; check
availability in your own API account. New API protocols need an implementation,
not just a model-name edit.

## Prompts and personalisation

On the device, edit `/etc/thinksmart/assistant/`:

- `live.md`: conversation, interruptions and delegation.
- `backend.md`: GPT Live reasoning/tool workflows.
- `system.md`: Realtime instructions.
- `household.md`: your household notes, initially blank of personal facts.
- `personalities/*.md`: selectable personalities named by filename.
- `wakeword.json`: local wake sensitivity.

Live gets its policy, personality and household notes. Its backend gets the
backend policy and household notes. Realtime gets system/personality/household.
The application also supplies the configured location and enabled capabilities.
Keep Live's SpawnThinking policy headings intact. Files reload at each new
conversation; Settings → Reload updates the personality list. No build is needed.

Prompt files use UTF-8, at most 32 KiB each and 64 KiB per assembled prompt.
Keep them readable by `thinksmart` and private. Upgrades seed missing defaults
and preserve existing prompts. See [ADMIN.md](ADMIN.md) for all file locations.

Themes, notes, routines and schedules live in
`/var/lib/thinksmart-home/personal/`. They can be edited by the built-in assistant
or over SSH and survive application upgrades. See [personal actions](PERSONAL.md).

TV Archive is an optional `tvarchive` widget, off by default. Add it to an existing
page after enabling it. Its progress, autoplay and optional module-only proxy are
described in [TV-ARCHIVE.md](TV-ARCHIVE.md).

Weather location can also be changed on the device: **Weather → Location**. The
selection is saved separately in `~thinksmart/.config/ThinkSmart/Home/weather.json`
and takes precedence over the installed default. See [Weather](WEATHER.md).
