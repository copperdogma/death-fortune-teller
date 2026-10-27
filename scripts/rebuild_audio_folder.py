#!/usr/bin/env python3
"""Stage verified audio at fresh card locations, retaining the old folder.

Default is read-only. --write requires explicit user authorization and a card
in the computer's reader. Never formats, deletes, or overwrites existing files.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys

from check_audio_reads import read_once


def hashes(root):
    result = {}
    for path in sorted(root.rglob('*')):
        if path.is_symlink():
            raise ValueError('Symlinks are unsupported')
        if path.is_file():
            result[path.relative_to(root).as_posix()] = hashlib.sha256(read_once(path)).hexdigest()
        elif not path.is_dir():
            raise ValueError('Unexpected non-file entry')
    return result


def file_state(root):
    return {p.relative_to(root).as_posix(): (p.stat().st_size, p.stat().st_mtime_ns)
            for p in root.rglob('*') if p.is_file()}


def content_hashes(root):
    return {name: value for name, value in hashes(root).items()
            if not any(part.startswith('.') for part in Path(name).parts)}


def rebuild(volume, source, backup, write=False, allow_different_original=False,
            archive_name='audio-original-20261025', staging_name='audio-rebuild-20261025'):
    volume, source, backup = (p.resolve() for p in (volume, source, backup))
    if source.is_relative_to(volume) or backup.is_relative_to(volume):
        raise ValueError('Verified source and backup must be outside the card')
    active = volume / 'audio'
    for name in (archive_name, staging_name):
        if not name.startswith('audio-') or Path(name).name != name or '/' in name or '\\' in name:
            raise ValueError('Archive/staging must be distinct audio-prefixed directory names')
    if archive_name == staging_name:
        raise ValueError('Archive and staging names must differ')
    archive = volume / archive_name
    staging = volume / staging_name
    for path in (active, source, backup):
        if path.is_symlink() or not path.is_dir():
            raise ValueError('Expected real directory')
    if archive.exists() or staging.exists() or archive.is_symlink() or staging.is_symlink():
        raise FileExistsError('Archive or staging already exists; refusing overwrite')
    expected = content_hashes(source)
    if expected != content_hashes(backup):
        raise ValueError('Source differs from the independently verified reference tree')
    # Finder metadata is preserved in the archive, not copied as active content.
    # macOS can synthesize AppleDouble companions during FAT file creation.
    if len(expected) != 9 or any(not name.endswith('.wav') for name in expected):
        raise ValueError('Expected exactly the nine qualified audio assets')
    original = hashes(active)
    original_state = file_state(active)
    if not set(expected).issubset(original):
        raise ValueError('Expected audio files are missing from the card')
    if not allow_different_original and any(original.get(name) != value for name, value in expected.items()):
        raise ValueError('Current card differs from verified backup; stop for review')
    if any(original_state[name][0] != (source / name).stat().st_size for name in expected):
        raise ValueError('Original file lengths differ; stop for review')
    required = sum((source / name).stat().st_size for name in expected)
    if shutil.disk_usage(volume).free < required + 1024 * 1024:
        raise ValueError('Insufficient card space for preserved-original rebuild')
    report = {'write': write, 'files': len(expected), 'bytes': required,
              'original_files': len(original), 'archive': str(archive),
              'active': str(active), 'differing_original_allowed': allow_different_original}
    if not write:
        return report
    staging.mkdir()
    # On failure leave this uniquely named partial staging folder for inspection;
    # the active folder is unchanged until all copies have passed verification.
    for name, expected_hash in expected.items():
        print(f'Copying {name}', file=sys.stderr, flush=True)
        data = read_once(source / name)
        if hashlib.sha256(data).hexdigest() != expected_hash:
            raise ValueError('Source changed after planning')
        target = staging / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
    for attempt in range(2):
        print(f'Uncached verification pass {attempt + 1}', file=sys.stderr, flush=True)
        if content_hashes(staging) != expected:
            raise ValueError('Fresh copies failed uncached read-back verification')
    if file_state(active) != original_state:
        raise ValueError('Original folder metadata changed during staging; switch refused')
    if not allow_different_original and hashes(active) != original:
        raise ValueError('Original folder changed during staging; switch refused')
    active.rename(archive)
    try:
        staging.rename(active)
    except BaseException:
        archive.rename(active)
        raise
    archived_hashes = hashes(archive)
    if file_state(archive) != original_state or content_hashes(active) != expected:
        raise ValueError('Post-switch verification failed; both folders retained')
    if not allow_different_original and archived_hashes != original:
        raise ValueError('Archived originals read differently; both folders retained')
    report.update(verified=True, original_preserved=True, original_bytes_not_written=True,
                  original_readback_stable=archived_hashes == original)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('volume', type=Path)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--backup', required=True, type=Path)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--allow-different-original', '--allow-unstable-original', action='store_true',
                        help='Only after reviewing source differences; original bytes are never rewritten')
    parser.add_argument('--archive-name', default='audio-original-20261025')
    parser.add_argument('--staging-name', default='audio-rebuild-20261025')
    args = parser.parse_args()
    print(json.dumps(rebuild(args.volume, args.source, args.backup, args.write,
                             args.allow_different_original, args.archive_name,
                             args.staging_name), indent=2))
