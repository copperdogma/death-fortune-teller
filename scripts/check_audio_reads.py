#!/usr/bin/env python3
"""Compare repeated, read-only WAV reads, optionally with a repository reference.

On macOS, F_NOCACHE disables data caching for each read descriptor so repeated
hashes are not merely repeated copies of the process/kernel page-cache buffer.
This never writes the source file or changes its metadata deliberately.
"""
import argparse
import array
import fcntl
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import wave

MAX_BYTES = 32 * 1024 * 1024


def read_once(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        if sys.platform == 'darwin':
            fcntl.fcntl(fd, getattr(fcntl, 'F_NOCACHE', 48), 1)
        length = os.fstat(fd).st_size
        if not 0 < length <= MAX_BYTES:
            raise ValueError(f'Expected 1..{MAX_BYTES} bytes, got {length}')
        data = bytearray()
        while len(data) < length:
            chunk = os.read(fd, min(128 * 1024, length - len(data)))
            if not chunk:
                raise OSError('Short file read')
            data.extend(chunk)
        if os.read(fd, 1):
            raise OSError('File grew while reading')
        return bytes(data)
    finally:
        os.close(fd)


def inspect(data):
    with wave.open(io.BytesIO(data), 'rb') as wav:
        channels, width, rate, frames = (wav.getnchannels(), wav.getsampwidth(),
                                        wav.getframerate(), wav.getnframes())
        if (channels, width, rate) != (2, 2, 44100):
            raise ValueError('Native Death requires PCM stereo 44.1kHz, 16-bit')
        pcm = wav.readframes(frames)
    if len(pcm) != frames * channels * width:
        raise ValueError('Incomplete PCM payload')
    samples = array.array('h', pcm)
    if sys.byteorder != 'little':
        samples.byteswap()
    fnv = 2166136261
    for byte in pcm:
        fnv = ((fnv ^ byte) * 16777619) & 0xffffffff
    return {'file_bytes': len(data), 'file_sha256': hashlib.sha256(data).hexdigest(),
            'pcm_bytes': len(pcm), 'pcm_sha256': hashlib.sha256(pcm).hexdigest(),
            'pcm_fnv1a': f'{fnv:08x}', 'frames': frames,
            'nonzero_samples': sum(value != 0 for value in samples),
            'peak': max(abs(value) for value in samples)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--reference', type=Path)
    parser.add_argument('--passes', type=int, default=3, choices=range(2, 6))
    args = parser.parse_args()
    passes = [inspect(read_once(args.source)) for _ in range(args.passes)]
    stable = len({entry['file_sha256'] for entry in passes}) == 1
    report = {'source': str(args.source.resolve()), 'read_only': True,
              'macos_fd_cache_disabled': sys.platform == 'darwin',
              'passes': passes, 'stable_file_reads': stable}
    if args.reference:
        reference = inspect(read_once(args.reference))
        report['reference'] = reference
        report['matches_reference'] = (stable and passes[0]['file_sha256'] == reference['file_sha256'])
    print(json.dumps(report, indent=2))
    return 0 if stable else 1


if __name__ == '__main__':
    sys.exit(main())
