#!/usr/bin/env python3
"""Draw Resources/Icon.png: 1024 square, #0a0a0f field, #c9a84c parametric curve."""

import math

from PIL import Image, ImageDraw, ImageFilter

W = 1024
BG = (0x0A, 0x0A, 0x0F, 255)
GOLD = (0xC9, 0xA8, 0x4C, 255)
GOLD_SOFT = (0xE4, 0xC5, 0x6B, 255)


def curve_point(t: float) -> tuple[float, float]:
    """A parametric response: low shelf, a cut, a bell, then a gentle high shelf."""
    x = 150 + t * 724
    shelf = 18 * math.tanh((t - 0.16) * 10)
    dip = -78 * math.exp(-((t - 0.40) / 0.07) ** 2)
    bell = 150 * math.exp(-((t - 0.66) / 0.09) ** 2)
    air = 36 * math.tanh((t - 0.86) * 8)
    y = 620 - shelf - dip - bell - air
    return x, y


def main() -> None:
    img = Image.new("RGBA", (W, W), BG)
    glow = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    g = ImageDraw.Draw(glow)
    g.ellipse((120, 160, 904, 900), fill=(0xC9, 0xA8, 0x4C, 28))
    glow = glow.filter(ImageFilter.GaussianBlur(48))
    img = Image.alpha_composite(img, glow)

    draw = ImageDraw.Draw(img)

    # Frame
    draw.rounded_rectangle((78, 78, 946, 946), radius=96, outline=GOLD, width=10)
    draw.rounded_rectangle((108, 108, 916, 916), radius=76, outline=(0xC9, 0xA8, 0x4C, 90), width=3)

    # Diamond mark, same motif as the editor title rule
    cx, cy = 512, 196
    s = 22
    draw.polygon([(cx, cy - s), (cx + s, cy), (cx, cy + s), (cx - s, cy)], fill=GOLD)
    draw.line((250, cy, cx - s - 16, cy), fill=GOLD, width=4)
    draw.line((cx + s + 16, cy, 774, cy), fill=GOLD, width=4)

    # Baseline of 0 dB
    draw.line((168, 620, 856, 620), fill=(0xC9, 0xA8, 0x4C, 70), width=2)

    points = [curve_point(i / 240) for i in range(241)]
    halo = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    ImageDraw.Draw(halo).line(points, fill=(0xC9, 0xA8, 0x4C, 80), width=28, joint="curve")
    halo = halo.filter(ImageFilter.GaussianBlur(8))
    img = Image.alpha_composite(img, halo)
    draw = ImageDraw.Draw(img)
    draw.line(points, fill=GOLD, width=14, joint="curve")

    # Nodes sitting on the curve
    for t, radius in ((0.18, 16), (0.40, 18), (0.66, 26), (0.88, 16)):
        x, y = curve_point(t)
        draw.ellipse((x - radius - 8, y - radius - 8, x + radius + 8, y + radius + 8), outline=GOLD_SOFT, width=3)
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=BG, outline=GOLD, width=6)

    out = Image.new("RGB", (W, W), (0x0A, 0x0A, 0x0F))
    out.paste(img, mask=img.split()[-1])
    out.save("Resources/Icon.png", "PNG", optimize=True)
    print(f"wrote Resources/Icon.png {out.size}")


if __name__ == "__main__":
    main()
