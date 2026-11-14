#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
python3 - <<'PY'
import json,os
path='private/media.json'
if os.stat(path).st_mode & 0o077:raise SystemExit('Media configuration must have mode 600')
config=json.load(open(path))
if not isinstance(config,dict):raise SystemExit('Invalid media configuration')
print('Media configuration parsed; credentials will not be printed.')
PY
cat private/media.json | ./tools/thinksmart-ssh 'set -e; umask 077; install -d -m 700 -o thinksmart -g thinksmart /var/lib/thinksmart-home/.config/ThinkSmart/Home; cat > /var/lib/thinksmart-home/.config/ThinkSmart/Home/media.json; chmod 600 /var/lib/thinksmart-home/.config/ThinkSmart/Home/media.json; chown thinksmart:thinksmart /var/lib/thinksmart-home/.config/ThinkSmart/Home/media.json'
