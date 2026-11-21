# Voice, audio and wake word

Voice is optional. The native adapter supports OpenAI Realtime and GPT Live using
the models listed in Settings. API availability/pricing belongs to your account;
this repository supplies no key or credit. Other provider keys alone do not add
a new protocol implementation.

The local audio path supports echo cancellation, a speaker-reference mix, media
ducking and local Sheila detection. Only active cloud conversations send audio
to the API. Disabling Assistant closes local listening and cloud conversation.
The module flag can remove Assistant entirely. Each model remembers its voice.

## Build and install

First build the launcher as described in [INSTALL.md](INSTALL.md). For audio:

```sh
./tools/build-audio.sh
./tools/build-wakeword.sh
./tools/build-shared-audio.sh
```

The wake build fetches pinned Sherpa-ONNX libraries/model archives and verifies
their hashes. Public setup uses the phonetic Sheila model; the developer's
personalised streaming model/recordings are deliberately absent. This fallback
has a higher CPU cost. Users can develop their own streaming model using the
included microfrontend/runtime interface and upstream microWakeWord tools.

Optional private keyword test clips can be placed at
`private/wakeword/fixtures/{sheila,unrelated}.wav`; when present, the wake build
runs offline inference checks. Otherwise test local detection on the device.
No human recordings are included in the source package.

### Activation sound and sensitivity

Assistant Settings includes **Activation sound** and **Wake sensitivity**. A short
original two-note tone plays only when a local wake detection starts a new
conversation, including from screen sleep. Manual Start and repeated Sheila
interruptions within an existing conversation do not play it. The tone uses the
voice bus, so it enters the speaker reference for echo cancellation. It opens no
API session of its own, and stopping/disabling Assistant stops the tone too.

The sensitivity scale runs from 0 to 100, with **50 / Default** in the middle.
Lower values reduce false activations; higher values make Sheila easier to wake.
The slider maps to a narrow threshold range around the configured file default
(±0.03, bounded by the supported limits). For a file default of 0.97, the range
is 0.94–0.995, with 0.97 at the midpoint; the two halves map independently.
Numbers on the interface are a sensitivity scale, not probabilities. The −/+
buttons move five points. Changes save on release and briefly restart only the
local listener; controls are disabled during a conversation. Existing actual
thresholds are retained when upgrading, even if outside the UI's narrower range.

The base threshold remains in `/etc/thinksmart/assistant/wakeword.json`.
UI overrides save as `assistant/wakeThreshold` in `Home.conf`; **Default**
removes the override and reloads that file. The tone toggle saves as
`assistant/wakeSoundEnabled` and defaults to on. The trained streaming Sheila
model uses `threshold` and its model's averaging window; `score` and
`trailingBlanks` apply to the older phonetic detector.

For occasional TV/video activations, try a small threshold increase first.
Echo cancellation covers this device's own speaker, not an unrelated television.
If false wakes persist at a high threshold, examples of those sounds as negative
training data can inform a later model revision; no recordings are saved
automatically. “Go to sleep” remains the immediate conversation-dismissal command.

On the device, shared audio requires PipeWire, WirePlumber, ALSA PipeWire plugins,
SpeexDSP and WebRTC audio processing. The existing ALSA configuration must provide
the physical `speaker` PCM and the hardware input expected by the device image.
The shared setup backs up the prior routing configuration and provides rollback.

```sh
./tools/install-wakeword.sh
./tools/install-shared-audio.sh
```

Create `private/voice.json` from `config/voice.example.json`, mode 600, and fill
the API key privately. After shared setup, install the voice credentials using
`tools/install-voice.sh`; it also redeploys the launcher. The selected shared
worker is already installed by the shared-audio installer. Review those scripts
before changing an existing audio setup.

Start with a local echo test and local wake detection before a paid conversation.
The enable switch on the conversation page, immediately left of Settings, permits local listening; Start or Sheila
opens a conversation. Realtime has VAD options; Live handles continuous listening
and interruptions through its own protocol. Live's backend handles reasoning and
tool selection using the allowed local actions. Sessions close on stop, screen
sleep, quiet timeout or the maximum duration.

When Sheila opens the conversation view after a wake word, it remembers the
previous dashboard page or expanded app and returns there when the conversation
ends, including a quiet timeout or “go to sleep”. Back also returns there.
Opening another page or an output such as a recipe cancels that automatic return,
so the new view stays open. Starting manually from the Assistant view leaves it
open afterwards. Fullscreen video is not replaced by the conversation view.

Use **open_application** to show an enabled app such as Chess, News or Notes.
It opens an existing page or a transient expanded view without changing the
layout. Acknowledgement can finish after navigation, and the opened app remains
on screen when the conversation ends. See [page/application editing](PAGES-AND-MODULES.md).

## Ending a conversation

The local `status` command includes bounded `lastStop`, `lastResponse` and
`lastInterrupt` metadata to diagnose premature cutoffs. These live in RAM only,
with fixed reason codes, timings and state flags; they contain no transcript,
audio, tool arguments or credentials and cause no log-file writes. The first
stop of an active session is retained, so later UI cleanup cannot replace its
reason. It distinguishes page navigation, display sleep, idle/session limits,
explicit dismissal and errors, plus Realtime response completion/cancellation.

Say **“go to sleep”** to dismiss Sheila. Both Realtime and Live have an
`end_conversation` tool that closes cloud audio and stops queued assistant speech
without a spoken goodbye. It keeps the assistant enabled for local wake-word
listening and leaves the screen and media selection/playback unchanged, restoring
any conversation ducking. This differs from asking to stop the radio or turn off
the display.

An available user transcript also recognises this standalone command directly;
Live allows a brief 250 ms for a partial transcript to finish. Realtime normally
uses the model tool unless input transcripts are already available. This adds no
separate transcription service. Quoted phrases and Notes dictation remain text.

Both models receive guidance that TV/radio/background speech can accidentally
trigger the wake word. They may end silently with reason `false_activation` when
there is good reason to think no one is addressing Sheila. “False alarm” and
“I wasn't talking to you” are also explicit dismissal requests. Classification is
model judgement, not a guaranteed distinction between TV and a real request.
The idle follow-up window is **8 seconds** after activity ends. Active answers,
backend/tool work, manual turns and genuine Realtime speech are protected from
that idle cutoff. Background speech can still look like activity, so both models
are also instructed to end promptly when speech is no longer addressed to Sheila,
even after a genuine exchange (`not_addressed`), or when the user clearly finishes
(`conversation_complete`). These are silent immediate endings; requested answers
must finish first. The model must not extend the exchange with unsolicited
“anything else?” questions. This remains model judgement, not speaker recognition.
Wake sensitivity is unchanged. Dismissal metadata
records only a bounded reason/source, not audio or transcripts. The policy is
supplied by the application, preserving SSH-edited prompt files.

## Local voice previews

Public source does not include third-party voice recordings without provenance.
To add your own permitted clips, put `<voice>.wav` under
`artifacts/voice-previews/` and run `python3 tools/prepare-voice-previews.py`.
The resulting `app/deploy/voice-previews/` files are installed with the launcher.
Standard PCM WAVs retain their rate; known headerless Live PCM is mono PCM16 at
24 kHz. Preview clips must be short (up to 12 seconds).

Selecting a voice plays its clip locally; the speaker/stop button replays/stops
it. Missing samples leave voice selection usable and show an unavailable message.
Previews do not open an API connection or capture audio. They stop on navigation,
screen sleep and before conversation startup.

## Prompts

See [CONFIGURATION.md](CONFIGURATION.md). Live's conversation/delegation policy
and reasoning backend instructions are separate files. Personality is supplied
to the speaking model; household notes reach both. Tool schemas and local
permissions are enforced in code, including the selected module flags.

## Idle visual cost

The home card's CPU percentage is total utilization across all cores, not the
wake detector's process usage. Idle orb motion now rotates a cached rendering
at 10 updates per second. Shader/Canvas deformation runs for active conversation
states; idle rotation does not rerun the procedural field. The visual still
pauses when hidden, during page transitions and while the screen sleeps.

A short device comparison measured roughly 22% total CPU before and 9% after,
with local wake listening enabled. Dashboard process CPU fell from about 96% to
19% of one core. These are short CPU samples, not battery/power measurements.
The detector-only synthetic benchmark used about 0.61% of one core; live capture,
resampling and the shared audio graph add separate costs. Quiet-audio gating
was not introduced for this change.

Back from Assistant settings returns to the conversation view. The conversation
page's enable switch controls both local wake listening and permission to start
a conversation; enabling it alone does not open a cloud session.

## Continuity between conversations

Each wake or manual Start creates a new cloud session and clears the previous
on-screen transcript. Provider conversation IDs and full dialogue are not replayed.
Household notes, personalities and module policies reload each time. Saved app
state remains accessible through its tools.

Sheila can also retain **selected facts and useful context** in
`/var/lib/thinksmart-home/personal/memory.md`. This UTF-8 Markdown file is loaded
into each new Realtime session and both GPT Live roles as reference data, not
instructions. Notes dictation excludes it. No background summariser, transcript
recording, extra model call or idle polling is added. Saving a memory is model
judgement, so complete recall is not guaranteed. Use “remember …” for an explicit
request; important static household information still belongs in `household.md`.

The native `memory_action` tool supports:

- `memory.list`: optional `id` or `search`.
- `memory.add`: `text`, optional short `id` (otherwise generated).
- `memory.patch`: `id`, exact `old_text` matching once, and `new_text`.
- `memory.remove`: `id`.
- `memory.reset`: `confirm: true`, only for an explicit request to forget all
  learned memory. It never clears prompts, notes, recipes or other app data.

**Assistant settings → Reset memory** offers the same reset with a confirmation
dialog. It is disabled during a conversation so an active session cannot continue
using the erased facts. A voice-requested reset clears the file immediately;
older context already sent to the provider disappears at the next conversation.
The model is told not to recreate erased entries from that old context.

Limits are **8 KiB total, 32 entries and 1 KiB of UTF-8 text per entry**. Writes
that exceed a limit fail without truncation or eviction; Sheila can consolidate
or remove obsolete entries first. The policy favours concise preferences and
ongoing plans, with an optional dated `recent_context` handoff rather than a
transcript. It forbids learning from background/TV speech, quoted or dictated
material and uncertain speaker attribution, and storing credentials. Native
writes also reject common credential patterns; this is not a general secret
classifier. Memory contents are sent to the configured voice provider.

SSH edits reload at the next conversation/tool read without a build. Keep the
file owned by `thinksmart`, mode **600**, in its private mode-700 directory:

```markdown
# Sheila memory

## favourite_radio
Prefers BBC Radio 4.

## recent_context
2026-11-21: Planning a vegetable soup for four people.
```

Entry IDs contain up to 48 letters, digits, underscores or hyphens. Optional
`<!-- updated: ISO-8601 timestamp -->` lines are maintained by the tool. Ordinary
paragraphs and bullets are allowed within an entry; `##` headings start entries.
Malformed/oversized files are preserved and ordinary writes fail; explicit reset
can recover them. Symlinks and files readable/writable by other users are refused.
Atomic locked writes detect concurrent SSH edits and do not rewrite unchanged
content. Memory has no background service and is unavailable when the Assistant
module is disabled. Each device has independent learned memory; no tailnet sync
is implied.
