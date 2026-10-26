#!/usr/bin/env python3
"""Read-only preflight for the native skull audio contract; no device access."""
import argparse
import json
import math
from pathlib import Path
import re
import struct


class AssetError(ValueError):
    pass


def inspect_wave(path):
    """Walk RIFF metadata without loading the PCM payload into RAM."""
    with Path(path).open('rb') as stream:
        size = stream.seek(0, 2)
        stream.seek(0)
        header = stream.read(12)
        if len(header) != 12 or header[:4] != b'RIFF' or header[8:] != b'WAVE':
            raise AssetError('expected RIFF/WAVE header')
        limit = struct.unpack_from('<I', header, 4)[0] + 8
        if limit != size:
            raise AssetError('RIFF length does not match file size')
        position, fmt, data = 12, None, None
        while position < limit:
            if limit - position < 8:
                raise AssetError('truncated chunk header')
            stream.seek(position)
            kind, length = struct.unpack('<4sI', stream.read(8))
            payload = position + 8
            following = payload + length + (length & 1)
            if following > limit:
                raise AssetError('truncated chunk payload or padding')
            if kind == b'fmt ':
                if fmt is not None or length < 16:
                    raise AssetError('duplicate or short fmt chunk')
                fmt = struct.unpack('<HHIIHH', stream.read(16))
            elif kind == b'data':
                if data is not None:
                    raise AssetError('duplicate data chunk')
                data = (payload, length)
            position = following
        if fmt is None or data is None:
            raise AssetError('missing fmt or data chunk')
        encoding, channels, rate, byte_rate, alignment, bits = fmt
        if (encoding, channels, rate, bits) != (1, 2, 44100, 16):
            raise AssetError('requires PCM 44.1 kHz stereo 16-bit')
        if alignment != 4 or byte_rate != rate * alignment:
            raise AssetError('inconsistent frame alignment or byte rate')
        offset, length = data
        if not length or length % alignment:
            raise AssetError('empty or incomplete PCM frame data')
        return dict(data_offset=offset, data_bytes=length, frames=length // 4,
                    sample_rate=rate, channels=channels, bits=bits)


def boundary_frame(milliseconds, rate=44100):
    return (milliseconds * rate + 999) // 1000


def inspect_script(path, wave):
    intervals, warnings = [], []
    previous_end = 0
    for number, raw in enumerate(Path(path).read_text(encoding='utf-8-sig').splitlines(), 1):
        if not raw.strip():
            continue
        fields = [field.strip() for field in raw.split(',')]
        if len(fields) not in (3, 4) or fields[0] not in ('A', 'B'):
            raise AssetError(f'line {number}: expected A|B,start_ms,duration_ms[,jaw]')
        if not all(re.fullmatch(r'[0-9]+', value) for value in fields[1:3]):
            raise AssetError(f'line {number}: times must be unsigned integers')
        start, duration = map(int, fields[1:3])
        end = start + duration
        if duration == 0 or end > 0xffffffff:
            raise AssetError(f'line {number}: zero duration or 32-bit time overflow')
        if start < previous_end:
            raise AssetError(f'line {number}: unordered or overlapping interval')
        first, last = boundary_frame(start), boundary_frame(end)
        if last > wave['frames']:
            raise AssetError(f'line {number}: interval extends beyond PCM data')
        if len(fields) == 4:
            try:
                jaw = float(fields[3])
            except ValueError as exc:
                raise AssetError(f'line {number}: invalid jaw metadata') from exc
            if not math.isfinite(jaw):
                raise AssetError(f'line {number}: nonfinite jaw metadata')
            warnings.append(f'line {number}: jaw metadata is unused by legacy playback')
        intervals.append(dict(line=number, role=fields[0], start_frame=first,
                              end_frame=last, gap_before_ms=start - previous_end))
        previous_end = end
    if not intervals:
        raise AssetError('empty script')
    return dict(intervals=intervals, warnings=warnings)


def validate_directory(directory):
    directory = Path(directory)
    report = dict(audio_directory=str(directory.resolve()), waves=[], errors=[])
    if not directory.is_dir():
        report['errors'].append('audio directory does not exist')
        return report
    files = sorted(directory.iterdir())
    waves = [p for p in files if p.is_file() and p.suffix.lower() == '.wav']
    if not waves:
        report['errors'].append('no WAV assets found')
    for path in waves:
        try:
            entry = dict(file=path.name, **inspect_wave(path))
            report['waves'].append(entry)
            script = path.with_suffix('.txt')
            if path.name.startswith('Skit') and not script.is_file():
                raise AssetError('skit has no paired .txt script')
            if script.is_file():
                entry['script'] = inspect_script(script, entry)
        except (AssetError, OSError, UnicodeError) as exc:
            report['errors'].append(f'{path.name}: {exc}')
    for script in files:
        if script.is_file() and script.suffix.lower() == '.txt' and not script.with_suffix('.wav').is_file():
            report['errors'].append(f'{script.name}: script has no paired .wav')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('audio_directory', type=Path)
    args = parser.parse_args()
    report = validate_directory(args.audio_directory)
    print(json.dumps(report, indent=2))
    return bool(report['errors'])


if __name__ == '__main__':
    raise SystemExit(main())
