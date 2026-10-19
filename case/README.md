# Case

A two-part case for the Guition JC8048W550C with room for the battery
(103665, 10 x 36 x 65 mm) and the Adafruit MAX17048 fuel gauge (#5580).
The back is flat; four socket head screws sit flush in counterbores.

| Part | File | Print |
|---|---|---|
| Front shell | `out/shell.stl` | front face down, no supports |
| Back lid | `out/lid.stl` | outer face down, no supports |
| Button plunger | `out/button.stl` | upright, flange down |
| Fit test | `out/test.stl` | the first 14 mm of the shell, ~1 h |

Outside size: 139 x 100 x 27.8 mm. PETG or PLA, 0.2 mm layers, 3 walls.

## Design notes

- Board positions are read off Guition's dimensioned drawing
  (`3-Structure_Diagram/Dimensions.png` in the official package), to about 1 mm.
- The board's mounting holes sit on corner ears, so the USB-C receptacle is
  ~5.7 mm inside the case wall. A pocket reaches right up to it so a plug can
  seat fully.
- The battery button presses sideways at the board's bottom edge, 8 mm inside
  the wall. A separate plunger slides through a guide block and stands 0.8 mm
  proud of the case; its flange keeps it from falling out.
- RESET and BOOT face the back: paperclip holes through the lid.
- Every overhang is 45 degrees or a short bridge, and the bed edges have a
  0.5 mm chamfer against elephant's foot.

## Before the full print

Print `test.stl` and drop the board in:

1. The display shows fully inside the window.
2. The board's four holes sit on the four standoffs.
3. A USB-C plug seats fully through the side pocket.
4. The plunger hole lines up with the battery button.

If something is off, change the value at the top of `case.py` and regenerate.
The one value not on the drawing is `STACK` (front of the glass to the front
of the PCB, default 4.8 mm): measure it if you can.

```
uv venv .venv && VIRTUAL_ENV=.venv uv pip install cadquery
.venv/bin/python case.py
```

## Hardware and assembly

- 4x ruthex RX-M3x5x4 heat-set inserts and 4x M3 x 6 socket head cap screws
  screws. Those are the only screws: the lid pillars clamp the board. Without
  inserts set `INSERTS = False` and use M3 x 10 self-tappers.

1. Press the inserts into the four corner bosses of the shell.
2. Push the plunger into its hole from the inside, flange inwards.
3. Lay the display face down into the shell, its holes on the standoffs.
4. Put the battery in the lid's cradle and the gauge between its corner brackets
   (a dab of double-sided tape keeps both in place).
5. Wire battery -> gauge -> board BAT, and the gauge's STEMMA QT cable to P4.
6. Close the lid; its pillars clamp the board. Screw it down.

# Waveshare ESP32-S3-Touch-LCD-5 case

`waveshare_case.py`, built from Waveshare's own STEP model (their
`ESP32-S3-Touch-LCD-5_Drawing.zip`; not included here). The cover glass is the
front: it sits flush in a pocket on a 2.5 mm ledge.

| Part | File | Print |
|---|---|---|
| Shell | `out/ws_shell.stl` | front face down, no supports |
| Lid | `out/ws_lid.stl` | outer face down, no supports |
| Switch slider | `out/ws_slider.stl` | flat, as exported |
| Fit test | `out/ws_test.stl` | the first 15 mm of the shell |

Outside size: 131.4 x 91.2 x 27.8 mm.

- **Screws:** 4x M2.5 x 14 socket head cap screws through the lid into the board's own
  M2.5 standoffs (2.8 mm of thread; heads flush in counterbores). They pull the glass onto the ledge,
  which clamps the shell between glass and lid. No inserts, no other screws.
  Tighten gently.
- **USB-C:** a guided pocket with a chamfered entry ends 0.3 mm from the port,
  so the plug seats fully.
- **Battery switch:** the slider's knob stands 0.6 mm proud of the wall; inside,
  its fork holds the switch lever (2 mm travel). Put it in from the inside
  before the board; the lever drops into the fork as the board goes in.
- **BOOT / RESET:** paperclip holes next to the USB-C pocket.
- **No fuel-gauge mount:** the MAX17048's I2C address clashes with the board's
  CH422G expander, so the firmware doesn't use a gauge on this board.

Assembly: slider in from the inside, display face down into the shell (ribbon
cable over the notch in the ledge), battery into the lid's cradle, plug it into
the board's MX1.25 socket, lid on, four screws.
