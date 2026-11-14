#!/usr/bin/env python3
"""Pinned official Sherpa-ONNX CPU libraries, C API header and phonetic KWS model."""
import hashlib
import json
import tarfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / 'build/wakeword'
VERSION = '2.0.1'
ASSETS = [
    ('aarch64', f'sherpa-onnx-v{VERSION}-linux-aarch64-shared-cpu-lib.tar.bz2', '15bd737eae467da32ec97e3740a329a535fd21e33caca4f327b0081451d11533', f'v{VERSION}'),
    ('x64', f'sherpa-onnx-v{VERSION}-linux-x64-shared-lib.tar.bz2', '532c45bbd41036d83cd70606797529aa8086c2053395a58967736c727625b120', f'v{VERSION}'),
    ('phonetic-model', 'sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20.tar.bz2', '68447f4fbc67e70eee3a93961f36e81e98f47aef73ce7e7ca00885c6cd3616a6', 'kws-models'),
]

def fetch(url, path, expected):
    if not path.exists():
        tmp = path.with_suffix(path.suffix + '.download')
        with urllib.request.urlopen(url, timeout=60) as source, tmp.open('wb') as target:
            while block := source.read(1024 * 1024):
                target.write(block)
        tmp.replace(path)
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != expected:
        raise SystemExit('Checksum mismatch: ' + path.name)

ROOT.mkdir(parents=True, exist_ok=True)
for architecture, filename, digest, tag in ASSETS:
    archive = ROOT / filename
    fetch(f'https://github.com/k2-fsa/sherpa-onnx/releases/download/{tag}/{filename}', archive, digest)
    destination = ROOT / architecture
    marker = destination / '.verified'
    if not marker.exists() or marker.read_text() != digest:
        destination.mkdir(exist_ok=True)
        with tarfile.open(archive) as source:
            source.extractall(destination, filter='data')
        marker.write_text(digest)

header = ROOT / 'include/sherpa-onnx/c-api/c-api.h'
header.parent.mkdir(parents=True, exist_ok=True)
fetch(f'https://raw.githubusercontent.com/k2-fsa/sherpa-onnx/v{VERSION}/sherpa-onnx/c-api/c-api.h', header,
      '5698f38ba09ad2bdf085c56a6cbbd7741acf15e25f4ce5c7097b9702cadf437f')
licenses = ROOT / 'licenses'
licenses.mkdir(exist_ok=True)
fetch(f'https://raw.githubusercontent.com/k2-fsa/sherpa-onnx/v{VERSION}/LICENSE', licenses / 'sherpa-onnx-LICENSE',
      'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30')
fetch('https://raw.githubusercontent.com/microsoft/onnxruntime/v1.23.2/LICENSE', licenses / 'onnxruntime-LICENSE',
      '2f07c72751aed99790b8a4869cf2311df85a860b22ded05fa22803587a48922c')
(ROOT / 'sources.json').write_text(json.dumps({'runtime': VERSION, 'assets': [{'name': a[1], 'sha256': a[2]} for a in ASSETS], 'source': 'https://github.com/k2-fsa/sherpa-onnx'}, indent=2) + '\n')
print('Verified pinned wake-word libraries, model and C API header.')
