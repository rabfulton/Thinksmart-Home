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
Assistant Settings → Enable assistant permits local listening; Start or Sheila
opens a conversation. Realtime has VAD options; Live handles continuous listening
and interruptions through its own protocol. Live's backend handles reasoning and
tool selection using the allowed local actions. Sessions close on stop, screen
sleep, quiet timeout or the maximum duration.

## Ending a conversation

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
The ordinary quiet timeout and wake sensitivity are unchanged. Dismissal metadata
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
