#!/usr/bin/env python3
"""Validate non-secret module/location configuration and prepare a local copy."""
import argparse
import json
import math
import os
from pathlib import Path
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

MODULES = ('assistant', 'radio', 'weather', 'news', 'chess', 'notes', 'bluetooth', 'vrm', 'navidrome', 'jellyfin', 'iptv', 'markets')


def validate(config):
    if not isinstance(config, dict) or config.get('version', 1) != 1:
        raise ValueError('Expected a version-1 device configuration')
    if set(config) - {'version', 'modules', 'weather'}:
        raise ValueError('Unknown configuration field; credentials belong in separate private files')
    modules = config.get('modules', {})
    if not isinstance(modules, dict) or set(modules) - set(MODULES) or any(type(value) is not bool for value in modules.values()):
        raise ValueError('Module flags must be known names with true/false values')
    weather = config.get('weather', {})
    if not isinstance(weather, dict) or set(weather) - {'name', 'latitude', 'longitude', 'timezone'}:
        raise ValueError('Invalid weather settings')
    for key, limit in (('latitude', 90), ('longitude', 180)):
        if key in weather and (type(weather[key]) not in (int, float) or not math.isfinite(weather[key]) or abs(weather[key]) > limit):
            raise ValueError('Invalid weather coordinates')
    if 'name' in weather and (not isinstance(weather['name'], str) or not weather['name'].strip() or len(weather['name']) > 80):
        raise ValueError('Weather name must contain 1–80 characters')
    if 'timezone' in weather:
        try:
            ZoneInfo(weather['timezone'])
        except (TypeError, ValueError, ZoneInfoNotFoundError) as error:
            raise ValueError('Use a valid IANA timezone') from error
    return config


def main():
    project = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=project / 'config/device.example.json')
    parser.add_argument('--output', type=Path, default=project / 'private/device.json')
    parser.add_argument('--enable', action='append', choices=MODULES, default=[])
    parser.add_argument('--disable', action='append', choices=MODULES, default=[])
    parser.add_argument('--check', action='store_true', help='Validate without writing or connecting to a device')
    parser.add_argument('--force', action='store_true', help='Replace an existing output configuration')
    args = parser.parse_args()
    config = validate(json.loads(args.config.read_text()))
    modules = config.setdefault('modules', {})
    for name in args.enable:
        modules[name] = True
    for name in args.disable:
        modules[name] = False
    validate(config)
    if not args.check:
        args.output.parent.mkdir(parents=True, mode=0o700, exist_ok=True)
        flags = os.O_WRONLY | os.O_CREAT | (os.O_TRUNC if args.force else os.O_EXCL)
        fd = os.open(args.output, flags, 0o600)
        with os.fdopen(fd, 'w') as output:
            json.dump(config, output, indent=2)
            output.write('\n')
        os.chmod(args.output, 0o600)
    print('Validated device configuration. Enabled modules: ' + ', '.join(name for name in MODULES if modules.get(name, name not in {"iptv", "markets"})))


if __name__ == '__main__':
    main()
