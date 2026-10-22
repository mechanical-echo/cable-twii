#!/usr/bin/env python3
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
LOGO = ROOT / "wii" / "data" / "logo.png"
FONT = ROOT / "wii" / "data" / "font_medium.ttf"
OUT_DIR = ROOT / "forwarder" / "banner"

BANNER_W = 608
BANNER_H = 456
ICON_W = 128
ICON_H = 96
SS = 4
STRIPE_A = (0xF4, 0xF5, 0xF7, 0xFF)
STRIPE_B = (0xEC, 0xEE, 0xF1, 0xFF)
STRIPE_PX = 3
PANEL_FILL = (0xFF, 0xFF, 0xFF, 0xFF)
PANEL_BORDER = (0xD6, 0xDB, 0xE0, 0xFF)
GLOW = (0x34, 0xBE, 0xED, 0x40)
BLUE = (0x34, 0xBE, 0xED, 0xFF)
GREY = (0x44, 0x44, 0x44, 0xFF)
BORDER = 2
GLOW_SPREAD = 3
TAGLINE = "Your own cable TV"
TAGLINE_PX = 26
TAGLINE_GAP = 22
BANNER_PANEL = (84, 136, 523, 319)
BANNER_RADIUS = 22
BANNER_LOGO_W = 330
ICON_PANEL = (6, 14, 121, 81)
ICON_RADIUS = 12
ICON_LOGO_W = 100


def banner_panel_rect():
    return BANNER_PANEL


def icon_panel_rect():
    return ICON_PANEL


def stripes(width, height):
    image = Image.new("RGBA", (width, height), STRIPE_A)
    draw = ImageDraw.Draw(image)
    for row in range(0, height, STRIPE_PX):
        if (row // STRIPE_PX) % 2 == 1:
            draw.rectangle((0, row, width - 1, row + STRIPE_PX - 1), fill=STRIPE_B)
    return image


def panel(width, height, rect, radius):
    layer = Image.new("RGBA", (width * SS, height * SS), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    x0, y0, x1, y1 = (value * SS for value in rect)
    spread = GLOW_SPREAD * SS
    draw.rounded_rectangle((x0 - spread, y0 - spread, x1 + spread, y1 + spread), radius=(radius + GLOW_SPREAD) * SS, fill=GLOW)
    draw.rounded_rectangle((x0, y0, x1, y1), radius=radius * SS, fill=PANEL_FILL, outline=PANEL_BORDER, width=BORDER * SS)
    return layer.resize((width, height), Image.LANCZOS)


def logo(width):
    image = Image.open(LOGO).convert("RGBA")
    return image.resize((width, round(image.height * width / image.width)), Image.LANCZOS)


def render_banner():
    image = stripes(BANNER_W, BANNER_H)
    image.alpha_composite(panel(BANNER_W, BANNER_H, BANNER_PANEL, BANNER_RADIUS))
    mark = logo(BANNER_LOGO_W)
    font = ImageFont.truetype(str(FONT), TAGLINE_PX)
    text_box = font.getbbox(TAGLINE)
    text_h = text_box[3] - text_box[1]
    _, y0, _, y1 = BANNER_PANEL
    top = (y0 + y1 + 1 - (mark.height + TAGLINE_GAP + text_h)) // 2
    image.alpha_composite(mark, ((BANNER_W - mark.width) // 2, top))
    draw = ImageDraw.Draw(image)
    text_x = (BANNER_W - font.getlength(TAGLINE)) / 2
    draw.text((text_x, top + mark.height + TAGLINE_GAP - text_box[1]), TAGLINE, font=font, fill=GREY)
    return image.convert("RGB")


def render_icon():
    image = stripes(ICON_W, ICON_H)
    image.alpha_composite(panel(ICON_W, ICON_H, ICON_PANEL, ICON_RADIUS))
    mark = logo(ICON_LOGO_W)
    _, y0, _, y1 = ICON_PANEL
    image.alpha_composite(mark, ((ICON_W - mark.width) // 2, (y0 + y1 + 1 - mark.height) // 2))
    return image.convert("RGB")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, image in (("banner.png", render_banner()), ("icon.png", render_icon())):
        path = OUT_DIR / name
        image.save(path, optimize=True)
        print(f"{path.relative_to(ROOT)} {image.width}x{image.height}")


if __name__ == "__main__":
    main()
