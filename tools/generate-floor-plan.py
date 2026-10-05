#!/usr/bin/env python3
"""
LUMEN - gallery floor plan generator.

Single source of truth for the gallery's 2D plan.

Emits:
  2026-10-01-lumen-floor-plan.svg   presentation drawing
  2026-10-01-lumen-plan.json        machine-readable plan (walls, rooms, doors)

The JSON is the data the C++ side loads: walls become AABB colliders and the
line list the minimap rasterises with Bresenham; room rects become the
threshold tests that trigger each room's vault light.

Units are metres. Plan +y is north. In the engine this maps to
x -> +X (east), y -> -Z (north), with +Y up.
"""

import json

S = 20.0          # px per metre
OX = -20.0        # sx = OX + S*x
OY = 90.0         # sy = OY + S*(YMAX - y)
YMAX = 36.0
W, H = 1600, 900

WALL_T = 0.45     # metres

# --- palette (matches the LUMEN deck: near-black ground, gold, cream) --------
BG       = "#14110D"
POCHE    = "#E6D6A4"   # wall fill / stroke
FLOOR_R  = "#2C2518"   # room floor  (lit)
FLOOR_C  = "#17130C"   # corridor floor (unlit)
FLOOR_A  = "#2B2317"   # atrium floor
FLOOR_V  = "#1E1A13"   # vestibule floor
GOLD     = "#F2D479"
CREAM    = "#F0E5C6"
MUTE     = "#9A8F72"
GRIDC    = "#2A251A"


def sx(x): return OX + S * x
def sy(y): return OY + S * (YMAX - y)


# --- the plan ----------------------------------------------------------------
# Ring corridor: outer 11..33 x 9..27, inner (atrium) 14..30 x 12..24  -> 3 m wide
RING_OUT = (11.0, 9.0, 33.0, 27.0)
ATRIUM   = (14.0, 12.0, 30.0, 24.0)
VEST     = (4.0, 16.0, 11.0, 22.0)

ROOMS = [
    dict(id="R1", name="Gallery One",   owner="Shawn Kuria",      rect=(11.0, 27.0, 21.0, 36.0),
         door=("S", 15.0, 18.0), vault_axis="y", vaults=2),
    dict(id="R2", name="Gallery Two",   owner="Charles Murage",   rect=(23.0, 27.0, 33.0, 36.0),
         door=("S", 26.0, 29.0), vault_axis="y", vaults=2),
    dict(id="R3", name="Gallery Three", owner="Timothy Kimani",   rect=(33.0, 11.0, 45.0, 25.0),
         door=("W", 16.0, 20.0), vault_axis="y", vaults=2, tag="generated artwork"),
    dict(id="R4", name="Gallery Four",  owner="Erykah Diang'a",   rect=(23.0, 0.0, 33.0, 9.0),
         door=("N", 26.0, 29.0), vault_axis="y", vaults=2),
    dict(id="R5", name="Gallery Five",  owner="Priscillah Njoroge", rect=(11.0, 0.0, 21.0, 9.0),
         door=("N", 15.0, 18.0), vault_axis="y", vaults=2),
]

ATRIUM_DOOR = ("W", 16.0, 20.0)     # atrium opening onto the west corridor leg
VEST_DOOR   = ("E", 17.5, 20.5)     # vestibule into the west corridor leg

# recessed sculpture niches: (x0, y0, x1, y1) cut into the corridor's outer wall
NICHES = [
    (21.0, 27.0, 23.0, 28.6),
    (21.0,  7.4, 23.0,  9.0),
    ( 9.4, 10.5, 11.0, 13.5),
    ( 9.4, 22.5, 11.0, 25.5),
]

# suggested route: the corridor centreline, plus a stub in from the entrance
ROUTE = [(12.5, 19.0), (12.5, 25.5), (31.5, 25.5), (31.5, 10.5),
         (12.5, 10.5), (12.5, 19.0)]
ROUTE_STUB = [(8.0, 19.0), (12.5, 19.0)]


def rect_walls(r):
    x0, y0, x1, y1 = r
    return [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)),
            ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]


def door_seg(rect, door):
    """Return the (p0,p1) segment of a doorway on a room rect."""
    x0, y0, x1, y1 = rect
    side, a, b = door
    if side == "N": return ((a, y1), (b, y1))
    if side == "S": return ((a, y0), (b, y0))
    if side == "W": return ((x0, a), (x0, b))
    return ((x1, a), (x1, b))


def hang_marks(room):
    """Painting hang positions, set on the inner face of each wall."""
    x0, y0, x1, y1 = room["rect"]
    side = room["door"][0]
    k = 0.35                      # inset from the wall centreline
    marks = []

    def along(p0, p1, n, span=2.2):
        (ax, ay), (bx, by) = p0, p1
        for i in range(n):
            t = (i + 0.5) / n
            cx, cy = ax + (bx - ax) * t, ay + (by - ay) * t
            if abs(by - ay) < 1e-6:
                marks.append(((cx - span / 2, cy), (cx + span / 2, cy)))
            else:
                marks.append(((cx, cy - span / 2), (cx, cy + span / 2)))

    if side in ("N", "S"):
        far_y = y0 + k if side == "N" else y1 - k
        along((x0 + 1.4, far_y), (x1 - 1.4, far_y), 3)
        along((x0 + k, y0 + 1.6), (x0 + k, y1 - 1.6), 2)
        along((x1 - k, y0 + 1.6), (x1 - k, y1 - 1.6), 2)
    else:
        far_x = x0 + k if side == "E" else x1 - k
        along((far_x, y0 + 1.4), (far_x, y1 - 1.4), 3)
        along((x0 + 1.6, y0 + k), (x1 - 1.6, y0 + k), 2)
        along((x0 + 1.6, y1 - k), (x1 - 1.6, y1 - k), 2)
    return marks


def vault_lines(room):
    """Crown lines of the Kimbell-style vaults (the light slots)."""
    x0, y0, x1, y1 = room["rect"]
    n = room["vaults"]
    out = []
    if room["vault_axis"] == "y":
        for i in range(n):
            cx = x0 + (x1 - x0) * (i + 0.5) / n
            out.append(((cx, y0 + 0.9), (cx, y1 - 0.9)))
    else:
        for i in range(n):
            cy = y0 + (y1 - y0) * (i + 0.5) / n
            out.append(((x0 + 0.9, cy), (x1 - 0.9, cy)))
    return out


# --- svg ---------------------------------------------------------------------
P = []
def add(s): P.append(s)

def line(p0, p1, stroke, w, dash=None, cap="butt", op=1.0):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    add(f'<line x1="{sx(p0[0]):.1f}" y1="{sy(p0[1]):.1f}" x2="{sx(p1[0]):.1f}" '
        f'y2="{sy(p1[1]):.1f}" stroke="{stroke}" stroke-width="{w}" '
        f'stroke-linecap="{cap}" opacity="{op}"{d}/>')

def frect(r, fill, op=1.0):
    x0, y0, x1, y1 = r
    add(f'<rect x="{sx(x0):.1f}" y="{sy(y1):.1f}" width="{(x1-x0)*S:.1f}" '
        f'height="{(y1-y0)*S:.1f}" fill="{fill}" opacity="{op}"/>')

def text(x, y, s, fill=CREAM, size=15, anchor="middle", weight="400",
         ls="0", fam="Georgia, 'Times New Roman', serif", op=1.0):
    add(f'<text x="{x:.1f}" y="{y:.1f}" fill="{fill}" font-size="{size}" '
        f'font-family="{fam}" font-weight="{weight}" text-anchor="{anchor}" '
        f'letter-spacing="{ls}" opacity="{op}">{s}</text>')


add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
    f'viewBox="0 0 {W} {H}" role="img">')
add('<title>LUMEN gallery floor plan</title>')
add('<desc>A loop corridor around a central atrium, with five vaulted galleries '
    'opening off its outer face and sculpture niches in the corridor wall.</desc>')
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# 5 m grid
add('<g opacity="0.5">')
for gx in range(0, 51, 5):
    line((gx, -1), (gx, 37), GRIDC, 1)
for gy in range(0, 41, 5):
    line((-1, gy), (47, gy), GRIDC, 1)
add('</g>')

# floors
frect(VEST, FLOOR_V)
x0, y0, x1, y1 = RING_OUT
ax0, ay0, ax1, ay1 = ATRIUM
add(f'<path d="M{sx(x0):.1f} {sy(y0):.1f} H{sx(x1):.1f} V{sy(y1):.1f} H{sx(x0):.1f} Z '
    f'M{sx(ax0):.1f} {sy(ay0):.1f} H{sx(ax1):.1f} V{sy(ay1):.1f} H{sx(ax0):.1f} Z" '
    f'fill="{FLOOR_C}" fill-rule="evenodd"/>')
frect(ATRIUM, FLOOR_A)
for r in ROOMS:
    frect(r["rect"], FLOOR_R)
for n in NICHES:
    frect(n, FLOOR_C)

# atrium oculus
add(f'<circle cx="{sx(22):.1f}" cy="{sy(18):.1f}" r="{3.0*S:.1f}" fill="{GOLD}" opacity="0.07"/>')
add(f'<circle cx="{sx(22):.1f}" cy="{sy(18):.1f}" r="{3.0*S:.1f}" fill="none" '
    f'stroke="{GOLD}" stroke-width="1.2" stroke-dasharray="5 5" opacity="0.55"/>')

# vault crown slots
for r in ROOMS:
    for p0, p1 in vault_lines(r):
        line(p0, p1, GOLD, 2.4, dash="9 7", op=0.5)

# hang positions
for r in ROOMS:
    for p0, p1 in hang_marks(r):
        line(p0, p1, GOLD, 5, cap="round", op=0.8)

# walls
WT = WALL_T * S
for r in [RING_OUT, ATRIUM, VEST] + [x["rect"] for x in ROOMS] + NICHES:
    for p0, p1 in rect_walls(r):
        line(p0, p1, POCHE, WT)

# door cuts
cuts = [(r["rect"], r["door"]) for r in ROOMS]
cuts.append((ATRIUM, ATRIUM_DOOR))
cuts.append((VEST, VEST_DOOR))
for rect, door in cuts:
    p0, p1 = door_seg(rect, door)
    line(p0, p1, BG, WT + 2)

# re-floor the cut openings so they read as thresholds, not holes
for rect, door in cuts:
    p0, p1 = door_seg(rect, door)
    line(p0, p1, FLOOR_C, WT - 3)

# niche openings onto the corridor
for (nx0, ny0, nx1, ny1) in NICHES:
    if ny0 >= 27.0:   seg = ((nx0 + .3, ny0), (nx1 - .3, ny0))
    elif ny1 <= 9.0:  seg = ((nx0 + .3, ny1), (nx1 - .3, ny1))
    elif nx1 <= 11.0: seg = ((nx1, ny0 + .3), (nx1, ny1 - .3))
    else:             seg = ((nx0, ny0 + .3), (nx0, ny1 - .3))
    line(seg[0], seg[1], BG, WT + 2)
    line(seg[0], seg[1], FLOOR_C, WT - 3)

# suggested route
pts = " ".join(f"{sx(p[0]):.1f},{sy(p[1]):.1f}" for p in ROUTE)
add(f'<polyline points="{pts}" fill="none" stroke="{GOLD}" stroke-width="1.8" '
    f'stroke-dasharray="3 7" stroke-linecap="round" opacity="0.8"/>')
stub = " ".join(f"{sx(p[0]):.1f},{sy(p[1]):.1f}" for p in ROUTE_STUB)
add(f'<polyline points="{stub}" fill="none" stroke="{GOLD}" stroke-width="1.8" '
    f'stroke-dasharray="3 7" stroke-linecap="round" opacity="0.8"/>')

# entrance arrow
add(f'<defs><marker id="ar" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" '
    f'markerHeight="7" orient="auto"><path d="M2 1L8 5L2 9" fill="none" '
    f'stroke="{GOLD}" stroke-width="1.6" stroke-linecap="round"/></marker></defs>')
add(f'<line x1="{sx(1.2):.1f}" y1="{sy(19):.1f}" x2="{sx(3.4):.1f}" y2="{sy(19):.1f}" '
    f'stroke="{GOLD}" stroke-width="1.6" marker-end="url(#ar)"/>')

# labels
for r in ROOMS:
    rx0, ry0, rx1, ry1 = r["rect"]
    cx, cy = sx((rx0 + rx1) / 2), sy((ry0 + ry1) / 2)
    tag = r.get("tag")
    bh = 62 if tag else 46
    bw = max(len(r["name"]), len(r["owner"]), len(tag or "")) * 8.6 + 26
    add(f'<rect x="{cx-bw/2:.1f}" y="{cy-26:.1f}" width="{bw:.1f}" height="{bh}" '
        f'fill="{FLOOR_R}"/>')
    text(cx, cy - 4, r["name"], GOLD, 17, weight="400", ls="1.2")
    text(cx, cy + 16, r["owner"], MUTE, 12, fam="Helvetica, Arial, sans-serif")
    if tag:
        text(cx, cy + 32, tag.upper(), CREAM, 10, ls="2",
             fam="Helvetica, Arial, sans-serif", op=0.75)

text(sx(22), sy(18) + 86, "ATRIUM", CREAM, 15, ls="3.5",
     fam="Helvetica, Arial, sans-serif")
text(sx(22), sy(18) + 104, "the only daylight · you are given the lamp here",
     MUTE, 11.5, fam="Helvetica, Arial, sans-serif")
text(sx(7.5), sy(20.6), "ENTRY", CREAM, 12, ls="2.5",
     fam="Helvetica, Arial, sans-serif")
text(sx(22), sy(18) - 8, "oculus", MUTE, 11, fam="Helvetica, Arial, sans-serif")

for i, (nx0, ny0, nx1, ny1) in enumerate(NICHES):
    cx, cy = sx((nx0 + nx1) / 2), sy((ny0 + ny1) / 2)
    add(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="4.5" fill="{GOLD}" opacity="0.75"/>')

# corridor annotation
cl_y = sy(24.7) + 4
add(f'<rect x="{sx(22)-118:.1f}" y="{cl_y-12:.1f}" width="236" height="16" fill="{FLOOR_C}"/>')
text(sx(22), cl_y, "LOOP CORRIDOR · UNLIT · 3 M", MUTE, 11,
     ls="2", fam="Helvetica, Arial, sans-serif")

# north arrow
nxp, nyp = 905, 790
add(f'<circle cx="{nxp}" cy="{nyp}" r="26" fill="none" stroke="{MUTE}" '
    f'stroke-width="1" opacity="0.55"/>')
add(f'<path d="M{nxp} {nyp-20} L{nxp+7} {nyp+8} L{nxp} {nyp+2} L{nxp-7} {nyp+8} Z" '
    f'fill="{GOLD}" opacity="0.9"/>')
text(nxp, nyp + 42, "N", MUTE, 12, fam="Helvetica, Arial, sans-serif")

# scale bar
bx, by = 60, 850
add(f'<line x1="{bx}" y1="{by}" x2="{bx + 10*S}" y2="{by}" stroke="{MUTE}" stroke-width="1.4"/>')
for i in range(3):
    px = bx + i * 5 * S
    add(f'<line x1="{px}" y1="{by-5}" x2="{px}" y2="{by+5}" stroke="{MUTE}" stroke-width="1.4"/>')
add(f'<rect x="{bx}" y="{by-4}" width="{5*S}" height="8" fill="{MUTE}" opacity="0.55"/>')
text(bx, by + 24, "0", MUTE, 11, anchor="start", fam="Helvetica, Arial, sans-serif")
text(bx + 10*S, by + 24, "10 m", MUTE, 11, anchor="start", fam="Helvetica, Arial, sans-serif")

# --- right-hand panel --------------------------------------------------------
PX = 960
text(PX, 150, "LUMEN", GOLD, 46, anchor="start", ls="14")
text(PX, 182, "GALLERY FLOOR PLAN", MUTE, 13, anchor="start", ls="5",
     fam="Helvetica, Arial, sans-serif")
add(f'<line x1="{PX}" y1="206" x2="{W-60}" y2="206" stroke="{GOLD}" stroke-width="1" opacity="0.5"/>')

body = [
    ("A single storey.", "Five vaulted galleries open off the outer face of a loop "
     "corridor that returns you to where you began."),
    ("The corridor is unlit.", "Your lamp is the only light in it. Sculpture sits in "
     "four recessed niches, lit only as you pass."),
    ("The rooms light themselves.", "Crossing a threshold fades up that room's vault "
     "slot over two seconds, washing the ceiling. Step back out and it fades down."),
    ("Gallery Three is the far room.", "Largest, furthest from the entrance, and the "
     "one that carries the generated artwork."),
]
yy = 252
for head, para in body:
    text(PX, yy, head, CREAM, 16, anchor="start", weight="400")
    yy += 24
    words, lineacc = para.split(), ""
    for w_ in words:
        trial = (lineacc + " " + w_).strip()
        if len(trial) > 58:
            text(PX, yy, lineacc, MUTE, 13.5, anchor="start",
                 fam="Helvetica, Arial, sans-serif")
            yy += 20
            lineacc = w_
        else:
            lineacc = trial
    if lineacc:
        text(PX, yy, lineacc, MUTE, 13.5, anchor="start",
             fam="Helvetica, Arial, sans-serif")
        yy += 20
    yy += 18

# key
add(f'<line x1="{PX}" y1="{yy}" x2="{W-60}" y2="{yy}" stroke="{GOLD}" stroke-width="1" opacity="0.3"/>')
yy += 30
text(PX, yy, "KEY", MUTE, 11.5, anchor="start", ls="3.5", fam="Helvetica, Arial, sans-serif")
yy += 26

def key_row(draw, label):
    global yy
    draw(yy)
    text(PX + 54, yy + 5, label, CREAM, 13.5, anchor="start",
         fam="Helvetica, Arial, sans-serif")
    yy += 30

key_row(lambda y: add(f'<line x1="{PX}" y1="{y}" x2="{PX+38}" y2="{y}" stroke="{GOLD}" '
                      f'stroke-width="5" stroke-linecap="round" opacity="0.8"/>'),
        "Hang position for a framed work")
key_row(lambda y: add(f'<line x1="{PX}" y1="{y}" x2="{PX+38}" y2="{y}" stroke="{GOLD}" '
                      f'stroke-width="2.4" stroke-dasharray="9 7" opacity="0.5"/>'),
        "Vault crown — the light slot above")
key_row(lambda y: add(f'<line x1="{PX}" y1="{y}" x2="{PX+38}" y2="{y}" stroke="{GOLD}" '
                      f'stroke-width="1.6" stroke-dasharray="3 7" opacity="0.65"/>'),
        "Suggested route · the tour camera path")
key_row(lambda y: add(f'<circle cx="{PX+19}" cy="{y}" r="4.5" fill="{GOLD}" opacity="0.75"/>'),
        "Sculpture niche")

text(PX, H - 54, "Five rooms · one atrium · 41 × 36 m · 1 unit = 1 metre",
     MUTE, 11.5, anchor="start", ls="1", fam="Helvetica, Arial, sans-serif")
add('</svg>')

svg = "\n".join(P)
with open("2026-10-01-lumen-floor-plan.svg", "w") as f:
    f.write(svg)

# --- plan data ---------------------------------------------------------------
def seg_list():
    """Every wall segment, with doorways removed. Minimap + collision source."""
    out = []
    openings = [door_seg(r["rect"], r["door"]) for r in ROOMS]
    openings.append(door_seg(ATRIUM, ATRIUM_DOOR))
    openings.append(door_seg(VEST, VEST_DOOR))

    def subtract(seg):
        (ax, ay), (bx, by) = seg
        pieces = [seg]
        for (ox, oy), (px, py) in openings:
            nxt = []
            for (cx, cy), (dx, dy) in pieces:
                horiz = abs(cy - dy) < 1e-6
                if horiz and abs(oy - py) < 1e-6 and abs(oy - cy) < 1e-6:
                    lo, hi = sorted([ox, px]); c0, c1 = sorted([cx, dx])
                    if hi > c0 and lo < c1:
                        if c0 < lo: nxt.append(((c0, cy), (lo, cy)))
                        if hi < c1: nxt.append(((hi, cy), (c1, cy)))
                        continue
                if (not horiz) and abs(ox - px) < 1e-6 and abs(ox - cx) < 1e-6:
                    lo, hi = sorted([oy, py]); c0, c1 = sorted([cy, dy])
                    if hi > c0 and lo < c1:
                        if c0 < lo: nxt.append(((cx, c0), (cx, lo)))
                        if hi < c1: nxt.append(((cx, hi), (cx, c1)))
                        continue
                nxt.append(((cx, cy), (dx, dy)))
            pieces = nxt
        return pieces

    for rect in [RING_OUT, ATRIUM, VEST] + [r["rect"] for r in ROOMS]:
        for s in rect_walls(rect):
            out.extend(subtract(s))
    return out


plan = dict(
    units="metres", north="+y", engine_mapping="x->+X, y->-Z, up=+Y",
    wall_thickness=WALL_T, wall_height=4.2, vault_apex_height=6.0,
    bounds=dict(x0=4.0, y0=0.0, x1=45.0, y1=36.0),
    atrium=dict(rect=ATRIUM, door=ATRIUM_DOOR, oculus=dict(centre=[22.0, 18.0], radius=3.0)),
    vestibule=dict(rect=VEST, door=VEST_DOOR),
    corridor=dict(outer=RING_OUT, inner=ATRIUM, width=3.0, lit=False),
    niches=[dict(rect=n) for n in NICHES],
    rooms=[dict(id=r["id"], name=r["name"], owner=r["owner"], rect=list(r["rect"]),
                door=dict(side=r["door"][0], a=r["door"][1], b=r["door"][2]),
                vaults=[[list(p0), list(p1)] for p0, p1 in vault_lines(r)],
                hangs=[[list(p0), list(p1)] for p0, p1 in hang_marks(r)])
           for r in ROOMS],
    walls=[[list(p0), list(p1)] for p0, p1 in seg_list()],
    route=[list(p) for p in ROUTE],
)
with open("2026-10-01-lumen-plan.json", "w") as f:
    json.dump(plan, f, indent=1)

print(f"svg  {len(svg)} bytes")
print(f"walls {len(plan['walls'])} segments")
print(f"hangs {sum(len(r['hangs']) for r in plan['rooms'])} positions")
for r in ROOMS:
    x0, y0, x1, y1 = r["rect"]
    print(f"  {r['id']} {r['name']:<14} {x1-x0:>5.1f} x {y1-y0:>4.1f} m "
          f"= {(x1-x0)*(y1-y0):>6.1f} m2")
