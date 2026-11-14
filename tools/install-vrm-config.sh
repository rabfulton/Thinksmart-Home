#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
python3 - <<'PY'
import json,os
p='private/vrm.json'
if os.stat(p).st_mode & 0o077:raise SystemExit('VRM file must have mode 600')
c=json.load(open(p))
if not isinstance(c.get('token'),str) or not c['token'].strip():raise SystemExit('VRM token is empty')
if int(c.get('installationId',0) or 0)<0:raise SystemExit('Invalid installation ID')
print('VRM configuration validated; credentials will not be printed.')
PY
cat private/vrm.json | ./tools/thinksmart-ssh 'set -e; umask 077; install -d -m 700 -o thinksmart -g thinksmart /var/lib/thinksmart-home/.config/ThinkSmart/Home; cat > /var/lib/thinksmart-home/.config/ThinkSmart/Home/vrm.json; chmod 600 /var/lib/thinksmart-home/.config/ThinkSmart/Home/vrm.json; chown thinksmart:thinksmart /var/lib/thinksmart-home/.config/ThinkSmart/Home/vrm.json'
