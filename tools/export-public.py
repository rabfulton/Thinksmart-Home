#!/usr/bin/env python3
"""Export an allowlisted, history-free public source snapshot. Never publishes."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
TOOLS = (
    'Dockerfile', 'Dockerfile.qt', 'Dockerfile.audio', 'Dockerfile.shared-audio',
    'build-home.sh', 'build-audio.sh', 'build-wakeword.sh', 'build-shared-audio.sh',
    'build-chess-engine.sh', 'install-home.sh', 'install-voice.sh',
    'install-wakeword.sh', 'install-shared-audio.sh', 'install-chess-engine.sh',
    'install-media-config.sh', 'install-vrm-config.sh', 'install-audio-lab.sh',
    'fetch-wakeword.py', 'prepare-wakeword.py', 'prepare-voice-previews.py',
    'prepare_sheila_helpers.py', 'check-wakeword.py', 'check-voice-api.py',
    'check-live-api.py', 'configure-device.py', 'thinksmart-ssh', 'thinksmartctl',
    'export-public.py', 'check-public.py', 'test-public.py', 'render-screenshots.sh',
    'thinksmart-warp-ready.py', 'test-appliance.py', 'install-appliance.py',
)
APP_SUFFIXES = {'.cpp', '.h', '.hpp', '.cc', '.c', '.cmake', '.frag', '.qml', '.svg', '.json', '.service', '.socket', '.conf', '.xml'}
APP_NAMES = {'CMakeLists.txt', 'qmldir', 'LICENSE', 'SOURCE', 'thinksmart-session', 'thinksmart-route-audio', 'rollback-shared-audio'}


def files():
    for name in ('README.md', 'AGENTS.md', 'LICENSE', 'THIRD_PARTY.md'):
        yield ROOT / name
    yield from (ROOT / 'docs').glob('*.md')
    yield from (ROOT / 'docs/screenshots').glob('*.png')
    yield from (ROOT / 'config').glob('*.example.*')
    yield from (ROOT / 'config').glob('*.env.example')
    for name in TOOLS:
        yield ROOT / 'tools' / name
    for path in (ROOT / 'app').rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(ROOT).as_posix()
        if relative.startswith(('app/audio/training/', 'app/audio/fixtures/', 'app/deploy/voice-previews/')):
            continue
        if path.suffix in APP_SUFFIXES or path.name in APP_NAMES or relative.startswith('app/deploy/assistant/') and path.suffix == '.md':
            yield path


def export(destination, force=False):
    destination = destination.resolve()
    if destination == ROOT or not destination.is_relative_to(ROOT / 'build/public'):
        raise ValueError('Export destination must be inside build/public/')
    if destination.exists() and any(destination.iterdir()):
        if not force or not (destination / 'PUBLIC_MANIFEST.json').is_file() or (destination / '.git').exists():
            raise ValueError('Refusing to replace a nonempty directory or a Git repository')
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for source in sorted(set(files())):
        if source.is_symlink() or not source.is_file():
            raise ValueError('Missing file or symlink in export allowlist: ' + source.name)
        target = destination / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    # The public default is independent of anybody's edited household notes.
    (destination / 'app/deploy/assistant/household.md').write_text(
        '# Household notes\n\nAdd your own household facts and preferences on the device.\n'
        'No personal facts are supplied by this project.\n')
    (destination / '.gitignore').write_text(
        '/private/\n/artifacts/\n/build/\n/upstream/\n/.venv*/\n__pycache__/\n'
        '*.pyc\n*.img\n*.img.gz\n*.onnx\n*.tflite\n*.wav\n*.pem\n*.key\n'
        '/admin.md\n/SETUP.md\n/spec.md\n')
    manifest = {path.relative_to(destination).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in sorted(destination.rglob('*')) if path.is_file()}
    (destination / 'PUBLIC_MANIFEST.json').write_text(json.dumps({'version': 1, 'files': manifest}, indent=2) + '\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/public/thinksmart-home')
    parser.add_argument('--force', action='store_true', help='Replace only an earlier uncommitted public export')
    args = parser.parse_args()
    manifest = export(args.output, args.force)
    print(f'Exported {len(manifest)} allowlisted files to {args.output}; no Git history or private directories copied.')
