#!/usr/bin/env python3
"""Generate dist/Interface/ApparelPreview/previewicon.swf.

Uncompressed SWF6 (AS2/GFx-compatible) containing one exported MovieClip
"PreviewIcon": a gold eye (preview = "you can see it on you") sized 32x32 px.
Hand-encoded per the SWF spec: DefineShape (v1, solid RGB fills) for the eye
almond and the pupil, wrapped in a DefineSprite, exported via ExportAssets.
"""

import struct
from pathlib import Path

TWIPS = 20  # twips per pixel


class BitWriter:
    def __init__(self):
        self.bytes = bytearray()
        self.bitpos = 0  # bits used in current byte

    def write_bits(self, value: int, nbits: int):
        if nbits == 0:
            return
        mask = (1 << nbits) - 1
        value &= mask
        for i in range(nbits - 1, -1, -1):
            bit = (value >> i) & 1
            if self.bitpos == 0:
                self.bytes.append(0)
            self.bytes[-1] |= bit << (7 - self.bitpos)
            self.bitpos = (self.bitpos + 1) % 8

    def write_sbits(self, value: int, nbits: int):
        self.write_bits(value & ((1 << nbits) - 1), nbits)

    def align(self):
        self.bitpos = 0

    def data(self) -> bytes:
        return bytes(self.bytes)


def sbits_needed(*values: int) -> int:
    n = 1
    for v in values:
        needed = v.bit_length() + 1  # sign bit
        n = max(n, needed)
    return n


def rect(xmin, xmax, ymin, ymax) -> bytes:
    bw = BitWriter()
    n = sbits_needed(xmin, xmax, ymin, ymax)
    bw.write_bits(n, 5)
    for v in (xmin, xmax, ymin, ymax):
        bw.write_sbits(v, n)
    bw.align()
    return bw.data()


def tag(code: int, body: bytes) -> bytes:
    if len(body) >= 0x3F:
        return struct.pack("<HI", (code << 6) | 0x3F, len(body)) + body
    return struct.pack("<H", (code << 6) | len(body)) + body


def style_change_moveto(bw: BitWriter, dx: int, dy: int, fillstyle1: int):
    bw.write_bits(0, 1)  # non-edge record
    # Flag bits MSB->LSB: NewStyles, LineStyle, FillStyle1, FillStyle0, MoveTo.
    bw.write_bits(0b00101 if fillstyle1 else 0b00001, 5)
    n = sbits_needed(dx, dy)
    bw.write_bits(n, 5)
    bw.write_sbits(dx, n)
    bw.write_sbits(dy, n)
    if fillstyle1:
        bw.write_bits(fillstyle1, 1)  # fill bits == 1 -> 1 bit index


def curve(bw: BitWriter, cdx: int, cdy: int, adx: int, ady: int):
    bw.write_bits(0b10, 2)  # curved edge
    n = max(2, sbits_needed(cdx, cdy, adx, ady))
    bw.write_bits(n - 2, 4)
    for v in (cdx, cdy, adx, ady):
        bw.write_sbits(v, n)


def end_shape(bw: BitWriter):
    bw.write_bits(0, 6)
    bw.align()


def define_shape(shape_id: int, bounds: bytes, rgb: tuple, path_fn) -> bytes:
    body = bytearray()
    body += struct.pack("<H", shape_id)
    body += bounds
    body += bytes([1, 0x00, rgb[0], rgb[1], rgb[2]])  # 1 fill style: solid RGB
    body += bytes([0])                                # 0 line styles
    body += bytes([0x10])                             # 1 fill bit, 0 line bits
    bw = BitWriter()
    path_fn(bw)
    end_shape(bw)
    body += bw.data()
    return tag(2, bytes(body))


def px(v: float) -> int:
    return int(round(v * TWIPS))


def eye_path(bw: BitWriter):
    # Almond/eye outline: two symmetric quadratic curves. NOTE: SWF curve
    # records take the control delta from the CURRENT point and the anchor
    # delta from the CONTROL point.
    style_change_moveto(bw, px(-15), 0, 1)
    curve(bw, px(15), px(-13), px(15), px(13))    # ctrl (0,-13) -> anchor (15,0)
    curve(bw, px(-15), px(13), px(-15), px(-13))  # ctrl (0,13)  -> anchor (-15,0)


def pupil_path(bw: BitWriter):
    # Circle r=5.5px via 4 quadratic segments.
    r = 5.5
    style_change_moveto(bw, px(r), 0, 1)
    curve(bw, 0, px(r), px(-r), 0)   # (r,0)  -> ctrl (r,r)   -> (0,r)
    curve(bw, px(-r), 0, 0, px(-r))  # (0,r)  -> ctrl (-r,r)  -> (-r,0)
    curve(bw, 0, px(-r), px(r), 0)   # (-r,0) -> ctrl (-r,-r) -> (0,-r)
    curve(bw, px(r), 0, 0, px(r))    # (0,-r) -> ctrl (r,-r)  -> (r,0)


def place_object2(depth: int, char_id: int, tx: int = 0, ty: int = 0) -> bytes:
    body = bytearray()
    body += bytes([0x06])  # HasCharacter | HasMatrix
    body += struct.pack("<HH", depth, char_id)
    bw = BitWriter()
    bw.write_bits(0, 1)  # no scale
    bw.write_bits(0, 1)  # no rotate
    n = max(1, sbits_needed(tx, ty))
    bw.write_bits(n, 5)
    bw.write_sbits(tx, n)
    bw.write_sbits(ty, n)
    bw.align()
    body += bw.data()
    return tag(26, bytes(body))


def main():
    out = Path(__file__).resolve().parent.parent / "dist/Interface/ApparelPreview/previewicon.swf"
    out.parent.mkdir(parents=True, exist_ok=True)

    gold = (255, 215, 0)
    dark = (43, 32, 8)
    eye_bounds = rect(px(-16), px(16), px(-14), px(14))
    pupil_bounds = rect(px(-6), px(6), px(-6), px(6))

    tags = bytearray()
    tags += tag(9, bytes([0, 0, 0]))  # SetBackgroundColor (ignored for symbols)
    tags += define_shape(1, eye_bounds, gold, eye_path)
    tags += define_shape(2, pupil_bounds, dark, pupil_path)

    # Icon symbols are consumed with a top-left origin: keep all content in the
    # positive quadrant by translating the origin-centred shapes to (+16,+14)px.
    sprite_body = bytearray()
    sprite_body += struct.pack("<HH", 3, 1)  # sprite id 3, 1 frame
    sprite_body += place_object2(1, 1, px(16), px(14))
    sprite_body += place_object2(2, 2, px(16), px(14))
    sprite_body += tag(1, b"")  # ShowFrame
    sprite_body += tag(0, b"")  # End
    tags += tag(39, bytes(sprite_body))

    tags += tag(56, struct.pack("<HH", 1, 3) + b"PreviewIcon\x00")  # ExportAssets
    tags += tag(1, b"")  # ShowFrame
    tags += tag(0, b"")  # End

    frame = rect(0, px(32), 0, px(32))
    header_tail = frame + struct.pack("<HH", 12 << 8, 1)  # 12 fps (8.8 fixed), 1 frame
    file_len = 8 + len(header_tail) + len(tags)
    swf = b"FWS" + bytes([6]) + struct.pack("<I", file_len) + header_tail + tags

    out.write_bytes(swf)
    print(f"wrote {out} ({len(swf)} bytes)")


if __name__ == "__main__":
    main()
