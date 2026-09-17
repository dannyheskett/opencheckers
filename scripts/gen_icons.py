#!/usr/bin/env python3
"""Generate every launcher / store icon from one vector description.

The icons are the game itself in miniature: a crowned red king in front of a
black man, drawn the way render.c draws pieces (shadow, rim, face, ridge ring,
jewelled gold crown), on the felt green of the table. Keeping them generated rather than
hand-drawn means the palette can never drift from src/render.c, and every size
is produced from the same geometry.

Outputs (run from the repo root, needs Pillow):
    android/res/mipmap-*/ic_launcher.png            legacy square launcher icon
    android/res/mipmap-*/ic_launcher_foreground.png adaptive-icon foreground
    android/play-assets/icon-512.png                Play store listing icon
    android/play-assets/feature-graphic-1024x500.png Play store feature graphic
    ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png  iOS app icon
    ios/app-store-assets/icon-1024.png              App Store listing icon

    scripts/gen_icons.py
"""
import os

from PIL import Image, ImageDraw

# Palette, copied from the constants at the top of src/render.c.
FELT = (12, 92, 52, 255)
FELT_DARK = (10, 76, 44, 255)
RED_PIECE = (198, 52, 48, 255)
RED_HI = (236, 110, 100, 255)
RED_LO = (140, 30, 30, 255)
BLK_PIECE = (48, 48, 56, 255)
BLK_HI = (110, 112, 124, 255)
BLK_LO = (20, 20, 26, 255)
CROWN_GOLD = (235, 200, 60, 255)
CROWN_DARK = (176, 132, 28, 255)
CROWN_LIGHT = (255, 236, 150, 255)

SS = 4  # supersample factor; every shape is drawn large and downscaled


def circle(d, cx, cy, r, **kw):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], **kw)


def crown(d, cx, cy, r):
    """The crown from draw_crown() in src/render.c, sized from the piece radius."""
    w, h = r * 1.00, r * 0.78
    top, bot = cy - h * 0.52, cy + h * 0.48
    band, valley, tip = h * 0.22, top + h * 0.50, top + h * 0.14
    left, right = cx - w / 2, cx + w / 2
    d.rectangle([left, valley, right, bot - band], fill=CROWN_GOLD)
    d.polygon([(left, tip), (left, valley), (cx - w * 0.17, valley)], fill=CROWN_GOLD)
    d.polygon([(cx, top), (cx - w * 0.22, valley), (cx + w * 0.22, valley)], fill=CROWN_GOLD)
    d.polygon([(right, tip), (cx + w * 0.17, valley), (right, valley)], fill=CROWN_GOLD)
    d.rectangle([left, bot - band, right, bot], fill=CROWN_DARK)
    jr = w * 0.075
    for x, y in ((left, tip), (cx, top), (right, tip)):
        circle(d, x, y, jr, fill=CROWN_LIGHT)
    for i in (-1, 0, 1):
        circle(d, cx + i * w * 0.28, bot - band / 2, jr * 0.8, fill=CROWN_LIGHT)


def piece(d, cx, cy, r, red, king):
    """One piece, layered exactly as draw_piece() in src/render.c."""
    base, hi, lo = (RED_PIECE, RED_HI, RED_LO) if red else (BLK_PIECE, BLK_HI, BLK_LO)
    circle(d, cx, cy + r * 0.06, r, fill=(0, 0, 0, 90))       # soft shadow
    circle(d, cx, cy, r, fill=lo)                              # rim
    circle(d, cx, cy, r * 0.86, fill=base)                     # face
    circle(d, cx, cy, r * 0.6, outline=hi, width=max(1, int(r * 0.05)))  # ridge
    if king:
        crown(d, cx, cy, r)


def compose(size, background, content_scale):
    """The icon at `size` px. `background` is None for the adaptive foreground.

    `content_scale` is the fraction of the canvas the piece pair spans, so the
    adaptive foreground can stay inside its 66% safe zone while the legacy
    square icon fills more of its tile.
    """
    n = size * SS
    img = Image.new("RGBA", (n, n), background if background else (0, 0, 0, 0))
    layer = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    # Two overlapping pieces on a diagonal, like a jump in progress: the black
    # man behind, up and left; the red king in front, down and right.
    span = n * content_scale
    r = span * 0.34
    off = span / 2 - r
    piece(d, n / 2 - off, n / 2 - off, r, red=False, king=False)
    piece(d, n / 2 + off, n / 2 + off, r, red=True, king=True)

    img.alpha_composite(layer)
    return img.resize((size, size), Image.LANCZOS)


def feature_graphic(w, h):
    """Play's 1024x500 feature graphic: the icon art on a felt gradient."""
    img = Image.new("RGBA", (w * 2, h * 2), FELT)
    d = ImageDraw.Draw(img)
    for y in range(h * 2):  # subtle vertical shade, dark at the bottom
        t = y / (h * 2)
        d.line([0, y, w * 2, y],
               fill=tuple(int(FELT[i] + (FELT_DARK[i] - FELT[i]) * t) for i in range(3)))
    art = compose(h * 2, None, 0.62)
    img.alpha_composite(art, ((w * 2 - art.width) // 2, 0))
    return img.resize((w, h), Image.LANCZOS)


def save(img, path, opaque=False):
    """Write `img`, flattening away the alpha channel when `opaque` is set.

    Apple rejects an app icon that has an alpha channel outright -- it masks the
    corners itself -- so the iOS icons must be flat RGB. The Android adaptive
    foreground is the opposite case and must keep its transparency.
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if opaque:
        flat = Image.new("RGB", img.size, FELT[:3])
        flat.paste(img, mask=img.split()[3])
        img = flat
    img.save(path)
    print("gen_icons: wrote %s (%dx%d %s)" % (path, img.width, img.height, img.mode))


def main():
    # Legacy square launcher icon and the adaptive foreground, per density. The
    # adaptive foreground canvas is 108dp to the legacy 48dp, and its content
    # must stay inside the central 72dp, hence the smaller content scale.
    for suffix, legacy in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96),
                           ("xxhdpi", 144), ("xxxhdpi", 192)):
        d = "android/res/mipmap-%s" % suffix
        save(compose(legacy, FELT, 0.78), "%s/ic_launcher.png" % d)
        save(compose(legacy * 108 // 48, None, 0.52),
             "%s/ic_launcher_foreground.png" % d)

    save(compose(512, FELT, 0.72), "android/play-assets/icon-512.png")
    save(feature_graphic(1024, 500), "android/play-assets/feature-graphic-1024x500.png",
         opaque=True)

    ios_icon = compose(1024, FELT, 0.72)
    save(ios_icon, "ios/Assets.xcassets/AppIcon.appiconset/icon-1024.png", opaque=True)
    save(ios_icon, "ios/app-store-assets/icon-1024.png", opaque=True)


if __name__ == "__main__":
    main()
