#!/usr/bin/env python3
"""Generate FangTune icons, banner and README screenshots.

Run from the repository root:
    python tools/make_assets.py

Requires Pillow. The mascot itself lives in tools/icon.svg and is rendered
with ImageMagick before this script runs (or by this script when available).
"""

from __future__ import annotations

import math
import os
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
DOCS = ROOT / "docs"
ASSETS.mkdir(exist_ok=True)
DOCS.mkdir(exist_ok=True)

FONT_DIR = Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"


def font(name: str, size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_DIR / name), size)


BOLD = "segoeuib.ttf"
REG = "segoeui.ttf"
LIGHT = "segoeuil.ttf"
SEMI = "segoeuisl.ttf"


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def gradient(size, top, bottom, diagonal=False):
    w, h = size
    img = Image.new("RGB", size)
    px = img.load()
    for y in range(h):
        for x in range(w):
            t = (y / max(1, h - 1))
            if diagonal:
                t = (x / max(1, w - 1) + y / max(1, h - 1)) * 0.5
            px[x, y] = lerp(top, bottom, t)
    return img


def radial_glow(size, color, radius):
    w, h = size
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    px = img.load()
    cx, cy = w / 2, h / 2
    for y in range(h):
        for x in range(w):
            d = math.hypot((x - cx) / radius, (y - cy) / radius)
            a = max(0.0, 1.0 - d)
            a = a * a
            px[x, y] = (color[0], color[1], color[2], int(color[3] * a))
    return img


def rounded_mask(size, radius):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius, fill=255)
    return mask


def paste_rounded(base, img, xy, radius, shadow=True):
    img = img.convert("RGBA")
    mask = rounded_mask(img.size, radius)
    if shadow:
        sh = Image.new("RGBA", (img.width + 60, img.height + 60), (0, 0, 0, 0))
        sh.paste(Image.new("RGBA", img.size, (0, 0, 0, 150)), (30, 36), mask)
        sh = sh.filter(ImageFilter.GaussianBlur(18))
        base.alpha_composite(sh, (xy[0] - 30, xy[1] - 30))
    base.paste(img, xy, mask)


def text(draw, xy, s, f, fill, anchor="la"):
    draw.text(xy, s, font=f, fill=fill, anchor=anchor)


def draw_play(draw, cx, cy, r, color):
    draw.polygon([(cx - r * 0.42, cy - r * 0.6), (cx - r * 0.42, cy + r * 0.6),
                  (cx + r * 0.62, cy)], fill=color)


def draw_pause(draw, cx, cy, r, color):
    w = r * 0.24
    draw.rounded_rectangle((cx - r * 0.42, cy - r * 0.55, cx - r * 0.42 + w, cy + r * 0.55), w / 2, fill=color)
    draw.rounded_rectangle((cx + r * 0.18, cy - r * 0.55, cx + r * 0.18 + w, cy + r * 0.55), w / 2, fill=color)


def draw_prev(draw, cx, cy, r, color):
    draw.rectangle((cx - r * 0.7, cy - r * 0.55, cx - r * 0.55, cy + r * 0.55), fill=color)
    draw.polygon([(cx + r * 0.6, cy - r * 0.55), (cx + r * 0.6, cy + r * 0.55),
                  (cx - r * 0.45, cy)], fill=color)


def draw_next(draw, cx, cy, r, color):
    draw.rectangle((cx + r * 0.55, cy - r * 0.55, cx + r * 0.7, cy + r * 0.55), fill=color)
    draw.polygon([(cx - r * 0.6, cy - r * 0.55), (cx - r * 0.6, cy + r * 0.55),
                  (cx + r * 0.45, cy)], fill=color)


def make_album(size=512, palette=0):
    palettes = [
        ((24, 12, 56), (236, 72, 153), (46, 213, 115)),
        ((8, 20, 40), (56, 189, 248), (129, 140, 248)),
        ((30, 10, 20), (251, 146, 60), (244, 63, 94)),
        ((6, 24, 20), (45, 212, 191), (163, 230, 53)),
    ]
    top, mid, accent = palettes[palette % len(palettes)]
    img = gradient((size, size), top, mid, diagonal=True).convert("RGBA")

    glow = radial_glow((size, size), (*accent, 190), size * 0.55)
    img.alpha_composite(glow)

    d = ImageDraw.Draw(img)
    cx, cy = size * 0.5, size * 0.52
    for i in range(7):
        rr = size * (0.1 + i * 0.055)
        a = 70 - i * 8
        d.ellipse((cx - rr, cy - rr, cx + rr, cy + rr), outline=(255, 255, 255, a),
                  width=max(2, size // 160))

    # abstract hills
    d.polygon([(0, size), (0, size * 0.72), (size * 0.35, size * 0.55),
               (size * 0.62, size * 0.78), (size, size * 0.62), (size, size)],
              fill=(5, 8, 18, 160))
    d.ellipse((size * 0.58, size * 0.18, size * 0.82, size * 0.42), fill=(255, 255, 255, 220))
    d.ellipse((size * 0.63, size * 0.2, size * 0.88, size * 0.45), fill=(*mid, 255))

    # small note mark
    d.ellipse((size * 0.12, size * 0.78, size * 0.2, size * 0.86), fill=(255, 255, 255, 220))
    d.rectangle((size * 0.192, size * 0.58, size * 0.205, size * 0.83), fill=(255, 255, 255, 220))
    d.polygon([(size * 0.205, size * 0.58), (size * 0.29, size * 0.62),
               (size * 0.205, size * 0.68)], fill=(255, 255, 255, 220))
    return img


def make_icon_sizes():
    src = ASSETS / "icon-1024.png"
    if not src.exists():
        magick = Path(r"C:\Program Files\ImageMagick-7.1.2-Q16-HDRI\magick.exe")
        subprocess.run([str(magick), "-background", "none", str(ROOT / "tools" / "icon.svg"),
                        "-resize", "1024x1024", str(src)], check=True)
    im = Image.open(src).convert("RGBA")
    for s in (256, 48, 24):
        im.resize((s, s), Image.LANCZOS).save(ASSETS / f"icon-{s}.png")
    im.resize((48, 48), Image.LANCZOS).save(ASSETS / "icon.png")


def make_cia_banner():
    W, H = 256, 128
    img = gradient((W, H), (6, 10, 16), (10, 34, 25), diagonal=True).convert("RGBA")
    glow = radial_glow((360, 360), (46, 213, 115, 110), 200)
    img.alpha_composite(glow, (-90, -110))

    icon = Image.open(ASSETS / "icon-1024.png").convert("RGBA").resize((92, 92), Image.LANCZOS)
    paste_rounded(img, icon, (14, 18), 20, shadow=False)

    d = ImageDraw.Draw(img)
    text(d, (122, 28), "FangTune", font(BOLD, 30), (246, 248, 251))
    d.rounded_rectangle((124, 70, 166, 74), 2, fill=(46, 213, 115))
    text(d, (124, 82), "MP3 player for 3DS", font(REG, 13), (154, 162, 176))

    img.convert("RGB").save(ASSETS / "banner-cia.png")


def make_banner():
    W, H = 1600, 900
    img = gradient((W, H), (5, 9, 15), (10, 32, 24), diagonal=True).convert("RGBA")

    glow = radial_glow((1100, 1100), (46, 213, 115, 120), 550)
    img.alpha_composite(glow, (-160, -140))

    d = ImageDraw.Draw(img)

    # subtle diagonal lines
    for x in range(-H, W, 46):
        d.line((x, H, x + H, 0), fill=(255, 255, 255, 7), width=2)

    icon = Image.open(ASSETS / "icon-1024.png").convert("RGBA").resize((560, 560), Image.LANCZOS)
    paste_rounded(img, icon, (150, 170), 120)

    d = ImageDraw.Draw(img)
    text(d, (800, 300), "FangTune", font(BOLD, 186), (246, 248, 251))
    d.rounded_rectangle((806, 512, 1000, 522), 5, fill=(46, 213, 115))

    text(d, (806, 560), "MP3 player for Nintendo 3DS", font(SEMI, 56), (46, 213, 115))
    text(d, (806, 640), "Album art  •  Touch library  •  Shuffle", font(REG, 36), (154, 162, 176))
    text(d, (806, 700), "Homebrew built with devkitPro", font(LIGHT, 30), (101, 110, 126))

    # little badge
    d.rounded_rectangle((806, 748, 1120, 806), 29, fill=(20, 38, 30, 255),
                        outline=(46, 213, 115, 130), width=2)
    text(d, (963, 777), "sdmc:/Music", font(BOLD, 26), (126, 240, 170), anchor="mm")

    img.convert("RGB").save(DOCS / "banner.png")


def top_screen():
    S = 2
    W, H = 400 * S, 240 * S
    img = gradient((W, H), (9, 12, 18), (19, 25, 35)).convert("RGBA")

    glow = radial_glow((620, 620), (46, 213, 115, 50), 300)
    img.alpha_composite(glow, (-20, -160))

    d = ImageDraw.Draw(img)
    text(d, (22 * S, 12 * S), "FangTune", font(BOLD, 10 * S), (46, 213, 115))
    text(d, (378 * S, 15 * S), "MP3 PLAYER", font(REG, 5.6 * S), (101, 110, 126), anchor="ra")

    art = make_album(512).resize((150 * S, 150 * S), Image.LANCZOS)
    paste_rounded(img, art, (22 * S, 46 * S), 12 * S)

    d = ImageDraw.Draw(img)
    text(d, (196 * S, 48 * S), "NOW PLAYING", font(BOLD, 6.4 * S), (46, 213, 115))
    text(d, (196 * S, 66 * S), "Midnight Drive", font(BOLD, 11.2 * S), (246, 248, 251))
    text(d, (196 * S, 108 * S), "Neon Coast", font(REG, 8 * S), (154, 162, 176))
    text(d, (196 * S, 125 * S), "Afterglow", font(REG, 6.4 * S), (101, 110, 126))

    # progress
    px, py, pw, ph = 196 * S, 149 * S, 184 * S, 4 * S
    d.rounded_rectangle((px, py, px + pw, py + ph), ph / 2, fill=(48, 57, 72))
    fill = pw * 0.62
    d.rounded_rectangle((px, py, px + fill, py + ph), ph / 2, fill=(46, 213, 115))
    d.ellipse((px + fill - 6 * S, py + ph / 2 - 6 * S, px + fill + 6 * S, py + ph / 2 + 6 * S),
              fill=(46, 213, 115))
    text(d, (196 * S, 157 * S), "1:42", font(REG, 6 * S), (101, 110, 126))
    text(d, (380 * S, 157 * S), "2:44", font(REG, 6 * S), (101, 110, 126), anchor="ra")

    cy = 194 * S
    d.ellipse((238 * S - 16 * S, cy - 16 * S, 238 * S + 16 * S, cy + 16 * S), fill=(40, 50, 66))
    draw_prev(d, 238 * S, cy, 16 * S, (246, 248, 251))
    d.ellipse((290 * S - 22 * S, cy - 22 * S, 290 * S + 22 * S, cy + 22 * S), fill=(46, 213, 115))
    draw_pause(d, 290 * S, cy, 22 * S, (10, 30, 20))
    d.ellipse((342 * S - 16 * S, cy - 16 * S, 342 * S + 16 * S, cy + 16 * S), fill=(40, 50, 66))
    draw_next(d, 342 * S, cy, 16 * S, (246, 248, 251))

    text(d, (290 * S, 220 * S), "A play/pause    L/R track    D-pad seek",
         font(REG, 5 * S), (101, 110, 126), anchor="ma")

    img.convert("RGB").save(DOCS / "screenshot-top.png")


def bottom_screen():
    S = 2
    W, H = 320 * S, 240 * S
    img = Image.new("RGBA", (W, H), (245, 245, 247, 255))
    d = ImageDraw.Draw(img)

    # header
    d.rectangle((0, 0, W, 56 * S), fill=(255, 255, 255, 255))
    d.line((0, 55 * S, W, 55 * S), fill=(224, 224, 229, 255), width=S)
    text(d, (16 * S, 8 * S), "Library", font(BOLD, 17 * S), (20, 20, 24))
    text(d, (16 * S, 38 * S), "24 songs", font(REG, 7.2 * S), (124, 124, 130))
    d.rounded_rectangle((232 * S, 14 * S, 304 * S, 40 * S), 13 * S, fill=(220, 252, 231))
    text(d, (268 * S, 27 * S), "Shuffle", font(BOLD, 6.8 * S), (21, 128, 61), anchor="mm")

    rows = [
        ("Midnight Drive", "Neon Coast", "2:44", True),
        ("Paper Planes", "The Satellites", "3:18", False),
        ("Golden Hour", "Mira Vale", "2:57", False),
        ("Slow Current", "Harbor Lights", "4:02", False),
        ("Static Bloom", "Neon Coast", "3:31", False),
    ]

    list_top = 56 * S
    row_h = 38 * S
    for i, (title, artist, dur, current) in enumerate(rows):
        y = list_top + i * row_h
        if current:
            d.rectangle((0, y, W, y + row_h), fill=(229, 248, 237))
            d.rectangle((0, y, 3 * S, y + row_h), fill=(46, 213, 115))
        elif i == 1:
            d.rectangle((0, y, W, y + row_h), fill=(238, 238, 243))

        art = make_album(256, i).resize((32 * S, 32 * S), Image.LANCZOS)
        paste_rounded(img, art, (14 * S, y + 3 * S), 7 * S, shadow=False)
        d = ImageDraw.Draw(img)

        text(d, (56 * S, y + 5 * S), title, font(BOLD if current else REG, 7.4 * S),
             (15, 90, 48) if current else (20, 20, 24))
        text(d, (56 * S, y + 21 * S), artist, font(REG, 5.2 * S), (124, 124, 130))

        if current:
            for b in range(3):
                bh = (7 + (b * 4 + i) % 8) * S
                bx = 286 * S + b * 6 * S
                d.rectangle((bx, y + 25 * S - bh, bx + 4 * S, y + 25 * S), fill=(46, 213, 115))
        else:
            text(d, (306 * S, y + 13 * S), dur, font(REG, 4.8 * S), (124, 124, 130), anchor="ra")

        d.line((56 * S, y + row_h - 1, 306 * S, y + row_h - 1), fill=(224, 224, 229), width=1)

    # scrollbar
    d.rounded_rectangle((315 * S, list_top + 2 * S, 318 * S, 196 * S), 2 * S, fill=(226, 226, 231))
    d.rounded_rectangle((315 * S, list_top + 2 * S, 318 * S, list_top + 70 * S), 2 * S, fill=(190, 190, 197))

    # footer
    d.rectangle((0, 200 * S, W, H), fill=(255, 255, 255, 255))
    d.line((0, 200 * S, W, 200 * S), fill=(224, 224, 229), width=S)
    d.rectangle((0, 200 * S, W, 203 * S), fill=(229, 229, 234))
    d.rectangle((0, 200 * S, W * 0.62, 203 * S), fill=(46, 213, 115))

    art = make_album(256).resize((30 * S, 30 * S), Image.LANCZOS)
    paste_rounded(img, art, (12 * S, 207 * S), 7 * S, shadow=False)
    d = ImageDraw.Draw(img)
    text(d, (50 * S, 206 * S), "Midnight Drive", font(BOLD, 7.2 * S), (20, 20, 24))
    text(d, (50 * S, 221 * S), "Neon Coast", font(REG, 5 * S), (124, 124, 130))
    d.ellipse((294 * S - 14 * S, 220 * S - 14 * S, 294 * S + 14 * S, 220 * S + 14 * S),
              fill=(46, 213, 115))
    draw_pause(d, 294 * S, 220 * S, 14 * S, (10, 30, 20))

    img.convert("RGB").save(DOCS / "screenshot-bottom.png")


def preview():
    W, H = 1400, 1340
    img = gradient((W, H), (6, 9, 14), (14, 24, 32), diagonal=True).convert("RGBA")
    glow = radial_glow((1500, 1500), (46, 213, 115, 70), 700)
    img.alpha_composite(glow, (-50, -60))

    # body shadow
    body = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bd = ImageDraw.Draw(body)
    bd.rounded_rectangle((180, 40, 1220, 1280), 92, fill=(18, 21, 28, 255))
    shadow = body.filter(ImageFilter.GaussianBlur(34))
    img.alpha_composite(shadow, (0, 16))

    d = ImageDraw.Draw(img)
    d.rounded_rectangle((180, 40, 1220, 1280), 92, fill=(24, 28, 37, 255),
                        outline=(55, 63, 80, 255), width=3)

    # top screen bezel + camera
    d.rounded_rectangle((276, 116, 1124, 644), 34, fill=(10, 12, 17, 255),
                        outline=(48, 56, 72, 255), width=3)
    d.ellipse((690, 92, 710, 112), fill=(8, 10, 14, 255), outline=(60, 70, 88, 255), width=2)
    d.ellipse((696, 98, 704, 106), fill=(40, 70, 60, 255))

    top = Image.open(DOCS / "screenshot-top.png").convert("RGB").resize((800, 480), Image.LANCZOS)
    img.paste(top, (300, 140))
    d = ImageDraw.Draw(img)

    # hinge
    d.rounded_rectangle((250, 636, 1150, 690), 27, fill=(15, 18, 24, 255),
                        outline=(52, 60, 76, 255), width=2)
    for cx in (300, 1100):
        d.ellipse((cx - 24, 639, cx + 24, 687), fill=(22, 26, 34, 255),
                  outline=(60, 70, 88, 255), width=2)
        d.ellipse((cx - 9, 654, cx + 9, 672), fill=(46, 213, 115, 90))

    # bottom screen bezel
    d.rounded_rectangle((356, 692, 1044, 1208), 30, fill=(10, 12, 17, 255),
                        outline=(48, 56, 72, 255), width=3)
    bottom = Image.open(DOCS / "screenshot-bottom.png").convert("RGB").resize((640, 480), Image.LANCZOS)
    img.paste(bottom, (380, 716))
    d = ImageDraw.Draw(img)

    # home button + wordmark
    d.ellipse((670, 1218, 730, 1272), fill=(22, 26, 34, 255),
              outline=(60, 70, 88, 255), width=3)
    d.ellipse((688, 1234, 712, 1258), fill=(46, 213, 115, 60))
    d.polygon([(700, 1236), (714, 1248), (686, 1248)], fill=(140, 152, 170))
    d.rectangle((691, 1248, 709, 1258), fill=(140, 152, 170))

    img.convert("RGB").save(DOCS / "preview.png")


def main():
    make_icon_sizes()
    make_cia_banner()
    make_banner()
    top_screen()
    bottom_screen()
    preview()
    print("Assets written to assets/ and docs/")


if __name__ == "__main__":
    main()
