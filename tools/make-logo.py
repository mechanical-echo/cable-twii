#!/usr/bin/env python3
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "wii" / "data" / "font_bold.ttf"
OUT = ROOT / "wii" / "data" / "logo.png"

GREY = (0x44, 0x44, 0x44, 0xFF)
BLUE = (0x34, 0xBE, 0xED, 0xFF)
PARTS = [("cable", GREY), ("-", BLUE), ("twii", BLUE)]

HEIGHT = 64
PAD = 6
SS = 4
TRACKING = -0.01


def round_up(value, step):
    return (value + step - 1) // step * step


def render():
    ink_h = (HEIGHT - 2 * PAD) * SS
    full = "".join(text for text, _ in PARTS)
    probe = ImageFont.truetype(str(FONT), ink_h).getbbox(full)
    size = int(ink_h * ink_h / (probe[3] - probe[1]))
    font = ImageFont.truetype(str(FONT), size)
    top = font.getbbox(full)[1]
    track = int(size * TRACKING)
    width = int(font.getlength(full)) + len(full) * abs(track) + 4 * PAD * SS
    canvas = Image.new("RGBA", (width, HEIGHT * SS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    x = PAD * SS
    for text, color in PARTS:
        for ch in text:
            draw.text((x, PAD * SS - top), ch, font=font, fill=color)
            x += font.getlength(ch) + track
    ink = canvas.crop(canvas.getbbox())
    ink_px = HEIGHT - 2 * PAD
    small = ink.resize((round(ink.width * ink_px / ink.height), ink_px), Image.LANCZOS)
    out_w = round_up(small.width + 2 * PAD, 4)
    logo = Image.new("RGBA", (out_w, HEIGHT), (0, 0, 0, 0))
    logo.alpha_composite(small, ((out_w - small.width) // 2, PAD))
    logo.save(OUT, optimize=True)
    print(f"{OUT.relative_to(ROOT)} {logo.width}x{logo.height}")


if __name__ == "__main__":
    render()
