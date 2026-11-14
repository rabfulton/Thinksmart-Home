#!/usr/bin/env python3
"""Prepare supplied WAV or headerless Live PCM clips; never change the originals."""
import argparse
import array
import json
from pathlib import Path
import re
import sys
import wave


def roughness(data):
    samples = array.array('h', data)
    if sys.byteorder != 'little':
        samples.byteswap()
    return sum(abs(a - b) for a, b in zip(samples, samples[1:])) / max(1, sum(abs(a) for a in samples))


def prepare(source, destination):
    result = []
    destination.mkdir(parents=True, exist_ok=True)
    for path in sorted(source.glob('*.wav')):
        if not re.fullmatch(r'[a-z0-9_-]+', path.stem):
            raise ValueError(f'Invalid preview filename: {path.name}')
        data = path.read_bytes()
        discarded = ''
        if data.startswith(b'RIFF'):
            with wave.open(str(path), 'rb') as clip:
                if clip.getcomptype() != 'NONE':
                    raise ValueError(f'Expected PCM WAV: {path.name}')
                channels, width, rate = clip.getnchannels(), clip.getsampwidth(), clip.getframerate()
                pcm = clip.readframes(clip.getnframes())
            kind = 'wav'
        else:
            # Live's output is mono little-endian PCM16 at 24 kHz. Some supplied
            # downloads start with an incomplete sample; use waveform continuity
            # to distinguish a missing leading byte from a missing trailing byte.
            channels, width, rate = 1, 2, 24000
            pcm = data
            if len(pcm) % 2:
                even, shifted = roughness(pcm[:-1]), roughness(pcm[1:])
                if shifted < even / 2:
                    pcm = pcm[1:]
                    discarded = 'leading partial sample'
                elif even < shifted / 2:
                    pcm = pcm[:-1]
                    discarded = 'trailing partial sample'
                else:
                    raise ValueError(f'Ambiguous raw PCM alignment: {path.name}')
            kind = 'raw-pcm16-24khz'
        seconds = len(pcm) / (channels * width * rate)
        if not 0 < seconds <= 12:
            raise ValueError(f'Expected a short preview (up to 12 seconds): {path.name}')
        with wave.open(str(destination / path.name), 'wb') as output:
            output.setparams((channels, width, rate, 0, 'NONE', 'not compressed'))
            output.writeframes(pcm)
        result.append(dict(voice=path.stem, sourceFormat=kind, seconds=round(seconds, 3), repaired=discarded))
    if not result:
        raise ValueError('No voice previews found')
    return result


if __name__ == '__main__':
    project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=project / 'artifacts/voice-previews')
    parser.add_argument('--destination', type=Path, default=project / 'app/deploy/voice-previews')
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.destination), indent=2))
