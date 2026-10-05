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


def circle(cx, cy, r, n=20):
    import math
    return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]


def bar(cx, cy, length, thick, deg):
    """A thin rectangle centred on cx,cy, turned deg degrees."""
    import math
    a = math.radians(deg)
    dx, dy = math.cos(a) * length / 2, math.sin(a) * length / 2
    nx, ny = -math.sin(a) * thick / 2, math.cos(a) * thick / 2
    return [(cx - dx + nx, cy - dy + ny), (cx + dx + nx, cy + dy + ny), (cx + dx - nx, cy + dy - ny), (cx - dx - nx, cy - dy - ny)]


def survival_icon(kind, colour, s=12.0):
    """Polygons for a need's icon in an s x s box centred on 0,0."""
    import math
    h = s / 2
    if kind == "hunger":   # a drumstick: the meat, its bone, the bone's knob
        return [(colour, None, circle(-h * 0.25, -h * 0.25, h * 0.6)), (colour, None, bar(h * 0.35, h * 0.35, h * 0.9, h * 0.3, 45)),
                (colour, None, circle(h * 0.75, h * 0.75, h * 0.22, 10))]
    if kind == "fatigue":  # a crescent moon: an outer arc and an inner one back
        outer = [(h * math.cos(math.radians(a)), h * math.sin(math.radians(a))) for a in range(60, 301, 15)]
        inner = [(h * 0.35 + h * 0.75 * math.cos(math.radians(a)), h * 0.75 * math.sin(math.radians(a))) for a in range(290, 69, -15)]
        return [(colour, None, outer + inner)]
    # cold: a snowflake - three bars crossing
    return [(colour, None, bar(0, 0, s, s * 0.14, d)) for d in (0, 60, 120)]


def meter(name, fill_color, width=240.0, height=14.0, with_value=False, icon=None):
    """Frame (plate + border), Fill (left-anchored bar inside), optional Value text to the right, optional Icon (a
    survival need's symbol) to the left."""
    pad = 2.0
    # the font is imported from gfxfontlib.swf as the game's HUD does (an import from fonts_en.swf got the movie refused)
    tags = [S.import_font(FONT_ID)] if with_value else []
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), S.rounded_rect(0, 0, width, height, 3.0))]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    # the fill's registration is its left edge: scaling _xscale empties it toward the left
    tags.append(S.shape3(20, [(fill_color, None, [(0, 0), (width - 2 * pad, 0), (width - 2 * pad, height - 2 * pad), (0, height - 2 * pad)])]))
    tags.append(S.sprite(21, [(1, 20, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.place(2, 21, "Fill", S.matrix(pad, pad)))
    if icon:
        # one DefineShape per polygon: polygons that OVERLAP inside one shape (the drumstick's bone over its meat, the
        # snowflake's crossing bars) are suspected of the Scaleform crash during the HUD's advance on 2026-10-04
        # (crash-2026-10-04-22-17-54, SkyrimSE+0FFD2A4 under HUDMenu::AdvanceMovie) - every other shape we ship is
        # made of polygons that do not overlap
        size = max(height + 4, 12.0)
        polys = survival_icon(icon, fill_color, size)
        for i, poly in enumerate(polys):
            tags.append(S.shape3(50 + i, [poly]))
        tags.append(S.sprite(41, [(i + 1, 50 + i, None, None) for i in range(len(polys))]))
        tags.append(S.place(4, 41, "Icon", S.matrix(-size / 2 - 4, height / 2)))
    if with_value:
        tags.append(S.edit_text(30, 80, height + 8, FONT_ID, height + 2, "#E6E1D2FF", 0))
        tags.append(S.place(3, 30, "Value", S.matrix(width + 6, -4)))
    return S.movie(width + (90 if with_value else 0), height, tags)


def text_widget(icon_color, width=120.0, height=22.0):
    """Frame (plate + border), Icon (a small diamond in the widget's colour), Value text to its right."""
    tags = [S.import_font(FONT_ID)]
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), S.rounded_rect(0, 0, width, height, 3.0))]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    r = height / 2 - 5
    tags.append(S.shape3(40, [(icon_color, None, [(0, -r), (r, 0), (0, r), (-r, 0)])]))
    tags.append(S.sprite(41, [(1, 40, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.place(2, 41, "Icon", S.matrix(height / 2, height / 2)))
    tags.append(S.edit_text(30, width - height - 4, height, FONT_ID, height - 6, "#E6E1D2FF", 0))
    tags.append(S.place(3, 30, "Value", S.matrix(height, 1)))
    return S.movie(width, height, tags)


def badge(ring_color, radius=26.0, thickness=4.0, segments=36):
    """Level as a badge: Frame (a round plate, a faint full ring track), Ring (Seg0..SegN-1, each one arc of the ring,
    shown by HPM up to the XP progress, starting at the top and running clockwise), Value (the level, centred)."""
    import math
    tags = [S.import_font(FONT_ID)]
    size = 2 * (radius + 2)
    c = size / 2
    plate = [(c + (radius - thickness - 1) * math.cos(2 * math.pi * i / 48), c + (radius - thickness - 1) * math.sin(2 * math.pi * i / 48)) for i in range(48)]
    track = [(x + c, y + c) for x, y in S.arc(radius, thickness, 359.0, 72)]
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), plate), ("#FFFFFF24", None, track)]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    span = 360.0 / segments
    arc = S.arc(radius, thickness, span * 0.86, 4)   # one segment, centred on "up", a small gap to the next
    tags.append(S.shape3(20, [(ring_color, None, arc)]))
    tags.append(S.sprite(21, [(1, 20, None, None)]))
    ring_children = [(i + 1, 21, f"Seg{i}", S.matrix(0, 0, rot=i * span)) for i in range(segments)]
    tags.append(S.sprite(22, ring_children))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.place(2, 22, "Ring", S.matrix(c, c)))
    fh = radius * 0.9
    tags.append(S.edit_text(30, size, fh + 6, FONT_ID, fh, "#E6E1D2FF", 2))
    tags.append(S.place(3, 30, "Value", S.matrix(0, c - fh / 2 - 4)))
    return S.movie(size, size, tags)


def grid_widget(icons, cols=1, cell_w=120.0, row_h=20.0, plate=True, outline=False):
    """A multi-line widget: Frame (one plate round the grid), then one cell per entry of icons - a small diamond in that
    colour (None: no icon) and a text field. The fields are Value, Value2, Value3 ... in reading order (left to right,
    then down), the order HPM writes them in. No words in the art: what a row is reads from its icon colour, so the art
    needs no translating."""
    tags = [S.import_font(FONT_ID)]
    rows = (len(icons) + cols - 1) // cols
    width, height = cols * cell_w + 4, rows * row_h + 4
    # plate=False: the Frame is still there (it sizes and centres the widget) but draws nothing - text on the scene alone
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), S.rounded_rect(0, 0, width, height, 3.0))] if plate else
                         [("#00000000", None, S.rounded_rect(0, 0, width, height, 3.0))]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.edit_text(30, cell_w - (row_h if any(icons) else 4), row_h, FONT_ID, row_h - 6, "#E6E1D2FF", 0))
    depth, cid = 2, 40
    r = row_h / 2 - 5
    for i, colour in enumerate(icons):
        x, y = 2 + (i % cols) * cell_w, 2 + (i // cols) * row_h
        if colour:
            tags.append(S.shape3(cid, [(colour, None, [(0, -r), (r, 0), (0, r), (-r, 0)])]))
            tags.append(S.sprite(cid + 1, [(1, cid, None, None)]))
            tags.append(S.place(depth, cid + 1, f"Icon{i + 1}" if i else "Icon", S.matrix(x + row_h / 2, y + row_h / 2)))
            depth, cid = depth + 1, cid + 2
        name, mat = "Value" if i == 0 else f"Value{i + 1}", S.matrix(x + (row_h if colour else 4), y + 1)
        # outline=True: a dark Glow round each text (no plate) - text alone washed out over a bright sky or snow (2026-10-04)
        tags.append(S.place_glow(depth, 30, name, mat) if outline else S.place(depth, 30, name, mat))
        depth += 1
    return S.movie(width, height, tags)


def equip_cross(colours, half=18.0, text_w=130.0, size=13.0):
    """Equipped items as the Tween menu's cross (the shape of Norden UI's STB equip skin): four diamonds round a centre,
    each with the item's name beside it, facing out. Frame is the four plates (the widget centres on the cross); Icon..
    Icon4 are the small markers inside them; Value (right hand, right), Value2 (left hand, left), Value3 (shout or power,
    top), Value4 (ammo, bottom)."""
    tags = [S.import_font(FONT_ID)]
    gap = half + 2.0                     # centre to each diamond's centre: the diamonds meet at their corners
    slots = [(gap, 0.0), (-gap, 0.0), (0.0, -gap), (0.0, gap)]   # right, left, top, bottom
    plates = []
    for x, y in slots:
        x, y = x + 2 * gap, y + 2 * gap   # the art's origin is its top-left: the cross sits in a 4*gap square
        plates.append((PLATE, (1.0, EDGE), [(x, y - half), (x + half, y), (x, y + half), (x - half, y)]))
    tags.append(S.shape3(10, plates))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    r = half * 0.35
    th = size + 8
    tags.append(S.edit_text(30, text_w, th, FONT_ID, size, "#E6E1D2FF", 0))   # reads outward to the right
    tags.append(S.edit_text(31, text_w, th, FONT_ID, size, "#E6E1D2FF", 1))   # to the left
    tags.append(S.edit_text(32, text_w, th, FONT_ID, size, "#E6E1D2FF", 2))   # above / below, centred
    depth = 2
    for i, ((x, y), colour) in enumerate(zip(slots, colours)):
        cx, cy = x + 2 * gap, y + 2 * gap
        tags.append(S.shape3(40 + 2 * i, [(colour, None, [(0, -r), (r, 0), (0, r), (-r, 0)])]))
        tags.append(S.sprite(41 + 2 * i, [(1, 40 + 2 * i, None, None)]))
        tags.append(S.place(depth, 41 + 2 * i, "Icon" if i == 0 else f"Icon{i + 1}", S.matrix(cx, cy)))
        name = "Value" if i == 0 else f"Value{i + 1}"
        if i == 0:
            tags.append(S.place(depth + 1, 30, name, S.matrix(cx + half + 4, cy - th / 2 + 2)))
        elif i == 1:
            tags.append(S.place(depth + 1, 31, name, S.matrix(cx - half - 4 - text_w, cy - th / 2 + 2)))
        elif i == 2:
            tags.append(S.place(depth + 1, 32, name, S.matrix(cx - text_w / 2, cy - half - th)))
        else:
            tags.append(S.place(depth + 1, 32, name, S.matrix(cx - text_w / 2, cy + half + 2)))
        depth += 2
    # the potions carried - health, magicka, stamina: a small marker in each colour and the count, in a row under the cross
    tags.append(S.edit_text(33, 40.0, th, FONT_ID, size - 1, "#E6E1D2FF", 0))
    for i, colour in enumerate(("#8A3A32FF", "#3A5A8AFF", "#5A7A46FF")):
        x, y = 2 * gap - 60 + i * 44, 4 * gap + th + 6
        tags.append(S.shape3(60 + 2 * i, [(colour, None, [(0, -r), (r, 0), (0, r), (-r, 0)])]))
        tags.append(S.sprite(61 + 2 * i, [(1, 60 + 2 * i, None, None)]))
        tags.append(S.place(depth, 61 + 2 * i, f"Icon{5 + i}", S.matrix(x, y)))
        tags.append(S.place(depth + 1, 33, f"Value{5 + i}", S.matrix(x + r + 3, y - th / 2 + 2)))
        depth += 2
    return S.movie(4 * gap, 4 * gap, tags)


def playerbar(fill_color, phantom_color, width=260.0, height=12.0):
    """HPM's own player bar (phase 4, TrueHUD's player widget): Frame, Phantom (the recent loss, behind the fill, left
    registered), Fill, Penalty (Survival's reduction - registered on its RIGHT edge, so it grows from the bar's end), and
    a centred Value for the numbers."""
    pad = 2.0
    iw, ih = width - 2 * pad, height - 2 * pad
    tags = [S.import_font(FONT_ID)]
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), S.rounded_rect(0, 0, width, height, 3.0))]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    tags.append(S.shape3(20, [(phantom_color, None, [(0, 0), (iw, 0), (iw, ih), (0, ih)])]))
    tags.append(S.sprite(21, [(1, 20, None, None)]))
    tags.append(S.shape3(22, [(fill_color, None, [(0, 0), (iw, 0), (iw, ih), (0, ih)])]))
    tags.append(S.sprite(23, [(1, 22, None, None)]))
    tags.append(S.shape3(24, [("#2A2624E0", None, [(-iw, 0), (0, 0), (0, ih), (-iw, ih)])]))   # a dark band, drawn leftward
    tags.append(S.sprite(25, [(1, 24, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.place(2, 21, "Phantom", S.matrix(pad, pad)))
    tags.append(S.place(3, 23, "Fill", S.matrix(pad, pad)))
    tags.append(S.place(4, 25, "Penalty", S.matrix(pad + iw, pad, sx=0.001, sy=1.0)))   # no width until HPM's first read
    tags.append(S.edit_text(30, width, height + 6, FONT_ID, height, "#E6E1D2FF", 2))
    tags.append(S.place(5, 30, "Value", S.matrix(0, -3)))
    return S.movie(width, height, tags)


def infobar(fill_color, phantom_color, width=80.0, height=6.0, magicka_color="#2A4A8AFF", stamina_color="#3A6A32FF"):
    """A bar over a character (phase 4, TrueHUD's info bar): the art CENTRED on its origin (the holder sits on the
    projected head), Frame, Phantom and Fill left registered at the bar's left edge, Value the name above, Value2 the
    level at the bar's left. Under it (B2, TrueHUD's resource bars) two thin bars, each half the width: Frame2 / Fill2
    magicka on the left, registered at its left edge, and Frame3 / Fill3 stamina on the right, registered at its RIGHT
    edge so it empties toward the bar's end - HPM shows and hides them per [InfoBars] uResources*. Value3 (B3) is the
    damage counter at the bar's right end."""
    pad = 1.5
    iw, ih = width - 2 * pad, height - 2 * pad
    x0 = -width / 2
    gap, sh, sp = 1.0, 3.5, 1.0            # gap under the health bar, a sub-bar's height, its inner padding
    hw = (width - gap) / 2                  # each sub-bar's width
    sy = height + gap
    siw, sih = hw - 2 * sp, sh - 2 * sp
    tags = [S.import_font(FONT_ID)]
    tags.append(S.shape3(40, [(PLATE, (1.0, EDGE), S.rounded_rect(0, 0, hw, sh, 1.0))]))
    tags.append(S.sprite(41, [(1, 40, None, None)]))
    tags.append(S.shape3(42, [(magicka_color, None, [(0, 0), (siw, 0), (siw, sih), (0, sih)])]))
    tags.append(S.sprite(43, [(1, 42, None, None)]))
    tags.append(S.shape3(44, [(stamina_color, None, [(-siw, 0), (0, 0), (0, sih), (-siw, sih)])]))   # drawn leftward
    tags.append(S.sprite(45, [(1, 44, None, None)]))
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), S.rounded_rect(x0, 0, width, height, 2.0))]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    tags.append(S.shape3(20, [(phantom_color, None, [(0, 0), (iw, 0), (iw, ih), (0, ih)])]))
    tags.append(S.sprite(21, [(1, 20, None, None)]))
    tags.append(S.shape3(22, [(fill_color, None, [(0, 0), (iw, 0), (iw, ih), (0, ih)])]))
    tags.append(S.sprite(23, [(1, 22, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.place(2, 21, "Phantom", S.matrix(x0 + pad, pad)))
    tags.append(S.place(3, 23, "Fill", S.matrix(x0 + pad, pad)))
    tags.append(S.edit_text(30, 200.0, 16.0, FONT_ID, 11, "#E6E1D2FF", 2))
    tags.append(S.place(4, 30, "Value", S.matrix(-100.0, -15.0)))
    tags.append(S.edit_text(31, 30.0, 14.0, FONT_ID, 10, "#C8C0B0FF", 1))
    tags.append(S.place(5, 31, "Value2", S.matrix(x0 - 33.0, -4.0)))
    tags.append(S.place(6, 41, "Frame2", S.matrix(x0, sy)))
    tags.append(S.place(7, 43, "Fill2", S.matrix(x0 + sp, sy + sp)))
    tags.append(S.place(8, 41, "Frame3", S.matrix(x0 + hw + gap, sy)))
    tags.append(S.place(9, 45, "Fill3", S.matrix(-x0 - sp, sy + sp)))
    # B3: the damage counter - the health lost in the last seconds, at the bar's right end
    tags.append(S.edit_text(32, 40.0, 14.0, FONT_ID, 10, "#E8C070FF", 0))
    tags.append(S.place(10, 32, "Value3", S.matrix(-x0 + 3.0, -4.0)))
    return S.movie(width, height, tags)


def bossbar(fill_color, phantom_color, width=420.0, height=12.0):
    """The boss bar (phase 4, TrueHUD's): playerbar's parts - Frame, Phantom, Fill - with the boss's name (Value) centred
    above it and its level (Value2) at its left."""
    pad = 2.0
    iw, ih = width - 2 * pad, height - 2 * pad
    tags = [S.import_font(FONT_ID)]
    tags.append(S.shape3(10, [(PLATE, (1.0, EDGE), S.rounded_rect(0, 0, width, height, 3.0))]))
    tags.append(S.sprite(11, [(1, 10, None, None)]))
    tags.append(S.shape3(20, [(phantom_color, None, [(0, 0), (iw, 0), (iw, ih), (0, ih)])]))
    tags.append(S.sprite(21, [(1, 20, None, None)]))
    tags.append(S.shape3(22, [(fill_color, None, [(0, 0), (iw, 0), (iw, ih), (0, ih)])]))
    tags.append(S.sprite(23, [(1, 22, None, None)]))
    tags.append(S.place(1, 11, "Frame", S.matrix(0, 0)))
    tags.append(S.place(2, 21, "Phantom", S.matrix(pad, pad)))
    tags.append(S.place(3, 23, "Fill", S.matrix(pad, pad)))
    tags.append(S.edit_text(30, width, 20.0, FONT_ID, 15, "#E6E1D2FF", 2))
    tags.append(S.place(4, 30, "Value", S.matrix(0, -21.0)))
    tags.append(S.edit_text(31, 40.0, 16.0, FONT_ID, 12, "#C8C0B0FF", 1))
    tags.append(S.place(5, 31, "Value2", S.matrix(-44.0, -2.0)))
    # B4b: a second and a third boss - Boss2 / Boss3, each a sprite with the same Frame / Fill / Value / Value2, placed
    # by the plugin under (or over) the first by [BossBars] fSpacing; hidden while there is no such boss
    tags.append(S.sprite(50, [(1, 11, "Frame", S.matrix(0, 0)), (2, 23, "Fill", S.matrix(pad, pad)),
                              (3, 30, "Value", S.matrix(0, -21.0)), (4, 31, "Value2", S.matrix(-44.0, -2.0))]))
    tags.append(S.place(6, 50, "Boss2", S.matrix(0, 50.0)))
    tags.append(S.place(7, 50, "Boss3", S.matrix(0, 100.0)))
    return S.movie(width, height, tags)


def floattext(width=220.0, height=30.0, size=18, colour="#E6E1D2FF"):
    """Floating text (phase 4 build 4, TrueHUD's floating text): one Value field CENTRED on the origin (the holder sits on
    the projected point over the character), no plate - an outline (a Glow filter) keeps it readable on a bright scene."""
    tags = [S.import_font(FONT_ID)]
    tags.append(S.edit_text(30, width, height, FONT_ID, size, colour, 2))
    tags.append(S.place_glow(1, 30, "Value", S.matrix(-width / 2, -height / 2)))
    return S.movie(width, height, tags)


WIDGETS = {
    "floattext.swf": lambda: floattext(),
    # recent loot (phase 4 build 4): up to six rows, newest first, no icons
    # no plate (the owner, 2026-10-04: "You dont need the outline box for the recent loot")
    # ... and an outline on the text instead: over a bright sky the plain rows washed out (2026-10-04, HPM Minimal, Riverwood)
    "loot.swf": lambda: grid_widget([None] * 6, cols=1, cell_w=220.0, row_h=18.0, plate=False, outline=True),
    "bossbar.swf": lambda: bossbar("#8A2A26FF", "#C8A08CB0"),
    # the bars over characters (phase 4 build 2): one art for all of them, red health with a pale loss behind it
    "infobar.swf": lambda: infobar("#8A2A26FF", "#C8A08CB0"),
    # HPM's own player bars (phase 4 build 1): the game's colours, muted; the phantom a lighter shade of each
    "playerhealth.swf": lambda: playerbar("#8A2A26FF", "#C8A08CB0"),
    "playermagicka.swf": lambda: playerbar("#2A4A8AFF", "#9AB0D0B0"),
    "playerstamina.swf": lambda: playerbar("#3A6A32FF", "#A8C49CB0"),
    "breath.swf": lambda: meter("breath", "#3A7488FF"),        # muted teal blue: air left underwater
    "casting.swf": lambda: meter("casting", "#8A7440FF"),      # muted gold: a spell / bow / shout charging
    "detection.swf": lambda: meter("detection", "#8A3A32FF"),  # muted red: how close the most aware actor is to seeing you
    "bowdraw.swf": lambda: meter("bowdraw", "#5A7A46FF"),       # muted green: the bow drawing
    "shoutcharge.swf": lambda: meter("shoutcharge", "#6A5A86FF"),   # muted violet: the shout charging toward its next word
    # info widgets (they carry text - the font import is only in these, so the meters above never depend on it)
    "gold.swf": lambda: text_widget("#B8963CFF"),                # gold: the coins you carry
    "weight.swf": lambda: text_widget("#7A7468FF"),              # grey: carried / maximum weight
    "time.swf": lambda: text_widget("#8FA6B8FF"),               # pale steel blue: the in-game hour
    "shout.swf": lambda: meter("shout", "#6A5A86FF", width=160.0, height=10.0, with_value=True),   # muted violet: the voice recovering, seconds left beside it
    "level_badge.swf": lambda: badge("#5A7A46FF"),           # green ring: the same level, as a badge with an XP ring
    # resistances, one combined widget the width of the HUD's bars (it sits under them): fire, frost, shock, magic /
    # poison, disease, armor rating, speed - STB Widgets' eight, each its icon colour and its value
    "resist.swf": lambda: grid_widget(["#A04A2AFF", "#5A8AA8FF", "#7A6AB8FF", "#8A5A9AFF",
                                       "#5A8A3AFF", "#8A7A4AFF", "#8A8478FF", "#B8B0A0FF"], cols=4, cell_w=44.0, row_h=17.0),
    # equipped: the Tween menu's cross - right hand right, left hand left, shout / power top, ammo (with its count) bottom
    "equip.swf": lambda: equip_cross(["#B8B0A0FF", "#8A8478FF", "#6A5A86FF", "#8A7440FF"]),
    "playtime.swf": lambda: text_widget("#9A8A6AFF"),            # sand: real hours played on this character
    # active effects: up to six "name m:ss", the soonest to end first, no icons
    "effects.swf": lambda: grid_widget([None] * 6, cols=1, cell_w=230.0, row_h=18.0),
    # Survival Mode's needs (only while it is on): how far each has gone
    "hunger.swf": lambda: meter("hunger", "#8A6A3AFF", width=160.0, height=8.0, icon="hunger"),     # muted amber, a drumstick
    "fatigue.swf": lambda: meter("fatigue", "#7A8A9AFF", width=160.0, height=8.0, icon="fatigue"),  # slate, a crescent moon
    "cold.swf": lambda: meter("cold", "#6A8EA6FF", width=160.0, height=8.0, icon="cold"),          # frost blue, a snowflake
    "level.swf": lambda: meter("level", "#5A7A46FF", width=160.0, height=10.0, with_value=True),   # green: progress to the next level, the level beside it
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for fn, make in WIDGETS.items():
        data = make()
        open(os.path.join(OUT, fn), "wb").write(data)
        print(f"{fn}: {len(data)} bytes")


if __name__ == "__main__":
    main()
