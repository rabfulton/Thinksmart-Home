# Sheila wake word


When Assistant is enabled, the appliance listens locally for “Sheila” during
radio/Bluetooth playback and conversations. The audio test temporarily takes over
the microphone. It also listens with the screen asleep.
Turning Assistant off closes both the local listener and any cloud conversation.
The listener sends no microphone audio over the network and saves no recordings.

Say Sheila and make your request. A short memory-only buffer bridges detection
and connection setup (up to ten seconds). Media quietens during speech and replies, then fades back when the reply ends,
even while follow-up listening remains open. Saying Sheila during a reply
interrupts it. No standby microphone audio is uploaded or saved.

wakeword.json contains threshold (0..1), score (0..5) and trailingBlanks (1..5).
The word is fixed to Sheila in this version. Higher threshold or lower score is
more conservative. Settings → Reload configuration restarts the local listener
and applies edits. Edits are preserved by future deployments. These settings
control local detection, independently of the selected personality and API voice.

Default sensitivity:

```json
{"word":"Sheila","threshold":0.35,"score":1.5,"trailingBlanks":2}
```

Edit /etc/thinksmart/assistant/wakeword.json. Keep word set to Sheila.
If Assistant is in Hold to talk mode, wake opens the conversation but the Talk
button is still needed; choose Natural pauses for hands-free turns.

One local Chatterbox positive sample and one unrelated negative sample passed
on the development host; the positive sample also passed offline on ARM64.
These do not measure your accent, distance or real-room false activations.

The capture driver has an 80 ms buffer. The listener drains it in a separate
thread while inference runs, using a bounded 500 ms memory queue. The model
warms up for several seconds when the listener starts; Settings reports when it
is listening. Turning Assistant off terminates either phase.
