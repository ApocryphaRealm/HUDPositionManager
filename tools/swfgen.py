r"""A small writer for the AS2 widget SWFs HUD Position Manager loads into Skyrim's HUD (phase 2, 2026-10-04).

The owner: every widget HPM builds must be reskinnable ("whatever widgets we add can be re-skinned by Norden Black"), so
each widget is an ordinary SWF - version 8, ActionScript 2, no script of ours inside - whose main timeline holds NAMED
sprites the DLL drives (the clip contract, README "Widget art for reskins"):
    Frame   the art behind everything
    Fill    a meter's filled part, its registration point on its LEFT edge: the DLL sets _xscale 0..100
    Icon    an optional symbol
    Value   a dynamic HTML text field in the game's $EverywhereFont, imported the way the game's own HUD imports it -
            ImportAssets2 from "gfxfontlib.swf", Scaleform's font-library alias (2026-10-04: an import from fonts_en.swf,
            a file the interface path does not hold under that name, made Skyrim refuse the whole child movie)
    Mark    a ring indicator's one arc, duplicated and rotated by the DLL
Any SWF editor (JPEXS FFDec) opens these; a reskin keeps the instance names and replaces the art.

Coordinates are in pixels here and written in twips (1/20 px). Shapes are DefineShape3 (RGBA fills), straight edges only
(a curve is written as short straight segments).
"""
import math
import struct
import zlib


class Bits:
    def __init__(self):
        self.bits = []

    def u(self, value, n):
        for i in range(n - 1, -1, -1):
            self.bits.append((value >> i) & 1)

    def s(self, value, n):
        self.u(value & ((1 << n) - 1), n)

    def bytes(self):
        out = bytearray()
        bits = self.bits + [0] * ((8 - len(self.bits) % 8) % 8)
        for i in range(0, len(bits), 8):
            b = 0
            for bit in bits[i:i + 8]:
                b = (b << 1) | bit
            out.append(b)
        return bytes(out)


def nbits_signed(*values):
    n = 1
    for v in values:
        need = (abs(v).bit_length() + 1) if v else 1
        n = max(n, need)
    return n


def rect(xmin, xmax, ymin, ymax):
    tw = [int(round(v * 20)) for v in (xmin, xmax, ymin, ymax)]
    n = nbits_signed(*tw)
    b = Bits()
    b.u(n, 5)
    for v in tw:
        b.s(v, n)
    return b.bytes()


def matrix(tx=0.0, ty=0.0, sx=None, sy=None, rot=None):
    """A MATRIX record: translate in px, optional scale / rotation (rot in degrees)."""
    b = Bits()
    if rot is not None:
        c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
        sx_, sy_ = (sx or 1.0) * c, (sy or 1.0) * c
        r0, r1 = s, -s
        b.u(1, 1)
        a, d = int(round(sx_ * 65536)), int(round(sy_ * 65536))
        n = nbits_signed(a, d)
        b.u(n, 5); b.s(a, n); b.s(d, n)
        b.u(1, 1)
        e, f = int(round(r0 * 65536)), int(round(r1 * 65536))
        n = nbits_signed(e, f)
        b.u(n, 5); b.s(e, n); b.s(f, n)
    elif sx is not None or sy is not None:
        b.u(1, 1)
        a, d = int(round((sx or 1.0) * 65536)), int(round((sy or 1.0) * 65536))
        n = nbits_signed(a, d)
        b.u(n, 5); b.s(a, n); b.s(d, n)
        b.u(0, 1)
    else:
        b.u(0, 1)
        b.u(0, 1)
    t = [int(round(tx * 20)), int(round(ty * 20))]
    n = nbits_signed(*t) if any(t) else 0
    b.u(n, 5)
    if n:
        b.s(t[0], n); b.s(t[1], n)
    return b.bytes()


def tag(code, body):
    if len(body) < 0x3F and code not in (2, 32, 37, 39, 26):   # long form for shapes / sprites / text / place, for editors
        return struct.pack("<H", (code << 6) | len(body)) + body
    return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body


def rgba(c):
    """'#RRGGBB' or '#RRGGBBAA' -> 4 bytes."""
    c = c.lstrip("#")
    r, g, b = int(c[0:2], 16), int(c[2:4], 16), int(c[4:6], 16)
    a = int(c[6:8], 16) if len(c) >= 8 else 255
    return bytes((r, g, b, a))


def shape3(char_id, polygons):
    """DefineShape3 from [(fill '#RRGGBBAA' or None, line (width_px, '#RRGGBBAA') or None, [(x, y), ...] closed)]."""
    fills = [p[0] for p in polygons if p[0]]
    lines = [p[1] for p in polygons if p[1]]
    xs = [x for p in polygons for x, _ in p[2]]
    ys = [y for p in polygons for _, y in p[2]]
    pad = max([l[0] for l in lines] + [0])
    bounds = rect(min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad)
    body = bytearray(struct.pack("<H", char_id)) + bounds
    body.append(len(fills))
    for f in fills:
        body += b"\x00" + rgba(f)
    body.append(len(lines))
    for w, c in lines:
        body += struct.pack("<H", int(round(w * 20))) + rgba(c)
    nfill = max(1, len(fills).bit_length())
    nline = max(1, len(lines).bit_length()) if lines else 0
    b = Bits()
    b.u(nfill, 4)
    b.u(nline, 4)
    fi = li = 0
    cx = cy = 0
    for fill, line, pts in polygons:
        fill_idx = (fi := fi + 1) if fill else 0
        line_idx = (li := li + 1) if line else 0
        x0, y0 = int(round(pts[0][0] * 20)), int(round(pts[0][1] * 20))
        # style change: move to, fill style 1, line style
        b.u(0, 1)                     # non-edge
        b.u(0, 1)                     # new styles
        b.u(1 if line_idx else (1 if nline else 0), 1)   # line style
        b.u(1, 1)                     # fill style 1
        b.u(0, 1)                     # fill style 0
        b.u(1, 1)                     # move to
        n = nbits_signed(x0, y0)
        b.u(n, 5); b.s(x0, n); b.s(y0, n)
        b.u(fill_idx, nfill)          # fill style 1 (fill style 0 is not written: its flag is off)
        if nline:
            b.u(line_idx, nline)
        cx, cy = x0, y0
        for x, y in list(pts[1:]) + [pts[0]]:
            tx, ty = int(round(x * 20)), int(round(y * 20))
            dx, dy = tx - cx, ty - cy
            if dx == 0 and dy == 0:
                continue
            n = max(2, nbits_signed(dx, dy))
            b.u(1, 1)                 # edge
            b.u(1, 1)                 # straight
            b.u(n - 2, 4)
            if dx and dy:
                b.u(1, 1); b.s(dx, n); b.s(dy, n)
            elif dx:
                b.u(0, 1); b.u(0, 1); b.s(dx, n)
            else:
                b.u(0, 1); b.u(1, 1); b.s(dy, n)
            cx, cy = tx, ty
    b.u(0, 6)                         # end of shape
    body += b.bytes()
    return tag(32, bytes(body))


def place(depth, char_id, name=None, mat=None):
    """PlaceObject2: character at depth, optional instance name and matrix."""
    flags = 0x02 | (0x04 if mat is not None else 0) | (0x20 if name else 0)
    body = struct.pack("<BH", flags, depth) + struct.pack("<H", char_id)
    if mat is not None:
        body += mat
    if name:
        body += name.encode("latin-1") + b"\x00"
    return tag(26, body)


def place_glow(depth, char_id, name, mat, color="#000000E6", blur=3.0, strength=3.0, passes=1):
    """PlaceObject3 with a GlowFilter: an outline round a text field, so text stays readable on a bright scene without a
    plate behind it (Scaleform draws the Glow, DropShadow and Blur filters on text fields). The owner, 2026-10-04: an
    outline or a shadow on the text, never a box."""
    flags1 = 0x02 | 0x04 | 0x20                      # HasCharacter, HasMatrix, HasName
    flags2 = 0x01                                    # HasFilterList
    body = struct.pack("<BBH", flags1, flags2, depth) + struct.pack("<H", char_id) + mat + name.encode("latin-1") + b"\x00"
    body += struct.pack("<B", 1)                     # one filter
    body += struct.pack("<B", 2)                     # FilterID 2: GlowFilter
    body += rgba(color)
    body += struct.pack("<II", int(blur * 65536), int(blur * 65536))   # BlurX, BlurY: FIXED 16.16
    body += struct.pack("<H", int(strength * 256))   # Strength: FIXED8 8.8
    body += struct.pack("<B", 0x20 | (passes & 0x1F))   # not inner, no knockout, CompositeSource (must be 1), passes
    return tag(70, body)


def show_frame():
    return tag(1, b"")


def end():
    return b"\x00\x00"


def sprite(char_id, children):
    """DefineSprite with one frame: children = [(depth, char_id, name or None, matrix bytes or None)]."""
    body = struct.pack("<HH", char_id, 1)
    for depth, cid, name, mat in children:
        body += place(depth, cid, name, mat)
    body += show_frame() + end()
    return tag(39, body)


def import_font(char_id, font_name="$EverywhereFont", url="gfxfontlib.swf"):
    """ImportAssets2 (SWF 8): the game's font by its export name from gfxfontlib.swf - exactly what the game's
    hudmenu.swf carries ($EverywhereFont, $EverywhereMediumFont, $DragonFont)."""
    body = url.encode("latin-1") + b"\x00" + b"\x01\x00" + struct.pack("<H", 1) + struct.pack("<H", char_id) + font_name.encode("latin-1") + b"\x00"
    return tag(71, body)


def font_placeholder(char_id, font_name="$EverywhereFont"):
    """DefineFont2 with no glyphs, named for the game's font: Scaleform maps the name through Skyrim's fontconfig.txt to
    the font library (fonts_en.swf), the way a font is referenced without importing it. (An ImportAssets2 of
    fonts_en.swf made Skyrim refuse the whole child movie - 2026-10-04.) UNPROVEN until a text widget is seen in game."""
    name = font_name.encode("latin-1")
    body = struct.pack("<HBBB", char_id, 0x04, 1, len(name)) + name + struct.pack("<HH", 0, 0)   # wide codes; no glyphs; code table offset
    return tag(48, body)


def edit_text(char_id, w, h, font_id, size_px, color="#FFFFFFFF", align=0, initial=" ", font_name="$EverywhereFont"):
    """DefineEditText: a read-only, non-selectable dynamic HTML field (align 0 left, 1 right, 2 centre), set up as the
    game's HUD fields are (html, useOutlines, an initial <font face> so setting .text keeps the font)."""
    body = struct.pack("<H", char_id) + rect(0, w, 0, h)
    a = ("left", "right", "center")[align]
    c = color.lstrip("#")[:6]
    initial = f'<p align="{a}"><font face="{font_name}" size="{int(size_px)}" color="#{c}">{initial}</font></p>'
    flags1 = 0x01 | 0x04 | 0x08 | 0x80                          # HasFont, HasTextColor, ReadOnly, HasText
    flags2 = 0x10 | 0x20 | 0x01 | 0x02                          # NoSelect, HasLayout, UseOutlines, HTML
    body += struct.pack("<BB", flags1 & 0xFF, flags2)
    body += struct.pack("<HH", font_id, int(round(size_px * 20)))
    body += rgba(color)
    body += struct.pack("<BHHHh", align, 0, 0, 0, 0)
    body += b"\x00"   # no variable name, as the game's fields: one named like the instance ("Value") shadows it in AS2
    if initial:
        body += initial.encode("utf-8") + b"\x00"
    return tag(37, body)


def movie(width, height, tags, version=8):
    """A whole SWF: header, the tags, one frame, end. Compressed (CWS) as the game's own are."""
    payload = rect(0, width, 0, height) + struct.pack("<HH", 30 << 8, 1)   # 30 fps, 1 frame
    # FileAttributes must be the first tag of a SWF 8+ (flags 0: ActionScript 2, no network access). Scaleform refuses a
    # movie without it - loadMovie emptied the target and nothing loaded - though FFDec reads it (2026-10-04)
    payload += struct.pack("<HI", (69 << 6) | 4, 0)
    for t in tags:
        payload += t
    payload += show_frame() + end()
    raw = b"FWS" + bytes([version]) + struct.pack("<I", 8 + len(payload)) + payload
    return b"CWS" + raw[3:8] + zlib.compress(raw[8:])


def rounded_rect(x, y, w, h, r, steps=4):
    """A rectangle with its corners cut by `steps` straight segments each (a curve as straight edges)."""
    if r <= 0:
        return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]
    pts = []
    for cx, cy, a0 in ((x + w - r, y + r, -90), (x + w - r, y + h - r, 0), (x + r, y + h - r, 90), (x + r, y + r, 180)):
        for i in range(steps + 1):
            a = math.radians(a0 + 90 * i / steps)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def arc(radius, thickness, span_deg, steps=16):
    """A ring segment centred on angle 0 (pointing up), as a polygon: the outer edge, then the inner edge back."""
    outer, inner = [], []
    for i in range(steps + 1):
        a = math.radians(-90 - span_deg / 2 + span_deg * i / steps)
        outer.append((radius * math.cos(a), radius * math.sin(a)))
        inner.append(((radius - thickness) * math.cos(a), (radius - thickness) * math.sin(a)))
    return outer + inner[::-1]
