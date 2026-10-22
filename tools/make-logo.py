#!/usr/bin/env python3
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "wii" / "data" / "font_bold.ttf"
OUT = ROOT / "wii" / "data" / "logo.png"
ICON_OUT = ROOT / "wii" / "icon.png"

GREY = (0x44, 0x44, 0x44, 0xFF)
BLUE = (0x34, 0xBE, 0xED, 0xFF)
PARTS = [("cable", GREY), ("-", BLUE), ("twii", BLUE)]

HEIGHT = 64
PAD = 6
SS = 4
TRACKING = -0.01

ICON_W = 128
ICON_H = 48
STRIPE_A = (0xF4, 0xF5, 0xF7, 0xFF)
STRIPE_B = (0xEC, 0xEE, 0xF1, 0xFF)
STRIPE_PX = 3
PANEL_FILL = (0xFF, 0xFF, 0xFF, 0xFF)
PANEL_BORDER = (0xD6, 0xDB, 0xE0, 0xFF)
GLOW = (0x34, 0xBE, 0xED, 0x40)
ICON_INSET = 5
ICON_BORDER = 2
ICON_RADIUS = 12
ICON_INK_W = 96
ICON_INK_H = 22


def round_up(value, step):
    return (value + step - 1) // step * step


def wordmark_ink(ink_h):
    full = "".join(text for text, _ in PARTS)
    probe = ImageFont.truetype(str(FONT), ink_h).getbbox(full)
    size = int(ink_h * ink_h / (probe[3] - probe[1]))
    font = ImageFont.truetype(str(FONT), size)
    top = font.getbbox(full)[1]
    track = int(size * TRACKING)
    width = int(font.getlength(full)) + len(full) * abs(track) + 4 * PAD * SS
    canvas = Image.new("RGBA", (width, ink_h + 2 * PAD * SS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    x = PAD * SS
    for text, color in PARTS:
        for ch in text:
            draw.text((x, PAD * SS - top), ch, font=font, fill=color)
            x += font.getlength(ch) + track
    return canvas.crop(canvas.getbbox())


def render():
    ink = wordmark_ink((HEIGHT - 2 * PAD) * SS)
    ink_px = HEIGHT - 2 * PAD
    small = ink.resize((round(ink.width * ink_px / ink.height), ink_px), Image.LANCZOS)
    out_w = round_up(small.width + 2 * PAD, 4)
    logo = Image.new("RGBA", (out_w, HEIGHT), (0, 0, 0, 0))
    logo.alpha_composite(small, ((out_w - small.width) // 2, PAD))
    logo.save(OUT, optimize=True)
    print(f"{OUT.relative_to(ROOT)} {logo.width}x{logo.height}")


def stripes(width, height):
    image = Image.new("RGBA", (width, height), STRIPE_A)
    draw = ImageDraw.Draw(image)
    for row in range(0, height, STRIPE_PX):
        if (row // STRIPE_PX) % 2 == 1:
            draw.rectangle((0, row, width - 1, row + STRIPE_PX - 1), fill=STRIPE_B)
    return image


def panel(width, height):
    layer = Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    glow = (ICON_INSET - 2) * SS
    draw.rounded_rectangle(
        (glow, glow, width * SS - 1 - glow, height * SS - 1 - glow),
        radius=(ICON_RADIUS + 2) * SS,
        fill=GLOW,
    )
    inset = ICON_INSET * SS
    draw.rounded_rectangle(
        (inset, inset, width * SS - 1 - inset, height * SS - 1 - inset),
        radius=ICON_RADIUS * SS,
        fill=PANEL_FILL,
        outline=PANEL_BORDER,
        width=ICON_BORDER * SS,
    )
    return layer.resize((width, height), Image.LANCZOS)


def render_icon():
    ink = wordmark_ink(ICON_INK_H * SS)
    scale = min(ICON_INK_W / ink.width, ICON_INK_H / ink.height)
    small = ink.resize((round(ink.width * scale), round(ink.height * scale)), Image.LANCZOS)
    icon = stripes(ICON_W, ICON_H)
    icon.alpha_composite(panel(ICON_W, ICON_H))
    icon.alpha_composite(small, ((ICON_W - small.width) // 2, (ICON_H - small.height) // 2))
    icon.save(ICON_OUT, optimize=True)
    print(f"{ICON_OUT.relative_to(ROOT)} {icon.width}x{icon.height}")


if __name__ == "__main__":
    render()
    render_icon()
