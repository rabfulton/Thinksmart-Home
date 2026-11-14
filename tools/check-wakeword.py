#!/usr/bin/env python3
"""Offline smoke check using local synthetic fixtures; no devices or cloud calls."""
import json
import subprocess

base = ['build/wake-native/thinksmart-wakeword', '--model-dir', 'build/wakeword/stage/model', '--keywords-file', 'build/wakeword/stage/sheila.txt']
for fixture, expected in [('sheila', 'wake'), ('unrelated', 'no-wake')]:
    result = subprocess.run(base + ['--file', f'build/wakeword/{fixture}.s16'], capture_output=True, text=True, check=True, timeout=30)
    events = [json.loads(line) for line in result.stdout.splitlines()]
    if not events or events[-1].get('event') != expected:
        raise SystemExit(f'Offline {fixture} fixture failed: {events}')
    print(f'PASS: offline {fixture} fixture → {expected}')
