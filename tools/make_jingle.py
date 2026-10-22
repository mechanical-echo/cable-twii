#!/usr/bin/env python3
import math
import struct
import sys
from pathlib import Path

RATE = 32000
CHANNELS = 2
PEAK = 0.8
LENGTH_S = 2.4
ATTACK_S = 0.012
RELEASE_S = 0.05
HARMONIC = 0.25
NOTES = [
    (523.25, 0.00, 0.35, -0.3),
    (659.25, 0.18, 0.35, -0.1),
    (783.99, 0.36, 0.35, 0.1),
    (1046.50, 0.54, 0.90, 0.3),
]


def tone(t, freq, start, tau):
    if t < start:
        return 0.0
    dt = t - start
    envelope = min(1.0, dt / ATTACK_S) * math.exp(-dt / tau)
    phase = 2.0 * math.pi * freq * dt
    return envelope * (math.sin(phase) + HARMONIC * math.sin(2.0 * phase))


def render_jingle():
    count = int(LENGTH_S * RATE)
    left = [0.0] * count
    right = [0.0] * count
    for freq, start, tau, pan in NOTES:
        gain_l = math.sqrt((1.0 - pan) / 2.0)
        gain_r = math.sqrt((1.0 + pan) / 2.0)
        for i in range(int(start * RATE), count):
            sample = tone(i / RATE, freq, start, tau)
            left[i] += sample * gain_l
            right[i] += sample * gain_r
    for i in range(count):
        tail = min(1.0, (LENGTH_S - i / RATE) / RELEASE_S)
        left[i] *= tail
        right[i] *= tail
    peak = max(max(abs(v) for v in left), max(abs(v) for v in right))
    scale = PEAK * 32767 / peak
    return [(int(round(l * scale)), int(round(r * scale))) for l, r in zip(left, right)]


def wav_bytes(frames):
    data = b"".join(struct.pack("<hh", l, r) for l, r in frames)
    block = CHANNELS * 2
    fmt = struct.pack("<IHHIIHH", 16, 1, CHANNELS, RATE, RATE * block, block, 16)
    body = b"WAVEfmt " + fmt + b"data" + struct.pack("<I", len(data)) + data
    return b"RIFF" + struct.pack("<I", len(body)) + body


def main():
    out = Path(sys.argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)
    frames = render_jingle()
    out.write_bytes(wav_bytes(frames))
    print(f"{out} {len(frames) / RATE:.2f} s")


if __name__ == "__main__":
    main()
