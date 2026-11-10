#!/usr/bin/env python3
"""
Generates placeholder assets so a fresh clone renders something on first run.

These are all procedural - no downloaded images, nothing with a licence question.
Replace the works/ images with real public-domain scans from the Art Institute of
Chicago or Rijksmuseum open-access collections when you are ready; see
docs/ARTWORK.md.

The placeholder "paintings" are Vera Molnar style perturbed grids: a regular grid
whose vertices drift further out of alignment as you move across the canvas. That
is deliberate - it is the same family of algorithmic art Gallery Three exhibits,
so even the placeholders are on-theme.

    python tools/make-placeholder-assets.py
"""

import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "..", "assets")


def noise_tile(size, base, spread, blur=1.4, seed=0):
    """A flat colour with fine grain - enough to catch a raking light."""
    rnd = random.Random(seed)
    img = Image.new("RGB", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            n = rnd.gauss(0.0, spread)
            px[x, y] = tuple(
                max(0, min(255, int(base[i] + n + rnd.gauss(0, spread * 0.3))))
                for i in range(3)
            )
    return img.filter(ImageFilter.GaussianBlur(blur))


def plaster(size=512):
    img = noise_tile(size, (214, 209, 198), 7.0, 1.8, seed=1)
    # faint trowel sweeps
    d = ImageDraw.Draw(img)
    rnd = random.Random(11)
    for _ in range(40):
        x0, y0 = rnd.randrange(size), rnd.randrange(size)
        d.arc([x0 - 180, y0 - 60, x0 + 180, y0 + 60],
              rnd.randrange(360), rnd.randrange(360),
              fill=(206, 201, 191), width=2)
    return img.filter(ImageFilter.GaussianBlur(1.2))


def stone(size=512):
    """Floor: large slabs with a visible joint, so movement reads as movement."""
    img = noise_tile(size, (126, 120, 110), 9.0, 1.2, seed=2)
    d = ImageDraw.Draw(img)
    for i in range(0, size + 1, size // 2):
        d.line([(i, 0), (i, size)], fill=(96, 91, 83), width=4)
        d.line([(0, i), (size, i)], fill=(96, 91, 83), width=4)
    return img


def concrete(size=512):
    """Kahn's vaults are board-formed concrete: fine horizontal banding."""
    img = noise_tile(size, (178, 174, 166), 6.0, 1.5, seed=3)
    d = ImageDraw.Draw(img)
    for y in range(0, size, 46):
        d.line([(0, y), (size, y)], fill=(168, 164, 156), width=2)
        d.ellipse([12, y - 4, 20, y + 4], fill=(160, 156, 149))   # tie holes
        d.ellipse([size - 20, y - 4, size - 12, y + 4], fill=(160, 156, 149))
    return img.filter(ImageFilter.GaussianBlur(0.8))


def marble(size=512):
    img = noise_tile(size, (226, 223, 216), 5.0, 1.0, seed=4)
    d = ImageDraw.Draw(img)
    rnd = random.Random(44)
    for _ in range(18):                                  # veining
        x, y = rnd.randrange(size), rnd.randrange(size)
        pts = [(x, y)]
        for _ in range(34):
            x += rnd.randint(-16, 16)
            y += rnd.randint(2, 14)
            pts.append((x, y))
        d.line(pts, fill=(201, 197, 190), width=rnd.choice([1, 1, 2]))
    return img.filter(ImageFilter.GaussianBlur(0.9))


def gilt(size=512):
    img = noise_tile(size, (176, 143, 78), 11.0, 1.0, seed=5)
    d = ImageDraw.Draw(img)
    rnd = random.Random(55)
    for _ in range(600):                                 # gesso speckle
        x, y = rnd.randrange(size), rnd.randrange(size)
        r = rnd.choice([1, 1, 2])
        tone = rnd.choice([(198, 166, 98), (150, 118, 60)])
        d.ellipse([x - r, y - r, x + r, y + r], fill=tone)
    return img.filter(ImageFilter.GaussianBlur(0.7))


def label(w=512, h=256):
    """Wall label plate. Real text comes later; this is the paper it prints on."""
    img = Image.new("RGBA", (w, h), (238, 234, 223, 255))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, w - 1, h - 1], outline=(206, 200, 186), width=3)
    for i, (y, length, weight) in enumerate(
            [(70, 0.62, 5), (116, 0.44, 3), (156, 0.52, 3), (196, 0.34, 3)]):
        d.line([(44, y), (44 + int((w - 88) * length), y)],
               fill=(120, 114, 100) if i == 0 else (156, 150, 136), width=weight)
    return img


def molnar(index, size=768):
    """
    A Vera Molnar style perturbed grid: start from a regular grid of squares and
    let each vertex drift by an amount that grows across the canvas. Order on one
    side, disorder on the other. About twenty lines, and it is real algorithmic
    art rather than a grey rectangle.
    """
    rnd = random.Random(1000 + index)
    palettes = [
        ((22, 20, 17), (233, 216, 170)),
        ((26, 22, 28), (206, 160, 150)),
        ((18, 24, 26), (150, 196, 190)),
        ((28, 24, 18), (224, 182, 108)),
        ((20, 20, 24), (176, 176, 208)),
        ((24, 18, 18), (212, 140, 120)),
        ((16, 22, 20), (170, 206, 164)),
    ]
    bg, fg = palettes[index % len(palettes)]
    img = Image.new("RGB", (size, size), bg)
    d = ImageDraw.Draw(img)

    cells = 12
    margin = size * 0.08
    step = (size - 2 * margin) / cells

    for row in range(cells):
        for col in range(cells):
            # Disorder grows left to right, which is Molnar's whole move.
            k = (col / (cells - 1)) ** 1.7 * step * 0.42
            x0 = margin + col * step
            y0 = margin + row * step

            pts = []
            for dx, dy in ((0, 0), (1, 0), (1, 1), (0, 1)):
                pts.append((
                    x0 + dx * step * 0.82 + rnd.uniform(-k, k),
                    y0 + dy * step * 0.82 + rnd.uniform(-k, k),
                ))
            pts.append(pts[0])

            shade = 0.55 + 0.45 * math.sin(row * 0.6 + col * 0.3)
            colour = tuple(int(bg[i] + (fg[i] - bg[i]) * shade) for i in range(3))
            d.line(pts, fill=colour, width=2)

    return img.filter(ImageFilter.GaussianBlur(0.4))


def main():
    os.makedirs(os.path.join(ASSETS, "materials"), exist_ok=True)
    os.makedirs(os.path.join(ASSETS, "works"), exist_ok=True)

    mats = {
        "plaster.jpg":  plaster(),
        "stone.jpg":    stone(),
        "concrete.jpg": concrete(),
        "marble.jpg":   marble(),
        "gilt.jpg":     gilt(),
    }
    for name, img in mats.items():
        img.save(os.path.join(ASSETS, "materials", name), quality=92)
        print("materials/" + name)

    label().save(os.path.join(ASSETS, "materials", "label.png"))
    print("materials/label.png")

    for i in range(1, 8):
        name = f"placeholder-{i:02d}.jpg"
        molnar(i).save(os.path.join(ASSETS, "works", name), quality=90)
        print("works/" + name)


if __name__ == "__main__":
    main()
