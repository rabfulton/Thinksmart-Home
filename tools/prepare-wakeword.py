#!/usr/bin/env python3
"""Prepare a fixed phonetic Sheila keyword from the official model lexicon."""
from pathlib import Path
import shutil

root = Path('build/wakeword')
model = root / 'phonetic-model/sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20'
stage = root / 'stage'
(stage / 'model').mkdir(parents=True, exist_ok=True)
for output in ('encoder', 'decoder', 'joiner'):
    suffix = 'onnx' if output == 'decoder' else 'int8.onnx'
    shutil.copyfile(model / f'{output}-epoch-13-avg-2-chunk-8-left-64.{suffix}', stage / 'model' / (output + '.onnx'))
shutil.copyfile(model / 'tokens.txt', stage / 'model/tokens.txt')
phones = next(line.split()[1:] for line in (model / 'en.phone').read_text().splitlines() if line.startswith('SHEILA '))
vocabulary = {line.rsplit(' ', 1)[0] for line in (model / 'tokens.txt').read_text().splitlines()}
if not phones or any(phone not in vocabulary for phone in phones):
    raise SystemExit('Sheila pronunciation does not match model vocabulary')
(stage / 'sheila.txt').write_text(' '.join(phones) + ' @Sheila\n')
shutil.copyfile(root / 'sources.json', stage / 'sources.json')
print('Prepared custom Sheila keyword:', ' '.join(phones))
