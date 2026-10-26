#!/usr/bin/env python3
"""Back up SD user files, then exclusively create one verified diagnostic clip.

No formatting, overwrites, renames, or deletions of existing card files.
Protected macOS volume bookkeeping is excluded from the user-file backup.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

from check_audio_reads import read_once

NAME = 'initialized-readtest-20261025.wav'
EXPECTED = 'f5b6edd542b78ca14a9f442f7366d515edef75b0344bdb327fe1f29fdd277ba0'
SYSTEM_DIRS = {'.Spotlight-V100', '.Trashes', '.fseventsd', '.TemporaryItems'}


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        while chunk := source.read(128 * 1024):
            value.update(chunk)
    return value.hexdigest()


def scan(volume):
    files, skipped = [], []
    for current, directories, names in os.walk(volume, followlinks=False,
                                               onerror=lambda error: (_ for _ in ()).throw(error)):
        current = Path(current)
        if current == volume:
            skipped = sorted(set(directories) & SYSTEM_DIRS)
            directories[:] = [name for name in directories if name not in SYSTEM_DIRS]
        for name in directories + names:
            if (current / name).is_symlink():
                raise ValueError('Symlinks are not supported in this bounded card backup')
        for name in names:
            path = current / name
            if not path.is_file():
                raise ValueError('Unexpected non-file entry')
            files.append(path.relative_to(volume))
    if sum((volume / path).stat().st_size for path in files) > 2 * 1024**3:
        raise ValueError('Backup exceeds the expected card-size bound')
    return sorted(files), skipped


def stage(volume, backup, reference, write=False):
    volume, backup, reference = volume.resolve(), backup.resolve(), reference.resolve()
    if backup.is_relative_to(volume) or volume.is_relative_to(backup):
        raise ValueError('Backup and card must be separate directory trees')
    target = volume / 'audio' / NAME
    original = volume / 'audio' / 'initialized.wav'
    if target.exists() or target.is_symlink():
        raise FileExistsError('Diagnostic target already exists; refusing overwrite')
    data = reference.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXPECTED:
        raise ValueError('Repository reference differs from the qualified asset')
    if hashlib.sha256(read_once(original)).hexdigest() != EXPECTED:
        raise ValueError('Card original differs from the qualified asset')
    files, skipped = scan(volume)
    report = {'files': len(files), 'target': str(target), 'bytes': len(data),
              'sha256': EXPECTED, 'backup': str(backup), 'skipped_system_dirs': skipped}
    if not write:
        return report
    backup.mkdir(mode=0o700, parents=False, exist_ok=False)
    entries = []
    for relative in files:
        source, destination = volume / relative, backup / relative
        destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
        before = source.stat()
        value = hashlib.sha256()
        with source.open('rb') as reader, destination.open('xb') as writer:
            os.chmod(destination, 0o600)
            while chunk := reader.read(128 * 1024):
                value.update(chunk)
                writer.write(chunk)
            writer.flush()
            os.fsync(writer.fileno())
        after = source.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise ValueError('Card file changed during backup; no test file created')
        if digest(destination) != value.hexdigest() or digest(source) != value.hexdigest():
            raise ValueError('Backup verification failed; no test file created')
        entries.append({'path': str(relative), 'bytes': before.st_size,
                        'sha256': value.hexdigest()})
    manifest = backup / 'backup-manifest.json'
    with manifest.open('x') as output:
        os.chmod(manifest, 0o600)
        json.dump({'files': entries, 'skipped_system_dirs': skipped}, output, indent=2)
    # Existing source bytes must still match immediately before the only card write.
    if hashlib.sha256(read_once(original)).hexdigest() != EXPECTED:
        raise ValueError('Original changed before test creation; no test file created')
    created = False
    try:
        fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        created = True
        with os.fdopen(fd, 'wb') as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        if hashlib.sha256(read_once(target)).hexdigest() != EXPECTED:
            raise ValueError('New test copy read-back verification failed')
        if hashlib.sha256(read_once(original)).hexdigest() != EXPECTED:
            raise ValueError('Original no longer matches reference')
    except BaseException:
        if created:
            target.unlink()  # Only the new file this invocation created.
        raise
    report.update(backup_verified=True, test_copy_verified=True, original_unchanged=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--volume', type=Path, required=True)
    parser.add_argument('--backup', type=Path, required=True)
    parser.add_argument('--reference', type=Path, default=Path(__file__).resolve().parents[1] / 'sd-card-files/audio/initialized.wav')
    parser.add_argument('--write', action='store_true', help='Requires user authorization; otherwise only inspect the plan')
    args = parser.parse_args()
    print(json.dumps(stage(args.volume, args.backup, args.reference, args.write), indent=2))


if __name__ == '__main__':
    main()
