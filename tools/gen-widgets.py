r"""HUD Position Manager's default widget art: one SWF per widget, generated (swfgen.py) so the art is source-controlled and
rebuildable, written to dist\Interface\HUDPositionManager\widgets\. The clip contract (instance names the DLL drives) is
in swfgen.py's docstring and the README; a reskin replaces a file and keeps the names.

    python tools/gen-widgets.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swfgen as S

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(R, "dist", "Interface", "HUDPositionManager", "widgets")

# a plain style that sits beside the vanilla HUD: a dark translucent plate, a thin border, a flat fill. Colours are MUTED,
# in the range of the HUD's own bars: the HUD renders roughly twice as bright as the source colour (2026-10-04, Njordlinger:
# #2050E0 drew as (66,170,251)), so the first light blue #7EC8E3 and pale gold #E6D7A0 both drew pure white
PLATE = "#000000A0"
EDGE = "#7A7468C8"
FONT_ID = 1


def meter(name, fill_color, width=240.0, height=14.0, with_value=False):
    """Frame (plate + border), Fill (left-anchored bar inside), optional Value text to the right."""
    pad = 2.0
    # no ImportAssets2 of fonts_en.swf: Skyrim's Scaleform REFUSES a loaded child movie that carries one (2026-10-04:
    # the same meter loaded without it and was refused with it) - text uses a font placeholder instead (font_placeholder)
    tags = [S.font_placeholder(FONT_ID)] if with_value else []
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), S.rounded_rect(0, 0, width, height, 3.0))]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    # the fill's registration is its left edge: scaling _xscale empties it toward the left
    tags.append(S.shape3(20, [(fill_color, None, [(0, 0), (width - 2 * pad, 0), (width - 2 * pad, height - 2 * pad), (0, height - 2 * pad)])]))
    tags.append(S.sprite(21, [(1, 20, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.place(2, 21, "Fill", S.matrix(pad, pad)))
    if with_value:
        tags.append(S.edit_text(30, 80, height + 6, FONT_ID, height, "#FFFFFFFF", 0, ""))
        tags.append(S.place(3, 30, "Value", S.matrix(width + 6, -3)))
    return S.movie(width + (90 if with_value else 0), height, tags)


WIDGETS = {
    "breath.swf": lambda: meter("breath", "#3A7488FF"),        # muted teal blue: air left underwater
    "casting.swf": lambda: meter("casting", "#8A7440FF"),      # muted gold: a spell / bow / shout charging
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for fn, make in WIDGETS.items():
        data = make()
        open(os.path.join(OUT, fn), "wb").write(data)
        print(f"{fn}: {len(data)} bytes")


if __name__ == "__main__":
    main()
