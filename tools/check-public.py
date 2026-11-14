#!/usr/bin/env python3
"""Check a public source snapshot for private files, credential patterns and broken links."""
import argparse
import json
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

FORBIDDEN_DIRS = {'private', 'artifacts', 'build', 'upstream', '.aws', '.codex', '.agents', '__pycache__'}
FORBIDDEN_SUFFIXES = {'.wav', '.onnx', '.tflite', '.img', '.pem', '.key', '.p12', '.pcap', '.pyc'}
KEY = re.compile(r'(?m)^-----BEGIN (?:OPENSSH |RSA |EC |DSA )?PRIVATE KEY-----\s*$')
TOKEN = re.compile(r'\b(?:sk-(?:proj-)?[A-Za-z0-9_-]{24,}|xai-[A-Za-z0-9_-]{24,}|gh[pousr]_[A-Za-z0-9]{25,}|tskey-[A-Za-z0-9_-]{20,})\b')
HOME = re.compile(r'/home/([A-Za-z0-9_.-]+)/')
TAILNET = re.compile(r'\b[\w-]+\.tail[0-9a-f]+\.ts\.net\b')
LINK = re.compile(r'!?\[[^\]]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)')


def inspect(root):
    root = root.resolve()
    issues = []
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root)
        if '.git' in relative.parts:
            continue
        if path.is_symlink():
            issues.append(f'{relative}: symlinks are not allowed in the public snapshot')
            continue
        if not path.is_file():
            continue
        if any(part in FORBIDDEN_DIRS or part.startswith('.venv') for part in relative.parts):
            issues.append(f'{relative}: private/generated directory')
        if path.suffix in FORBIDDEN_SUFFIXES or path.name in {'admin.md', 'SETUP.md', 'spec.md'} or path.name.endswith('.img.gz'):
            issues.append(f'{relative}: excluded personal/generated file type')
        content = path.read_bytes()
        if path.suffix == '.png':
            if not relative.as_posix().startswith('docs/screenshots/') or not content.startswith(b'\x89PNG\r\n\x1a\n'):
                issues.append(f'{relative}: unreviewed binary image')
            continue
        try:
            text = content.decode('utf-8')
        except UnicodeError:
            issues.append(f'{relative}: unexpected binary file')
            continue
        if KEY.search(text) or TOKEN.search(text):
            issues.append(f'{relative}: possible private key or API token (value withheld)')
        if any(m.group(1) not in {'user', 'username', 'runner', 'your-user'} for m in HOME.finditer(text)) or TAILNET.search(text):
            issues.append(f'{relative}: personal home path or tailnet hostname')
        if path.suffix == '.json':
            try:
                value = json.loads(text)
            except ValueError:
                issues.append(f'{relative}: invalid JSON')
                continue
            def credentials(value):
                if isinstance(value, dict):
                    for key, item in value.items():
                        if key in {'apiKey', 'accessToken', 'password', 'token'} and isinstance(item, str) and item:
                            issues.append(f'{relative}: populated credential field (value withheld)')
                        credentials(item)
                elif isinstance(value, list):
                    for item in value:
                        credentials(item)
            credentials(value)
        if path.suffix == '.md':
            for target in LINK.findall(text):
                parsed = urlsplit(target.strip('<>'))
                if parsed.scheme or parsed.netloc or not parsed.path:
                    continue
                linked = (path.parent / unquote(parsed.path)).resolve()
                if not linked.is_relative_to(root) or not linked.exists():
                    issues.append(f'{relative}: broken or external local link: {parsed.path}')
    for name in ('README.md', 'AGENTS.md', 'LICENSE', 'THIRD_PARTY.md', 'docs/INSTALL.md', 'docs/CONFIGURATION.md', 'docs/screenshots/home-landscape.png'):
        if not (root / name).is_file():
            issues.append('Missing required public file: ' + name)
    return issues


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    problems = inspect(args.directory)
    for problem in problems:
        print(problem)
    if problems:
        raise SystemExit(1)
    print('Public snapshot checks passed: file types, credential patterns, personal paths and local documentation links.')
