#!/usr/bin/env python3
"""Validate Realtime credentials/session settings without capture or generation."""
import json
import re
import sys
from pathlib import Path
from urllib.parse import urlencode
import websocket

config = json.loads(Path(sys.argv[1] if len(sys.argv) > 1 else 'private/voice.json').read_text())
if config.get('provider', 'openai') != 'openai':
    raise SystemExit('Only the OpenAI protocol is implemented')
ws = None
try:
    ws = websocket.create_connection(
        'wss://api.openai.com/v1/realtime?' + urlencode({'model': config.get('model', 'gpt-realtime-2.1-mini')}),
        header={'Authorization': 'Bearer ' + config['apiKey']}, timeout=12)
    started = False
    for _ in range(12):
        event = json.loads(ws.recv())
        if event['type'] == 'session.created':
            ws.send(json.dumps({'type': 'session.update', 'session': {
                'type': 'realtime', 'output_modalities': ['audio'],
                'instructions': 'Be brief. Use local tools for device data.',
                'audio': {'input': {'format': {'type': 'audio/pcm', 'rate': 24000},
                                    'turn_detection': {'type': 'semantic_vad', 'eagerness': 'auto', 'create_response': True, 'interrupt_response': True}},
                          'output': {'format': {'type': 'audio/pcm', 'rate': 24000}, 'voice': config.get('voice', 'marin')}},
                'tools': [], 'tool_choice': 'auto', 'max_output_tokens': 1024}}))
        elif event['type'] == 'session.updated':
            print('OpenAI authenticated and accepted the Realtime audio/session settings. No audio sent or response requested.')
            started = True
            break
        elif event['type'] == 'error':
            error = event.get('error', {})
            # No provider message, payload, request header or credentials in output.
            safe = lambda value: re.sub(r'[^A-Za-z0-9_.\[\]-]', '', str(value))[:100]
            print('OpenAI configuration rejected:', safe(error.get('code')), safe(error.get('param')))
            break
    if not started:
        raise SystemExit(1)
except Exception as exc:
    print('Connection check failed:', type(exc).__name__, 'HTTP', getattr(exc, 'status_code', 'n/a'))
    raise SystemExit(1) from None
finally:
    if ws is not None:
        ws.close(timeout=2)
