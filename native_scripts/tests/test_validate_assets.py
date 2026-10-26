import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('assets', Path(__file__).parents[1] / 'validate_assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


def chunk(kind, payload):
    return kind + struct.pack('<I', len(payload)) + payload + bytes(len(payload) & 1)


def wave(*chunks):
    body = b'WAVE' + b''.join(chunks)
    return b'RIFF' + struct.pack('<I', len(body)) + body


FMT = chunk(b'fmt ', struct.pack('<HHIIHH', 1, 2, 44100, 176400, 4, 16))
DATA = chunk(b'data', bytes(176400))


class AssetsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.wav = self.root / 'Skit - test.wav'
        self.script = self.wav.with_suffix('.txt')

    def parse(self, content):
        self.wav.write_bytes(content)
        return assets.inspect_wave(self.wav)

    def test_chunk_order_and_odd_padding(self):
        result = self.parse(wave(chunk(b'JUNK', b'x'), DATA, FMT))
        self.assertEqual(result['data_offset'], 30)
        self.assertEqual(result['frames'], 44100)

    def test_rejects_truncation_and_bad_lengths(self):
        valid = wave(FMT, DATA)
        for content in (b'', valid[:-1], valid + b'x', wave(FMT, b'data\xff\xff\xff\xff')):
            with self.subTest(length=len(content)), self.assertRaises(assets.AssetError):
                self.parse(content)

    def test_rejects_missing_duplicate_or_incomplete_chunks(self):
        for content in (wave(DATA), wave(FMT), wave(FMT, FMT, DATA),
                        wave(FMT, DATA, DATA), wave(FMT, chunk(b'data', b'abc')),
                        wave(FMT, chunk(b'data', b''))):
            with self.subTest(content_length=len(content)), self.assertRaises(assets.AssetError):
                self.parse(content)

    def test_rejects_unsupported_or_inconsistent_format(self):
        for fmt in ((3, 2, 44100, 176400, 4, 16), (1, 1, 44100, 88200, 2, 16),
                    (1, 2, 48000, 192000, 4, 16), (1, 2, 44100, 1, 4, 16)):
            with self.subTest(fmt=fmt), self.assertRaises(assets.AssetError):
                self.parse(wave(chunk(b'fmt ', struct.pack('<HHIIHH', *fmt)), DATA))

    def test_role_boundaries_and_gap(self):
        metadata = self.parse(wave(FMT, DATA))
        self.script.write_text('A,0,1\nB,1,1\nB,10,990\n')
        result = assets.inspect_script(self.script, metadata)['intervals']
        self.assertEqual((result[0]['start_frame'], result[0]['end_frame']), (0, 45))
        self.assertEqual(result[1]['start_frame'], 45)
        self.assertEqual(result[2]['gap_before_ms'], 8)
        self.assertEqual(result[2]['end_frame'], 44100)

    def test_rejects_bad_scripts(self):
        metadata = self.parse(wave(FMT, DATA))
        for text in ('', 'C,0,1', 'A,-1,1', 'A,0,0', 'A,0,1.5',
                     'A,0,1001', 'A,0,100\nB,99,1', 'A,4294967295,1',
                     'A,0,1,nan', 'A,0,1,1,extra'):
            self.script.write_text(text)
            with self.subTest(text=text), self.assertRaises(assets.AssetError):
                assets.inspect_script(self.script, metadata)

    def test_pairing_and_missing_directory(self):
        self.parse(wave(FMT, DATA))
        self.assertTrue(assets.validate_directory(self.root)['errors'])
        self.script.write_text('A,0,1000')
        self.assertFalse(assets.validate_directory(self.root)['errors'])
        self.assertTrue(assets.validate_directory(self.root / 'missing')['errors'])


if __name__ == '__main__':
    unittest.main()
