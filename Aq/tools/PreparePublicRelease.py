#!/usr/bin/env python3
# PreparePublicRelease.py, Version: 1.03
"""Prepare a sanitized AquariumControl publication archive without touching live data."""

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import zipfile

EXCLUDED_DIRS = {
    'generated', 'DevState', 'Data', 'data', 'raspi', 'doc', 'Reference',
    'captures', 'managed_components', 'node_modules', '__pycache__', 'Private',
    'PrjsPrivate', 'hw_intern', 'build', 'bin', 'Log', 'ftp', 'stock', 'stock-firmware',
}
SOURCE_SUFFIXES = {'.h', '.cpp', '.c', '.ino', '.py', '.sh', '.md', '.txt', '.json',
                   '.html', '.css', '.js', '.service', '.properties', '.defaults'}
EXCLUDED_NAMES = {'Config.json', 'config.json', 'Devices.json', 'ToDo.txt',
                  'Version.inc.h', 'ProjectVersion.cmake', 'PrjRegeln.md', 'AGENTS.md',
                  'AqPrjsProjektregeln.md', 'SwUmstellung.md'}
HARDWARE_SUFFIXES = {'.md', '.csv', '.ods', '.FCStd', '.stl', '.png', '.jpg', '.svg', '.html'}
ASSET_SUFFIXES = {'.png', '.jpg', '.svg'}
SECRET_KEYS = {'wlanpw', 'wlanpw2', 'appw', 'ftppw', 'password', 'privatekey', 'token', 'apikey'}


def inspect_text(path, text):
    findings = []
    if re.search(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----', text):
        findings.append({'path': path, 'kind': 'private-key', 'blocking': True})
    if re.search(r'https?://[^\s/\"\']+:[^\s/\"\']+@', text):
        findings.append({'path': path, 'kind': 'URL-credentials-review', 'blocking': True})
    if path.endswith('.json'):
        try:
            document = json.loads(text)
        except ValueError:
            findings.append({'path': path, 'kind': 'invalid-json', 'blocking': True})
        else:
            def visit(value):
                if isinstance(value, dict):
                    for key, item in value.items():
                        if key.lower() in SECRET_KEYS and item not in ('', None):
                            findings.append({'path': path, 'kind': 'nonempty-secret-field', 'blocking': True})
                        visit(item)
                elif isinstance(value, list):
                    for item in value:
                        visit(item)
            visit(document)
    if re.search(r'\b(?:192\.168\.\d+\.\d+|[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})\b', text):
        findings.append({'path': path, 'kind': 'network-example-or-local-metadata-review', 'blocking': False})
    if re.search(r'/mnt/|/home/(?!<)|gneiss@', text):
        findings.append({'path': path, 'kind': 'local-path-or-author-contact-review', 'blocking': False})
    return findings


def prepare(root, destination):
    if destination.exists():
        raise RuntimeError('Destination already exists; choose a fresh directory.')
    destination.mkdir(parents=True)
    included, excluded, findings, sanitized = [], [], [], []
    payloads = {}
    for base, dirs, files in os.walk(root / 'Aq', followlinks=False):
        for name in list(dirs):
            directory = Path(base) / name
            if name in EXCLUDED_DIRS or name.startswith('.') or directory.is_symlink() or name.startswith('build'):
                dirs.remove(name)
                excluded.append({'path': directory.relative_to(root).as_posix() + '/', 'reason': 'private-runtime-generated-or-unreviewed-assets'})
        for name in sorted(files):
            source = Path(base) / name
            relative = source.relative_to(root).as_posix()
            if (source.is_symlink() or name in EXCLUDED_NAMES or name.startswith('.')
                    or 'Secrets' in name or source.suffix.lower() in {'.key', '.pem'}
                    or not (source.suffix in SOURCE_SUFFIXES or name in {'Makefile', 'LICENSE', 'LICENSE.md', 'LICENSE.txt', 'sdkconfig'})
                    and not (name == 'partitions.csv')
                    and not ('Hw' in source.relative_to(root).parts and source.suffix in HARDWARE_SUFFIXES)
                    and not ('WebSrc' in source.relative_to(root).parts and source.suffix in ASSET_SUFFIXES)):
                excluded.append({'path': relative, 'reason': 'outside-source-allowlist'})
                continue
            body = source.read_bytes()
            if source.suffix in {'.FCStd', '.ods'}:
                with zipfile.ZipFile(source) as nested:
                    if nested.testzip() is not None:
                        raise RuntimeError('Invalid hardware archive: ' + relative)
                    for member in nested.namelist():
                        if member.endswith('.xml'):
                            findings.extend(inspect_text(relative + ':' + member,
                                                         nested.read(member).decode('utf-8')))
            if source.suffix in {'.FCStd', '.ods', '.png', '.jpg', '.stl'}:
                payloads[relative] = body
                included.append(relative)
                continue
            text = body.decode('utf-8')
            if source.name.endswith('Config.inc.h') or source.name == 'ConfigDefaults.inc.h':
                text, changes = re.subn(r'(\b(?:WlanPw2?|ApPw|FtpPw)\s*,\s*)"([^"\n]+)"',
                                        r'\1"CHANGE_ME"', text)
                if changes:
                    sanitized.append({'path': relative, 'kind': 'source-password-defaults-replaced', 'count': changes})
                    body = text.encode('utf-8')
            # Manufacturer manuals remain local; keep public instructions readable.
            text = re.sub(r'\[([^\]]+)\]\([^)]*(?:WIFI_Kit_8_Pinout_Diagram|8-Relais%20Modul_DE)\.pdf\)',
                          r'\1 (manufacturer reference; not bundled)', text)
            if relative == 'Aq/README.md':
                text = ('# AquariumControl projects\n\nWork in progress; feedback welcome.\n\n'
                        '- [Feeder](Feeder/README.md)\n- [FeederSimple](FeederSimple/README.md)\n'
                        '- [MultiTimer](MultiTimer/README.md)\n- [Dashboard](Dashboard/README.md)\n'
                        '- [NOUS A8T](TmrSwA8T/README.md)\n- [Shelly](TmrSwShelly/README.md)\n'
                        '- [Room temperature](RoomTemperature/README.md)\n'
                        '- [Heat history](HeatHistory/README.md)\n')
            body = text.encode('utf-8')
            findings.extend(inspect_text(relative, text))
            payloads[relative] = body
            included.append(relative)
    root_files = [('README.public.md', 'README.md'), ('README.public.md', 'README.public.md'),
                  ('BUILD-SECRETS.md', 'BUILD-SECRETS.md')]
    for name in ('LICENSE.md', 'NOTICE.md', 'THIRD_PARTY.md'):
        if (root / name).is_file():
            root_files.append((name, name))
    for source_name, target_name in root_files:
        body = (root / source_name).read_bytes()
        payloads[target_name] = body
        included.append(target_name)
        findings.extend(inspect_text(target_name, body.decode('utf-8')))
    payloads['.gitignore'] = b'''**/DevState/\n**/generated/\n**/Data/\n**/data/\n**/raspi/\n**/doc/\n**/hw_intern/\n**/Private/\n**/PrjsPrivate/\n**/__pycache__/\n**/managed_components/\n**/node_modules/\n**/Config.json\n**/config.json\n**/Devices.json\n**/Version.inc.h\n**/ToDo.txt\n*.FCBak\n*.FCStd[0-9]*\n*.key\n*.pem\n*.bin\n*.elf\n*.map\n*.zip\n*.log\n*.pyc\n'''
    included.append('.gitignore')
    report = {'CreatedUtc': dt.datetime.now(dt.timezone.utc).isoformat(),
              'Status': 'PREPARED_FOR_USER_AUTHORIZED_PUBLICATION',
              'Scope': 'Aq sources and hardware; takeover separate; private runtime data and manufacturer PDF manuals excluded',
              'Included': sorted(included), 'Excluded': sorted(excluded, key=lambda item: item['path']),
              'Findings': findings, 'SanitizedExportCopies': sanitized,
              'Open': ['Fresh ESP checkout builds with own configuration',
                       'Generic device/config examples; incomplete hardware sections marked Coming soon']}
    (destination / 'Review.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    (destination / 'Files.txt').write_text('\n'.join(sorted(included)) + '\n')
    if any(item['blocking'] for item in findings):
        raise RuntimeError('Blocking findings; see Review.json (paths/categories only). No archive created.')
    archive = destination / 'AquariumSources.review.zip'
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED) as output:
        for name in sorted(payloads):
            entry = zipfile.ZipInfo(name)
            source = root / name
            mode = source.stat().st_mode if source.is_file() else 0o100644
            entry.external_attr = (mode & 0xFFFF) << 16
            output.writestr(entry, payloads[name], compress_type=zipfile.ZIP_DEFLATED)
    with zipfile.ZipFile(archive) as output:
        if output.testzip() is not None or sorted(output.namelist()) != sorted(included):
            raise RuntimeError('Archive verification failed.')
    print(f'Review archive: {archive}')
    print(f'{len(included)} files; {len(findings)} review findings. No Git initialization or upload.')


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='new review directory (never overwritten)')
    args = parser.parse_args()
    try:
        prepare(root, args.output.expanduser().resolve())
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'ERROR: {error}\n')


if __name__ == '__main__':
    main()
