import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import make_jingle as jingle


class Jingle(unittest.TestCase):
    def test_is_short_stereo_32k(self):
        frames = jingle.render_jingle()
        self.assertEqual(32000, jingle.RATE)
        self.assertEqual(2, jingle.CHANNELS)
        seconds = len(frames) / jingle.RATE
        self.assertGreater(seconds, 1.0)
        self.assertLess(seconds, 5.0)
        self.assertEqual(2, len(frames[0]))

    def test_peak_leaves_headroom(self):
        frames = jingle.render_jingle()
        peak = max(abs(sample) for frame in frames for sample in frame)
        self.assertLessEqual(peak, int(32767 * 0.9))
        self.assertGreater(peak, 8000)

    def test_starts_and_ends_silent(self):
        frames = jingle.render_jingle()
        self.assertEqual((0, 0), frames[0])
        self.assertLess(max(abs(s) for s in frames[-1]), 64)

    def test_wav_bytes_are_16_bit_pcm(self):
        frames = [(1000, -1000), (0, 0)]
        data = jingle.wav_bytes(frames)
        self.assertEqual(b"RIFF", data[:4])
        self.assertEqual(len(data) - 8, struct.unpack("<I", data[4:8])[0])
        self.assertEqual(b"WAVEfmt ", data[8:16])
        fmt_size, fmt, channels, rate, byte_rate, block, bits = struct.unpack("<IHHIIHH", data[16:36])
        self.assertEqual((16, 1, 2, 32000, 128000, 4, 16), (fmt_size, fmt, channels, rate, byte_rate, block, bits))
        self.assertEqual(b"data", data[36:40])
        self.assertEqual(8, struct.unpack("<I", data[40:44])[0])
        self.assertEqual(struct.pack("<hhhh", 1000, -1000, 0, 0), data[44:52])


if __name__ == "__main__":
    unittest.main()
