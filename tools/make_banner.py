#!/usr/bin/env python3
"""Generates assets/banner.gif for the README.

The banner is a monochrome starfield that mimics a Glyph Matrix panel: white
pixels on a black OLED-like dot grid, a soft glow behind the project name and
one shooting star per loop. The GIF uses a fixed grayscale palette, so the
animation stays black and white in every browser and both color themes.

Usage:  py tools/make_banner.py
"""

from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "banner.gif"

W, H = 800, 200
FRAMES = 36
FPS = 18
COLORS = 64
DOT_STEP = 4  # faint OLED pixel grid

TITLE = "GLYPH MATRIX"
SUBTITLE = "ESP32 · OLED 128×64"

FONT_CANDIDATES = [
    Path("C:/Windows/Fonts/consolab.ttf"),
    Path("C:/Windows/Fonts/consola.ttf"),
    Path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"),
    Path("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"),
    Path("/System/Library/Fonts/Supplemental/Courier New Bold.ttf"),
]


def pick_font() -> Path:
    for path in FONT_CANDIDATES:
        if path.exists():
            return path
    raise FileNotFoundError("No monospace font found, add one to FONT_CANDIDATES")


def tracked_width(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.FreeTypeFont,
                  tracking: int) -> float:
    return sum(draw.textlength(ch, font=font) for ch in text) + tracking * (len(text) - 1)


def draw_tracked(draw: ImageDraw.ImageDraw, x: float, y: float, text: str,
                 font: ImageFont.FreeTypeFont, fill, tracking: int) -> None:
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill)
        x += draw.textlength(ch, font=font) + tracking


def make_background() -> Image.Image:
    """Black panel with a faint vignette and a dim OLED dot grid."""
    bg = Image.new("RGB", (W, H), (0, 0, 0))
    px = bg.load()
    for y in range(H):
        t = y / (H - 1)
        base = 3 + 7 * t
        for x in range(W):
            # A soft horizontal falloff keeps the edges darker.
            edge = 0.70 + 0.30 * math.sin(math.pi * x / (W - 1))
            v = int(base * edge)
            px[x, y] = (v, v, v)

    grid = ImageDraw.Draw(bg)
    for y in range(DOT_STEP // 2, H, DOT_STEP):
        for x in range(DOT_STEP // 2, W, DOT_STEP):
            grid.point((x, y), fill=(16, 16, 16))
    return bg


class Star:
    __slots__ = ("x", "y", "size", "phase", "speed", "level", "sparkle")

    def __init__(self, x: float, y: float, size: int, phase: float, speed: int,
                 level: int, sparkle: bool) -> None:
        self.x = x
        self.y = y
        self.size = size
        self.phase = phase
        self.speed = speed
        self.level = level
        self.sparkle = sparkle


def make_stars(count: int, box: tuple[float, float, float, float]) -> list[Star]:
    rng = random.Random(20261113)
    stars: list[Star] = []
    x0, y0, x1, y1 = box
    while len(stars) < count:
        x = rng.uniform(4, W - 5)
        y = rng.uniform(4, H - 5)
        if x0 < x < x1 and y0 < y < y1:
            continue  # keep the name readable
        stars.append(Star(
            x=x,
            y=y,
            size=rng.choice([1, 1, 1, 2]),
            phase=rng.uniform(0, math.tau),
            speed=rng.choice([1, 1, 2]),
            level=rng.choice([150, 175, 200, 225, 255]),
            sparkle=rng.random() < 0.14,
        ))
    return stars


def draw_stars(overlay: ImageDraw.ImageDraw, stars: list[Star], frame: int) -> None:
    t = frame / FRAMES
    for s in stars:
        wave = 0.5 + 0.5 * math.sin(s.phase + math.tau * s.speed * t)
        alpha = int(10 + 235 * wave ** 3)
        if alpha <= 16:
            continue
        gray = int(s.level * (0.55 + 0.45 * wave))
        r = s.size
        overlay.ellipse([s.x - r, s.y - r, s.x + r, s.y + r],
                        fill=(gray, gray, gray, alpha))
        if s.sparkle and wave > 0.72:
            arm = 2 + int(3 * wave)
            ray = (gray, gray, gray, max(26, alpha // 2))
            overlay.line([(s.x - arm, s.y), (s.x + arm, s.y)], fill=ray, width=1)
            overlay.line([(s.x, s.y - arm), (s.x, s.y + arm)], fill=ray, width=1)


def draw_shooting_star(overlay: ImageDraw.ImageDraw, frame: int) -> None:
    first, last = 24, 31
    if not (first <= frame <= last):
        return
    t = (frame - first) / (last - first)
    head_x = 690 - 210 * t
    head_y = 22 + 46 * t
    trail = 74
    tail_x = head_x + trail
    tail_y = head_y - trail * 0.30
    fade = math.sin(math.pi * t)
    overlay.line([(tail_x, tail_y), (head_x, head_y)],
                 fill=(255, 255, 255, int(140 * fade)), width=2)
    overlay.ellipse([head_x - 2, head_y - 2, head_x + 2, head_y + 2],
                    fill=(255, 255, 255, int(240 * fade)))


def draw_hud(overlay: ImageDraw.ImageDraw) -> None:
    m, arm = 9, 16
    color = (78, 78, 78, 150)
    for x, y, sx, sy in ((m, m, 1, 1), (W - 1 - m, m, -1, 1),
                         (m, H - 1 - m, 1, -1), (W - 1 - m, H - 1 - m, -1, -1)):
        overlay.line([(x, y), (x + sx * arm, y)], fill=color, width=1)
        overlay.line([(x, y), (x, y + sy * arm)], fill=color, width=1)


def gray_palette(colors: int = COLORS) -> Image.Image:
    """Fixed grayscale ramp, so every frame shares the same black-and-white palette."""
    pal = Image.new("P", (1, 1))
    ramp: list[int] = []
    for i in range(colors):
        v = round(i * 255 / (colors - 1))
        ramp.extend((v, v, v))
    ramp.extend([0] * (768 - len(ramp)))
    pal.putpalette(ramp)
    return pal


def main() -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)

    font_path = pick_font()
    title_size = 66
    title_font = ImageFont.truetype(str(font_path), title_size)
    sub_font = ImageFont.truetype(str(font_path), 15)

    probe = ImageDraw.Draw(Image.new("RGBA", (W, H)))
    tracking = 6
    title_w = tracked_width(probe, TITLE, title_font, tracking)
    title_bbox = title_font.getbbox(TITLE)
    title_h = title_bbox[3] - title_bbox[1]
    title_x = (W - title_w) / 2
    title_y = (H - title_h) / 2 - title_bbox[1] - 14

    sub_tracking = 3
    sub_w = tracked_width(probe, SUBTITLE, sub_font, sub_tracking)
    sub_y = title_y + title_bbox[1] + title_h + 16

    box = (title_x - 34, title_y - 22, title_x + title_w + 34, sub_y + 22)
    stars = make_stars(120, box)

    background = make_background()
    frames: list[Image.Image] = []

    for frame in range(FRAMES):
        img = background.convert("RGBA")
        overlay = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)

        draw_stars(draw, stars, frame)
        draw_shooting_star(draw, frame)
        draw_hud(draw)
        img = Image.alpha_composite(img, overlay)

        # White glow behind the title.
        glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        gdraw = ImageDraw.Draw(glow)
        draw_tracked(gdraw, title_x, title_y, TITLE, title_font,
                     (255, 255, 255, 255), tracking)
        glow = glow.filter(ImageFilter.GaussianBlur(9))
        img = Image.alpha_composite(img, glow)

        # Sharp letters and the subtitle.
        text = ImageDraw.Draw(img)
        draw_tracked(text, title_x, title_y, TITLE, title_font,
                     (255, 255, 255, 255), tracking)
        draw_tracked(text, (W - sub_w) / 2, sub_y, SUBTITLE, sub_font,
                     (150, 150, 150, 230), sub_tracking)

        frames.append(img.convert("RGB"))

    palette = gray_palette()
    paletted = [f.quantize(palette=palette, dither=Image.Dither.NONE) for f in frames]

    paletted[0].save(
        OUT,
        save_all=True,
        append_images=paletted[1:],
        duration=int(1000 / FPS),
        loop=0,
        optimize=True,
        disposal=2,
    )
    print(f"wrote {OUT.relative_to(ROOT)} ({OUT.stat().st_size // 1024} KB, "
          f"{W}x{H}, {FRAMES} frames, {COLORS} gray levels)")


if __name__ == "__main__":
    main()
