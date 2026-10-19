"""Case for the Guition JC8048W550C (5" ESP32-S3 panel) with room for a
103665 LiPo (10 x 36 x 65 mm) and an Adafruit MAX17048 fuel gauge (#5580).

Two printed parts plus a quick fit test:
  shell : display window + side walls (print front face down, no supports)
  button: plunger for the battery button (print upright, flange down)
  lid   : flat back, M3 socket head screws in counterbores, battery cradle + gauge brackets,
          RESET/BOOT pinholes (print outer face down, no supports)
  test  : the first 14 mm of the shell, to check window, standoffs, USB-C and
          the battery button before the full print

Hardware: 4x ruthex RX-M3x5x4 heat-set inserts (hole 4.4 mm x 5.5 mm deep,
1.55 mm wall) and 4x M3 x 6 socket head cap screws (ISO 4762); or M3 x 10
self-tappers with INSERTS = False. The lid's pillars clamp the board, so these
are the only screws.

Run:  python case.py            -> out/*.stl and out/*.step
Coordinates: X/Y as seen from the BACK of the board (the side with the ESP32
module), Y up = the board edge with the BAT and speaker connectors. Z = 0 is
the front face of the case, Z grows towards the back.

Board feature positions are read off Guition's dimensioned drawing
(3-Structure_Diagram/Dimensions.png: 134 x 80 mm, M3 holes on corner ears
128 x 74 mm apart), to about +-1 mm. Print `test` first.
"""
from pathlib import Path

import cadquery as cq

# ---------------- parameters (mm) ----------------
BOARD_W, BOARD_H, PCB_T = 134.0, 80.0, 1.6
HOLES = [(3, 3), (131, 3), (3, 77), (131, 77)]  # board coords: from left/top, back view

STACK = 4.8        # LCD module (3.0) + touch glass, front of glass to PCB front: MEASURE
COMP = 6.0         # tallest part on the back of the PCB, plus margin
BATT = (65.0, 36.0, 10.0)  # battery L x W x T
BATT_GAP = 1.0     # between battery and the back-side components
GAUGE = (26.0, 18.0)       # fuel gauge board footprint

WALL = 2.0
FRONT_T = 2.0
LID_T = 2.4
SIDE_CLR = 0.5     # board ears to wall, left/right
MARGIN_TOP = 8.0   # room for the BAT plug + cable along the top edge
MARGIN_BOT = 8.0   # room for the P2-P5 plugs (fuel gauge cable) along the bottom edge
CORNER_R = 4.0
BED_CHAMFER = 0.5  # against elephant's foot on the edges that touch the print bed

WINDOW = (110.0, 70.0)  # active area is 108 x 64.8, ~1.9 mm off centre (FPC side unknown)
WINDOW_R = 1.5

INSERTS = True
INSERT_D, INSERT_DEPTH = 4.4, 5.5  # ruthex M3x5x4: hole 4.4, min. 5.0 deep, min. 1.3 wall
CBORE_D, CBORE_DEPTH = 6.0, 3.1   # M3 socket head: 5.5 x 3.0 mm
PAD = 1.6                         # lid pad under each screw (the bosses stop this much lower)

# Board features, board coords (from left / from top, back view), from Dimensions.png
USB_C_Y = 40.2       # USB-C centre, right edge of the board body
USB_C_FACE_X = 128.8  # front face of the receptacle (the body edge is at ~128.3; the ears reach 134)
BAT_BTN_X = 119.3    # battery button SW1 on the bottom edge, pressed from below (side-actuated)
RST_XY, BOOT_XY = (8.0, 55.1), (8.0, 61.3)  # top-actuated, pressed from the back
BATT_POS = (34.0, 5.0)   # battery top-left corner; ESP32 module is at x 6-32, y 24-42 (antenna x 6-13)
GAUGE_POS = (100.0, 45.0)  # gauge top-left corner: near P4 (x ~98, bottom edge)

# ---------------- derived ----------------
IN_W = BOARD_W + 2 * SIDE_CLR
IN_H = BOARD_H + MARGIN_TOP + MARGIN_BOT
OUT_W, OUT_H = IN_W + 2 * WALL, IN_H + 2 * WALL
PCB_FRONT = FRONT_T + STACK
PCB_BACK = PCB_FRONT + PCB_T
SHELL_H = PCB_BACK + COMP + BATT_GAP + BATT[2]
TOTAL = SHELL_H + LID_T
X0 = WALL + SIDE_CLR                  # board left edge (ears)
YBOT = WALL + MARGIN_BOT              # board bottom edge
YTOP = YBOT + BOARD_H                 # board top edge


def bxy(bx, by):
    """Board coords (from left, from top; back view) -> model X, Y."""
    return X0 + bx, YTOP - by


BOSS = [(WALL + 3.5, WALL + 3.5), (OUT_W - WALL - 3.5, WALL + 3.5),
        (WALL + 3.5, OUT_H - WALL - 3.5), (OUT_W - WALL - 3.5, OUT_H - WALL - 3.5)]
BOSS_D = 7.5


def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cyl(x, y, z0, z1, d):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(d / 2).extrude(z1 - z0)


def rounded_box(w, h, t, r, z0=0.0):
    return cq.Workplane("XY").workplane(offset=z0).center(w / 2, h / 2).rect(w, h).extrude(t).edges("|Z").fillet(r)


def usb_pocket(body):
    """The receptacle sits ~5.7 mm inside the ears' line, so a pocket from the
    outside reaches right up to its face: the plug's overmould can then seat
    fully (USB-C needs almost the whole 6.6 mm shell inserted). The pocket's
    solid surround starts at the front wall, so nothing overhangs."""
    _, yc = bxy(0, USB_C_Y)
    zc = PCB_BACK + 1.65
    face = X0 + USB_C_FACE_X + 0.3
    pw, ph = 13.5, 8.0                 # fits a 12.4 x 6.5 mm overmould
    body = body.union(box(face, OUT_W - WALL / 2, yc - pw / 2 - 1.6, yc + pw / 2 + 1.6, FRONT_T, zc + ph / 2 + 1.6))
    pocket = (cq.Workplane("YZ").workplane(offset=face).center(yc, zc)
              .rect(pw, ph).extrude(OUT_W - face + 1).edges("|X").fillet(2.5))
    return body.cut(pocket)


PLUNGER_D, PLUNGER_OUT = 3.0, 0.8      # shaft, and how far it stands proud of the wall


def button_guide(body):
    """Battery button SW1 is side-actuated, 8 mm inside the bottom wall. A
    separate printed plunger (out/button.stl) slides in a 3.4 mm hole through a
    solid guide block; its flange inside keeps it from falling out."""
    bx, _ = bxy(BAT_BTN_X, 0)
    zc = PCB_BACK + 1.0                # switch centre height
    yi = YBOT - 2.6                    # inner face of the guide block
    body = body.union(box(bx - 4.5, bx + 4.5, WALL / 2, yi, FRONT_T, zc + 3.5))
    hole = (cq.Workplane("XZ").workplane(offset=1).center(bx, zc)
            .circle((PLUNGER_D + 0.4) / 2).extrude(-(yi + 2)))   # XZ normal is -Y: negative extrude goes +Y
    return body.cut(hole)


def plunger():
    """Printed upright (flange down). Shaft from the outside (0.8 mm proud) to
    the guide block's inner face, flange, then a short tip 0.4 mm short of the
    switch."""
    yi = YBOT - 2.6
    shaft = yi + PLUNGER_OUT
    p = cq.Workplane("XY").circle(2.75).extrude(1.2)                       # flange
    p = p.union(cq.Workplane("XY").workplane(offset=1.2).circle(PLUNGER_D / 2).extrude(shaft))
    p = p.union(cq.Workplane("XY").workplane(offset=-1.0).circle(1.25).extrude(1.0))  # tip
    p = p.faces(">Z").edges().fillet(0.6)
    return p.translate((0, 0, 1.0))


def shell():
    body = rounded_box(OUT_W, OUT_H, SHELL_H, CORNER_R).faces("<Z").chamfer(BED_CHAMFER)
    body = body.cut(box(WALL, OUT_W - WALL, WALL, OUT_H - WALL, FRONT_T, SHELL_H + 1))

    # display window, chamfered on the outside (a 45 degree overhang when printed face down)
    cx, cy = bxy(BOARD_W / 2, BOARD_H / 2)
    win = (cq.Workplane("XY").workplane(offset=-1).center(cx, cy)
           .rect(*WINDOW).extrude(FRONT_T + 2).edges("|Z").fillet(WINDOW_R))
    body = body.cut(win)
    body = body.cut(cq.Workplane("XY").center(cx, cy).rect(WINDOW[0] + 2.4, WINDOW[1] + 2.4)
                    .workplane(offset=1.2).rect(*WINDOW).loft())

    # standoffs under the mounting holes (the board's front rests on them)
    for hx, hy in HOLES:
        body = body.union(cyl(*bxy(hx, hy), FRONT_T, PCB_FRONT, 5.5))

    # corner bosses for the lid screws
    for x, y in BOSS:
        top = SHELL_H - PAD
        body = body.union(cyl(x, y, FRONT_T, top, BOSS_D))
        if INSERTS:
            body = body.cut(cyl(x, y, top - INSERT_DEPTH, top + 1, INSERT_D))
            body = body.cut(cyl(x, y, top - INSERT_DEPTH - 4, top, 3.2))  # screw tip
        else:
            body = body.cut(cyl(x, y, top - 12, top + 1, 2.5))

    body = usb_pocket(body)
    body = button_guide(body)
    return body


def lid():
    z0 = SHELL_H
    plate = rounded_box(OUT_W, OUT_H, LID_T, CORNER_R, z0).faces(">Z").chamfer(BED_CHAMFER)

    # pads down onto the (lowered) bosses, M3 clearance holes, counterbores for the socket heads
    for x, y in BOSS:
        plate = plate.union(cyl(x, y, z0 - PAD, z0, BOSS_D))
        plate = plate.cut(cyl(x, y, z0 - PAD - 1, TOTAL + 1, 3.4))
        plate = plate.cut(cyl(x, y, TOTAL - CBORE_DEPTH, TOTAL + 1, CBORE_D))

    # RESET / BOOT pinholes (paperclip; the buttons face the back)
    for p in (RST_XY, BOOT_XY):
        plate = plate.cut(cyl(*bxy(*p), z0 - 1, TOTAL + 1, 2.2))

    # locating rim inside the walls, clear of the corner bosses and the two pockets
    rim = box(WALL + 0.3, OUT_W - WALL - 0.3, WALL + 0.3, OUT_H - WALL - 0.3, z0 - 1.8, z0)
    rim = rim.cut(box(WALL + 1.5, OUT_W - WALL - 1.5, WALL + 1.5, OUT_H - WALL - 1.5, z0 - 2, z0 + 1))
    for x, y in BOSS:
        rim = rim.cut(cyl(x, y, z0 - 2, z0 + 1, BOSS_D + 1.2))
    _, yc = bxy(0, USB_C_Y)
    rim = rim.cut(box(OUT_W - WALL - 3, OUT_W, yc - 10, yc + 10, z0 - 2, z0 + 1))
    plate = plate.union(rim)

    # pillars that clamp the board onto the front standoffs, with locating pins
    for hx, hy in HOLES:
        x, y = bxy(hx, hy)
        plate = plate.union(cyl(x, y, PCB_BACK, z0, 6.0))
        plate = plate.union(cyl(x, y, PCB_BACK - 1.4, PCB_BACK, 2.8))

    # battery cradle: low walls around the cell, cable exit on the gauge side
    L, W, _ = BATT
    x1, y1 = bxy(*BATT_POS)
    cx, cy = x1 + L / 2, y1 - W / 2
    cradle = (cq.Workplane("XY").workplane(offset=z0).center(cx, cy).rect(L + 3.2, W + 3.2).extrude(-4)
              .cut(cq.Workplane("XY").workplane(offset=z0 + 1).center(cx, cy).rect(L + 0.8, W + 0.8).extrude(-6)))
    cradle = cradle.cut(box(cx + L / 2 - 1, cx + L / 2 + 3, cy - 6, cy + 6, z0 - 5, z0 + 1))
    plate = plate.union(cradle)

    # fuel gauge: four L-shaped corner brackets (connector ends stay open)
    gw, gh = GAUGE
    gx, gy = bxy(*GAUGE_POS)  # top-left corner
    for cx_, sx in ((gx - 0.3, -1), (gx + gw + 0.3, 1)):
        for cy_, sy in ((gy + 0.3, 1), (gy - gh - 0.3, -1)):
            plate = plate.union(cq.Workplane("XY").workplane(offset=z0)
                                .center(cx_ + sx * 0.6, cy_ - sy * 2.0).rect(1.2, 5.2).extrude(-3))
            plate = plate.union(cq.Workplane("XY").workplane(offset=z0)
                                .center(cx_ - sx * 2.0, cy_ + sy * 0.6).rect(5.2, 1.2).extrude(-3))
    return plate


def main():
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)
    s, l = shell(), lid()
    test = s.intersect(box(-5, OUT_W + 5, -5, OUT_H + 5, 0, 14))
    # print orientation: shell + test front face down (as modelled), lid flipped outer face down
    lid_print = l.rotate((0, 0, 0), (1, 0, 0), 180).translate((0, OUT_H, TOTAL))
    for name, part in (("shell", s), ("lid", lid_print), ("test", test), ("button", plunger())):
        cq.exporters.export(part, str(out / f"{name}.stl"), tolerance=0.05, angularTolerance=0.1)
        cq.exporters.export(part, str(out / f"{name}.step"))
    cq.exporters.export(s.union(l), str(out / "assembly.step"))
    print(f"outer {OUT_W:.1f} x {OUT_H:.1f} x {TOTAL:.1f} mm; shell {SHELL_H:.1f} mm, lid {LID_T} mm")


if __name__ == "__main__":
    main()
