"""Case for the Waveshare ESP32-S3-Touch-LCD-5 (800x480) with room for a
103665 LiPo (10 x 36 x 65 mm).

The cover glass is the front of the case: it sits flush in a pocket, its black
border resting on a ledge. Four M2.5 socket head screws go through the flat lid
and its pillars into the board's own SMT standoffs, pulling the glass onto the
ledge, so the shell is clamped between the glass and the lid. No inserts, no
other screws.

Parts:
  ws_shell  : print front face down, no supports
  ws_lid    : print outer face down, no supports
  ws_slider : battery ON/OFF switch extension, print flat (as exported)
  ws_test   : the first 15 mm of the shell (glass pocket, ledge, USB-C,
              switch slot, BOOT/RESET holes), to check the fit first

Hardware: 4x M2.5 x 14 socket head cap screws (ISO 4762 / DIN 912), heads in
counterbores flush with the back.

All positions come from Waveshare's own STEP model
(ESP32-S3-Touch-LCD-5_Drawing.zip, "ESP32-S3-Touch-LCD-5_asm.stp").
Model frame: Z = 0 is the glass front, Z grows towards the back; X = -x and
Y = y of the STEP model (a 180 degree turn about Y, so the model isn't mirrored).

Run:  python waveshare_case.py -> out/ws_*.stl and .step
"""
from pathlib import Path

import cadquery as cq

# ---------------- from the STEP model (converted to the model frame) ----------------
GLASS_W, GLASS_H, GLASS_R, GLASS_T = 127.7, 87.45, 4.0, 0.97
GLASS_CY = -1.475                       # glass centre Y (y -45.2 .. 42.25)
LCD_X, LCD_Y0, LCD_Y1 = 60.35, -39.79, 36.01   # LCD module outline (behind the glass)
PCB_BACK = 10.0                         # Z of the board's back face
STANDOFFS = [(-55.5, 29.5), (55.5, 29.5), (-55.5, -33.5), (55.5, -33.5)]  # M2.5 SMT standoffs
STANDOFF_END = 14.02                    # Z of their back end
TP_FPC = (-46.48, -31.48, -41.43)       # touch-panel ribbon: X range, lowest Y (wraps the bottom edge)
USB = dict(x_mouth=-61.27, y=-1.96, z=11.17, w=9.6, h=4.2)   # USB-C receptacle
BTNS = [(-21.94, 11.8), (-13.32, 11.8)]  # BOOT / RESET: side-pressed on the USB edge, (Y, Z)
LEVER = dict(x_tip=60.05, x_root=58.3, y=-17.34, z0=11.8, z1=13.3, travel=2.0)  # battery switch lever
BATT_CONN = (55.85, 59.65, -3.39, 5.21)  # battery socket (MX1.25 2P): X0, X1, Y0, Y1
KEEP_BEHIND = 15.0                      # nothing on the board's back reaches past this Z in the battery area

# ---------------- design parameters ----------------
BATT = (65.0, 36.0, 10.0)               # battery L x W x T
BATT_POS = (-20.0, 27.0)                # battery top-left corner (X, Y): clear of the ESP32 and SD card
GLASS_CLR = 0.25
LIP = 1.6                               # shell wall outside the glass
WALL = 2.0                              # side wall behind the ledge
LEDGE_T = 2.5                           # shelf under the glass border
LCD_CLR = 0.4
LID_T = 2.4
BED_CHAMFER = 0.5
PRELOAD = 0.2                           # pillars stop short of the standoffs so the screws pull the glass down
CBORE_D, CBORE_DEPTH = 5.0, 2.6         # M2.5 socket head: 4.5 x 2.5 mm
SCREW_L = 14                            # M2.5 x 14
PILLAR_D = 7.5                          # leaves 1.25 mm around the counterbore
SLIDER = dict(stem_w=5.0, stem_h=3.4, proud=0.6, plate_t=1.2, plate_w=9.0, plate_h=6.4,
              arm_w=4.6, arm_h=3.0, z0=11.0, notch=1.8, overlap=0.85)

# ---------------- derived ----------------
OUT_W = GLASS_W + 2 * (GLASS_CLR + LIP)
OUT_H = GLASS_H + 2 * (GLASS_CLR + LIP)
OUT_R = GLASS_R + GLASS_CLR + LIP
LID_IN = KEEP_BEHIND + BATT[2] + 0.4    # Z of the lid's inner face
TOTAL = LID_IN + LID_T
XW = OUT_W / 2 - WALL                   # inner face of the side walls (|X|)
Y_IN0, Y_IN1 = GLASS_CY - OUT_H / 2 + WALL, GLASS_CY + OUT_H / 2 - WALL


def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cyl(x, y, z0, z1, d):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(d / 2).extrude(z1 - z0)


def rrect(w, h, r, z0, z1, cy=0.0):
    return (cq.Workplane("XY").workplane(offset=z0).center(0, cy).rect(w, h)
            .extrude(z1 - z0).edges("|Z").fillet(r))


def shell():
    body = rrect(OUT_W, OUT_H, OUT_R, 0, LID_IN, GLASS_CY).faces("<Z").chamfer(BED_CHAMFER)
    # glass pocket (flush with the front)
    body = body.cut(rrect(GLASS_W + 2 * GLASS_CLR, GLASS_H + 2 * GLASS_CLR, GLASS_R + GLASS_CLR, -1, GLASS_T, GLASS_CY))
    # opening under the ledge: LCD module + clearance
    body = body.cut(box(-LCD_X - LCD_CLR, LCD_X + LCD_CLR, LCD_Y0 - LCD_CLR, LCD_Y1 + LCD_CLR, 0, LID_IN + 1))
    # behind the ledge: 2 mm walls
    body = body.cut(rrect(OUT_W - 2 * WALL, OUT_H - 2 * WALL, OUT_R - WALL, GLASS_T + LEDGE_T, LID_IN + 1, GLASS_CY))
    # notch in the ledge for the touch panel's ribbon cable
    x0, x1, ylow = TP_FPC
    body = body.cut(box(x0 - 1.0, x1 + 1.0, ylow - 0.8, LCD_Y0, GLASS_T, GLASS_T + LEDGE_T + 0.1))

    # USB-C: plug opening with a chamfered entry and a guide shroud up to 0.3 mm from the mouth
    pw, ph = 13.5, 8.0
    face = USB["x_mouth"] - 0.3
    shroud = box(-XW - 0.01, face, USB["y"] - pw / 2 - 1.6, USB["y"] + pw / 2 + 1.6,
                 GLASS_T + LEDGE_T, USB["z"] + ph / 2 + 1.6)
    body = body.union(shroud)
    hole = (cq.Workplane("YZ").workplane(offset=-OUT_W / 2 - 1).center(USB["y"], USB["z"])
            .rect(pw, ph).extrude(OUT_W / 2 + 1 + face).edges("|X").fillet(2.5))
    body = body.cut(hole)
    funnel = (cq.Workplane("YZ").workplane(offset=-OUT_W / 2 - 0.01).center(USB["y"], USB["z"])
              .rect(pw + 2.4, ph + 2.4).workplane(offset=1.2).rect(pw, ph).loft())
    body = body.cut(funnel)

    # BOOT / RESET paperclip holes, USB side
    for y, z in BTNS:
        body = body.cut(cq.Workplane("YZ").workplane(offset=-OUT_W / 2 - 1).center(y, z).circle(1.0).extrude(WALL + 2))

    # battery switch: slot for the slider's stem
    s = SLIDER
    lv = LEVER
    y_lo = lv["y"] - s["stem_w"] / 2 - 0.25
    y_hi = lv["y"] + lv["travel"] + s["stem_w"] / 2 + 0.25
    body = body.cut(box(XW - 0.5, OUT_W / 2 + 1, y_lo, y_hi, s["z0"] - 0.2, s["z0"] + s["stem_h"] + 0.2))
    return body


def slider():
    """Battery switch extension in the model frame, at the switch's OFF/low position
    (lever at LEVER['y']). Every feature starts at Z = z0, so it prints flat."""
    s, lv = SLIDER, LEVER
    yc = lv["y"]
    z0 = s["z0"]
    x_wall_in, x_out = XW, OUT_W / 2
    stem = box(x_wall_in, x_out + s["proud"], yc - s["stem_w"] / 2, yc + s["stem_w"] / 2, z0, z0 + s["stem_h"])
    plate = box(x_wall_in - s["plate_t"], x_wall_in, yc - s["plate_w"] / 2, yc + s["plate_w"] / 2, z0, z0 + s["plate_h"])
    arm_tip = lv["x_tip"] - s["overlap"]
    arm = box(arm_tip, x_wall_in - s["plate_t"], yc - s["arm_w"] / 2, yc + s["arm_w"] / 2, z0, z0 + s["arm_h"])
    notch = box(arm_tip - 0.1, lv["x_tip"] + 0.35, yc - s["notch"] / 2, yc + s["notch"] / 2, z0 - 1, z0 + s["arm_h"] + 1)
    p = stem.union(plate).union(arm).cut(notch)
    # grip: three grooves across the knob face
    for dy in (-1.5, 0, 1.5):
        p = p.cut(box(x_out + s["proud"] - 0.35, x_out + s["proud"] + 1, yc + dy - 0.3, yc + dy + 0.3, z0 - 1, z0 + s["stem_h"] + 1))
    return p


def lid():
    z0 = LID_IN
    plate = rrect(OUT_W, OUT_H, OUT_R, z0, TOTAL, GLASS_CY).faces(">Z").chamfer(BED_CHAMFER)
    # pillars down to the standoffs, M2.5 clearance holes, counterbores for the socket heads
    for x, y in STANDOFFS:
        plate = plate.union(cyl(x, y, STANDOFF_END + PRELOAD, z0, PILLAR_D))
        plate = plate.cut(cyl(x, y, STANDOFF_END, TOTAL + 1, 2.8))
        plate = plate.cut(cyl(x, y, TOTAL - CBORE_DEPTH, TOTAL + 1, CBORE_D))
    # locating rim inside the walls
    rim = rrect(OUT_W - 2 * WALL - 0.6, OUT_H - 2 * WALL - 0.6, OUT_R - WALL - 0.3, z0 - 1.8, z0, GLASS_CY)
    rim = rim.cut(rrect(OUT_W - 2 * WALL - 3.0, OUT_H - 2 * WALL - 3.0, OUT_R - WALL - 1.5, z0 - 2, z0 + 1, GLASS_CY))
    # clear the USB shroud and the switch slider's plate
    rim = rim.cut(box(-OUT_W / 2, -XW + 3, USB["y"] - 10, USB["y"] + 10, z0 - 2, z0 + 1))
    rim = rim.cut(box(XW - 3, OUT_W / 2, LEVER["y"] - 8, LEVER["y"] + 10, z0 - 2, z0 + 1))
    plate = plate.union(rim)
    # battery cradle, cable exit towards the battery socket (+X end)
    L, W, _ = BATT
    bx, by = BATT_POS
    cx, cy = bx + L / 2, by - W / 2
    cradle = (cq.Workplane("XY").workplane(offset=z0).center(cx, cy).rect(L + 3.2, W + 3.2).extrude(-4)
              .cut(cq.Workplane("XY").workplane(offset=z0 + 1).center(cx, cy).rect(L + 0.8, W + 0.8).extrude(-6)))
    cradle = cradle.cut(box(cx + L / 2 - 1, cx + L / 2 + 3, cy - 6, cy + 6, z0 - 5, z0 + 1))
    return plate.union(cradle)


def main():
    out = Path(__file__).parent / "out"
    out.mkdir(exist_ok=True)
    s, l, sl = shell(), lid(), slider()
    test = s.intersect(box(-OUT_W, OUT_W, -OUT_H, OUT_H, 0, 15.0))
    lid_print = l.rotate((0, 0, 0), (1, 0, 0), 180).translate((0, 0, TOTAL))
    slider_print = sl.translate((0, 0, -SLIDER["z0"]))
    parts = (("ws_shell", s), ("ws_lid", lid_print), ("ws_slider", slider_print), ("ws_test", test))
    for name, part in parts:
        cq.exporters.export(part, str(out / f"{name}.stl"), tolerance=0.03, angularTolerance=0.1)
        cq.exporters.export(part, str(out / f"{name}.step"))
    cq.exporters.export(s.union(l).union(sl), str(out / "ws_assembly.step"))
    screw_tip = TOTAL - CBORE_DEPTH - SCREW_L
    print(f"outer {OUT_W:.1f} x {OUT_H:.1f} x {TOTAL:.1f} mm; M2.5x{SCREW_L} engages "
          f"{STANDOFF_END - screw_tip:.1f} mm of the standoff, tip {screw_tip - PCB_BACK:.1f} mm short of the PCB")


if __name__ == "__main__":
    main()
