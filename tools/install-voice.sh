#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
test -x build/audio-arm64/thinksmart-voice-audio
# Only the small WebSocket runtime is added to the existing Qt/ALSA stack.
./tools/thinksmart-ssh 'apt-get install -y --no-install-recommends libqt6websockets6 libspeexdsp1 libwebrtc-audio-processing-1-3'
tar -C build/audio-arm64 -cf - thinksmart-voice-audio | ./tools/thinksmart-ssh 'install -d -m 755 /opt/thinksmart/staging; tar -xf - -C /opt/thinksmart/staging; install -m 755 /opt/thinksmart/staging/thinksmart-voice-audio /usr/local/libexec/thinksmart-voice-audio'
# The key is transferred directly over SSH, never staged with public artifacts.
if [ -f private/voice.json ]; then
    python3 - <<'PY'
from pathlib import Path
import json
p = Path('private/voice.json')
c = json.loads(p.read_text())
if p.stat().st_mode & 0o077:
    raise SystemExit('private/voice.json must have mode 600')
if c.get('provider', 'openai') != 'openai' or not c.get('apiKey'):
    raise SystemExit('A configured OpenAI key is required')
PY
    cat private/voice.json | ./tools/thinksmart-ssh 'umask 077; install -d -o thinksmart -g thinksmart -m 700 /var/lib/thinksmart-home/.config/ThinkSmart/Home; cat > /var/lib/thinksmart-home/.config/ThinkSmart/Home/voice.json.new; chown thinksmart:thinksmart /var/lib/thinksmart-home/.config/ThinkSmart/Home/voice.json.new; chmod 600 /var/lib/thinksmart-home/.config/ThinkSmart/Home/voice.json.new; mv /var/lib/thinksmart-home/.config/ThinkSmart/Home/voice.json.new /var/lib/thinksmart-home/.config/ThinkSmart/Home/voice.json'
fi
./tools/install-home.sh
