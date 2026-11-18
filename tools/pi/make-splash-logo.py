#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Draw the placeholder Peepo logo used by the boot splash: a monoline
# "peepo" built from circles and strokes, so no font is involved, with a pair
# of heavy-lidded frog eyes peeping over it. Writes config/pi/splash/peepo/logo.png.
#   python tools/pi/make-splash-logo.py
import math
from pathlib import Path
from PIL import Image, ImageDraw

SCALE = 4                      # drawn large, then reduced for smooth edges
WIDTH, HEIGHT = 460, 250       # final size in pixels (the screen is 800x480)
INK = (0x8e, 0xdb, 0xc2, 255)  # the interface's accent colour
WHITE = (0xe9, 0xee, 0xf2, 255)
PUPIL = (0x10, 0x17, 0x1f, 255)  # the interface's background colour

S = lambda v: round(v * SCALE)
image = Image.new("RGBA", (S(WIDTH), S(HEIGHT)), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)

stroke = 16                    # line width
x_height = 66                  # diameter of every bowl
gap = 14                       # space between letters
base_top = 120                 # top of the x-height
descender = 46


def dot(x, y, d, fill=INK):
    draw.ellipse([S(x - d / 2), S(y - d / 2), S(x + d / 2), S(y + d / 2)], fill=fill)


def line(x0, y0, x1, y1):
    draw.line([S(x0), S(y0), S(x1), S(y1)], fill=INK, width=S(stroke))
    dot(x0, y0, stroke)
    dot(x1, y1, stroke)


def bowl(x, start=0, end=360):
    # A circle of the x-height whose left edge is at x; angles run clockwise
    # from three o'clock, as in Pillow.
    r = x_height / 2 - stroke / 2
    cx, cy = x + x_height / 2, base_top + x_height / 2
    box = [S(cx - r - stroke / 2), S(cy - r - stroke / 2), S(cx + r + stroke / 2), S(cy + r + stroke / 2)]
    draw.arc(box, start, end, fill=INK, width=S(stroke))
    return cx, cy, r


def letter_p(x):
    bowl(x)
    line(x + stroke / 2, base_top + stroke / 2, x + stroke / 2, base_top + x_height + descender)
    return x + x_height + gap


def letter_e(x):
    opening = 55
    cx, cy, r = bowl(x, opening, 360)
    line(cx - r, cy, cx + r, cy)
    a = math.radians(opening)
    dot(cx + r * math.cos(a), cy + r * math.sin(a), stroke)
    return x + x_height + gap


def letter_o(x):
    bowl(x)
    return x + x_height + gap


word = x_height * 5 + gap * 4
x = (WIDTH - word) / 2
x = letter_p(x)
x = letter_e(x)
x = letter_e(x)
x = letter_p(x)
letter_o(x)

# The eyes, peeping over the word in the manner of the Peepo frog: two big
# oval eyes pressed together, lids resting over the top third, and the pupils
# just under the lids, glancing to the right.
OUTLINE = (0x05, 0x08, 0x0b, 255)
eye_rx, eye_ry = 37, 26        # half-width and half-height of each eye
rim = 5                        # dark outline round the eyes and along the lids
eye_y = 72


def oval(cx, cy, rx, ry, fill):
    draw.ellipse([S(cx - rx), S(cy - ry), S(cx + rx), S(cy + ry)], fill=fill)


def arc_points(cx, cy, rx, ry, a0, a1, steps=48):
    # Points on an ellipse from a0 to a1 degrees, clockwise from three o'clock.
    return [(S(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / steps))),
             S(cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / steps)))) for i in range(steps + 1)]


def eye(cx, lid_from, lid_to):
    oval(cx, eye_y, eye_rx + rim / 2, eye_ry + rim / 2, OUTLINE)
    ix, iy = eye_rx - rim / 2, eye_ry - rim / 2
    oval(cx, eye_y, ix, iy, WHITE)
    dot(cx + 11, eye_y + 2, 22, PUPIL)
    dot(cx + 15, eye_y, 6, WHITE)
    # The lid: the top of the eye above a slightly tilted chord.
    draw.polygon(arc_points(cx, eye_y, ix, iy, lid_from, lid_to), fill=INK)
    (x0, y0), (x1, y1) = arc_points(cx, eye_y, eye_rx, eye_ry, lid_from, lid_to, 1)
    draw.line([x0, y0, x1, y1], fill=OUTLINE, width=S(rim))


# Each lid sits a little lower at the outer corner, for the sleepy Peepo look;
# the right eye is drawn last so its rim separates the two where they overlap.
eye(WIDTH / 2 - 33, 194, 338)
eye(WIDTH / 2 + 33, 202, 346)

out = Path(__file__).resolve().parents[2] / "config" / "pi" / "splash" / "peepo" / "logo.png"
out.parent.mkdir(parents=True, exist_ok=True)
image.resize((WIDTH, HEIGHT), Image.LANCZOS).save(out)
print(out)
