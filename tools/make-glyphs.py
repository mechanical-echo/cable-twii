#!/usr/bin/env python3
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "wii" / "data"

SS = 4
GLYPH = 48
STROKE = 4
WHITE = (0xFF, 0xFF, 0xFF, 0xFF)

CURSOR = 32
CURSOR_RIM = 2.1
CURSOR_EDGE = 1.0
CURSOR_STOPS = [(0.0, (0xEE, 0xFA, 0xFF)), (0.3, (0x7F, 0xD0, 0xEF)), (0.6, (0x34, 0xBE, 0xED)), (1.0, (0x2A, 0x8F, 0xB3))]
CURSOR_EDGE_COLOR = (0x2A, 0x8F, 0xB3)


def canvas(side):
    return Image.new("RGBA", (side * SS, side * SS), (0, 0, 0, 0))


def stroke_path(draw, points, closed):
    w = STROKE * SS
    pts = [(x * SS, y * SS) for x, y in points]
    if closed:
        pts.append(pts[0])
    for a, b in zip(pts, pts[1:]):
        draw.line([a, b], fill=WHITE, width=w)
    for x, y in pts:
        draw.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=WHITE)


def finish(img, side, out):
    img.resize((side, side), Image.LANCZOS).save(out)


def home():
    img = canvas(GLYPH)
    draw = ImageDraw.Draw(img)
    stroke_path(draw, [(7, 25), (24, 9), (41, 25)], False)
    stroke_path(draw, [(12, 22), (12, 41), (20, 41)], False)
    stroke_path(draw, [(36, 22), (36, 41), (28, 41)], False)
    finish(img, GLYPH, DATA / "glyph_home.png")


def play():
    img = canvas(GLYPH)
    draw = ImageDraw.Draw(img)
    stroke_path(draw, [(15, 10), (39, 24), (15, 38)], True)
    finish(img, GLYPH, DATA / "glyph_play.png")


def lerp_stops(t):
    for (p0, c0), (p1, c1) in zip(CURSOR_STOPS, CURSOR_STOPS[1:]):
        if t <= p1:
            k = (t - p0) / (p1 - p0) if p1 > p0 else 0.0
            return tuple(round(a + (b - a) * k) for a, b in zip(c0, c1))
    return CURSOR_STOPS[-1][1]


def cursor():
    side = CURSOR * SS
    img = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    px = img.load()
    centre = side / 2.0
    radius = side / 2.0 - 0.5
    rim = CURSOR_RIM * SS
    edge = CURSOR_EDGE * SS
    inner = radius - edge - rim
    hx, hy = centre + (0.35 - 0.5) * 2 * inner, centre + (0.30 - 0.5) * 2 * inner
    for y in range(side):
        for x in range(side):
            dx, dy = x + 0.5 - centre, y + 0.5 - centre
            d = (dx * dx + dy * dy) ** 0.5
            if d > radius:
                continue
            if d > radius - edge:
                px[x, y] = CURSOR_EDGE_COLOR + (255,)
            elif d > inner:
                px[x, y] = (255, 255, 255, 255)
            else:
                gx, gy = x + 0.5 - hx, y + 0.5 - hy
                t = min(1.0, ((gx * gx + gy * gy) ** 0.5) / (inner * 1.15))
                px[x, y] = lerp_stops(t) + (255,)
    finish(img, CURSOR, DATA / "cursor.png")


if __name__ == "__main__":
    home()
    play()
    cursor()
