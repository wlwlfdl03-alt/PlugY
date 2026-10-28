#!/usr/bin/env python3
"""
Make PlugYInstaller/PlugY/SortBtns.dc6, the image of the stash sort button (frame 0 : up, frame 1 : down).

The button background is the stone background of the PlugY stash buttons (StashBtns.dc6 and SharedGoldBtns.dc6) :
a pixel is kept where frames with different icons agree, the other pixels are filled with the stone texture.
The stone panel is darkened and the icon, two gold arrows turning around, is drawn 8 times bigger then reduced
to 32x32 in the colors of the game palette. The down frame is the up frame shrunk to 30x30 at (1,2),
like the down frames of StashBtns.dc6.

The palette of the game is not in the repository (copyrighted) : pass data\\global\\palette\\act1\\pal.dat.
Needs Pillow.

  tools/make_sort_btn.py pal.dat [--preview preview.png]
"""
import argparse, math, os, struct, sys

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = os.path.join(ROOT, 'PlugYInstaller', 'PlugY')
SIZE = 32
INNER_MIN, INNER_MAX = 5, 28    # stone panel inside the light border of the button


def read_dc6(path):
    d = open(path, 'rb').read()
    dirs, fpd = struct.unpack_from('<II', d, 16)
    frames = []
    for p in struct.unpack_from('<%dI' % (dirs * fpd), d, 24):
        flip, w, h, ox, oy, unk, nb, ln = struct.unpack_from('<IiiiiIII', d, p)
        data = d[p + 32:p + 32 + ln]
        px = [[None] * w for _ in range(h)]
        x, y, k = 0, (0 if flip else h - 1), 0
        while k < len(data) and 0 <= y < h:
            b = data[k]; k += 1
            if b == 0x80:
                x = 0; y += 1 if flip else -1
            elif b & 0x80:
                x += b & 0x7F
            else:
                for j in range(b):
                    px[y][x] = data[k + j]; x += 1
                k += b
        frames.append(px)
    return frames


def encode_frame(px):
    """Same encoding as the frames of StashBtns.dc6 : bottom-up lines, no trailing transparency, 0x80 per line."""
    out = bytearray()
    for y in range(len(px) - 1, -1, -1):
        row = px[y]
        end = len(row)
        while end and row[end - 1] is None:
            end -= 1
        x = 0
        while x < end:
            if row[x] is None:
                n = 0
                while x < end and row[x] is None and n < 0x7F:
                    x += 1; n += 1
                out.append(0x80 | n)
            else:
                run = []
                while x < end and row[x] is not None and len(run) < 0x7F:
                    run.append(row[x]); x += 1
                out.append(len(run)); out += bytes(run)
        out.append(0x80)
    return bytes(out)


def write_dc6(path, frames):
    datas = [encode_frame(f) for f in frames]
    head = struct.pack('<6I', 6, 1, 0, 0xEEEEEEEE, 1, len(frames))
    pos = len(head) + 4 * len(frames)
    ptrs, body = [], bytearray()
    for f, data in zip(frames, datas):
        ptrs.append(pos + len(body))
        body += struct.pack('<IiiiiIII', 0, len(f[0]), len(f), 0, 0, 0, 0, len(data)) + data + b'\xCD' * 3
    open(path, 'wb').write(head + struct.pack('<%dI' % len(ptrs), *ptrs) + body)


def make_background():
    stash = read_dc6(os.path.join(FILES, 'StashBtns.dc6'))
    gold = read_dc6(os.path.join(FILES, 'SharedGoldBtns.dc6'))
    # Up frames grouped by icon : arrows (the left and right arrows share their middle), stashes, gold buttons.
    families = [[stash[0], stash[2], stash[8], stash[10]], [stash[4], stash[6]], [gold[0]], [gold[2]]]
    bg = [[None] * SIZE for _ in range(SIZE)]
    holes = []
    for y in range(SIZE):
        for x in range(SIZE):
            seen = {}
            for i, fam in enumerate(families):
                for f in fam:
                    seen.setdefault(f[y][x], set()).add(i)
            agreed = [v for v, fams in seen.items() if len(fams) >= 3]
            if agreed:
                bg[y][x] = agreed[0]
            else:
                holes.append((y, x))
    # Fill the holes (under the icons) with the stone texture of the panel moved by an offset.
    inner = lambda y, x: INNER_MIN <= y <= INNER_MAX and INNER_MIN <= x <= INNER_MAX
    offsets = [(0, -12), (0, 12), (-12, 0), (12, 0), (-12, -12), (12, 12), (-12, 12), (12, -12),
               (0, -18), (0, 18), (-18, 0), (18, 0)]
    while holes:
        left = []
        for y, x in holes:
            src = [(y + dy, x + dx) for dy, dx in offsets
                   if inner(y + dy, x + dx) and bg[y + dy][x + dx] is not None]
            if src:
                bg[y][x] = bg[src[0][0]][src[0][1]]
            else:
                left.append((y, x))
        if len(left) == len(holes):
            sys.exit('cannot fill the background')
        holes = left
    return bg, stash


def rgb(pal, v):
    return pal[v * 3 + 2], pal[v * 3 + 1], pal[v * 3]


class Palette:
    """Nearest color among the colors of StashBtns.dc6 and the gold colors of the game palette."""
    GOLD = [78, 83, 88, 94, 97, 100, 106, 105, 108, 109, 111, 168, 170]     # dark to light
    BLACK = [172, 173, 174, 175]

    def __init__(self, pal, frames):
        self.pal = pal
        self.colors = sorted({v for f in frames for row in f for v in row if v is not None} | set(self.GOLD + self.BLACK))
        self.cache = {}

    def nearest(self, c):
        c = tuple(int(round(v)) for v in c)
        if c not in self.cache:
            self.cache[c] = min(self.colors, key=lambda v: sum((a - b) ** 2 for a, b in zip(rgb(self.pal, v), c)))
        return self.cache[c]


SCALE = 8                       # the icon is drawn 8 times bigger
PANEL_DARKEN = 0.45             # brightness of the stone panel behind the icon


def icon_mask():
    """Two clockwise arrows on a circle."""
    n = SIZE * SCALE
    mask = Image.new('L', (n, n), 0)
    d = ImageDraw.Draw(mask)
    cx, cy, r, w = 16.5 * SCALE, 16.5 * SCALE, 8.0 * SCALE, 3.2 * SCALE
    for start in (200, 20):
        d.arc([cx - r, cy - r, cx + r, cy + r], start, start + 125, fill=255, width=int(w))
        end = math.radians(start + 125)
        ex, ey = cx + (r - w / 2) * math.cos(end), cy + (r - w / 2) * math.sin(end)
        tx, ty = -math.sin(end), math.cos(end)      # direction of the arrow
        nx, ny = math.cos(end), math.sin(end)
        hw, hl = 4.2 * SCALE, 4.5 * SCALE
        d.polygon([(ex + nx * hw, ey + ny * hw), (ex - nx * hw, ey - ny * hw), (ex + tx * hl, ey + ty * hl)], fill=255)
    return mask


def coverage(mask, grow=0):
    """Part (0..1) of each pixel of the 32x32 image covered by the mask, grown or shrunk by some pixels."""
    if grow:
        size = 2 * abs(grow) * SCALE + 1
        mask = mask.filter(ImageFilter.MaxFilter(size) if grow > 0 else ImageFilter.MinFilter(size))
    small = mask.resize((SIZE, SIZE), Image.BOX)
    return [[small.getpixel((x, y)) / 255.0 for x in range(SIZE)] for y in range(SIZE)]


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def draw_icon(bg, palette):
    pal = palette.pal
    img = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            c = rgb(pal, bg[y][x]) if bg[y][x] is not None else None
            if c and INNER_MIN <= x <= INNER_MAX and INNER_MIN <= y <= INNER_MAX:
                c = tuple(v * PANEL_DARKEN for v in c)
            row.append(c)
        img.append(row)

    mask = icon_mask()
    outline, shape, inner = coverage(mask, 1), coverage(mask), coverage(mask, -1)
    gold = [rgb(pal, v) for v in Palette.GOLD]
    for y in range(SIZE):
        for x in range(SIZE):
            if img[y][x] is None:
                continue
            c = lerp(img[y][x], (6, 4, 2), min(1.0, outline[y][x]))
            if shape[y][x] > 0.02:
                # Lighter in the middle of the arrows and at the top.
                k = max(0.0, min(1.0, 0.25 + 0.55 * inner[y][x] + 0.35 * (1.0 - (y - 6) / 24.0)))
                i = k * (len(gold) - 1)
                g = lerp(gold[int(i)], gold[min(len(gold) - 1, int(i) + 1)], i - int(i))
                c = lerp(c, g, min(1.0, shape[y][x]))
            img[y][x] = c

    return [[None if bg[y][x] is None else palette.nearest(img[y][x]) for x in range(SIZE)] for y in range(SIZE)]


def make_down(up, palette):
    """Shrink the 32x31 up image to 30x30 at (1,2) with an area filter."""
    down = [[None] * SIZE for _ in range(SIZE)]
    sw, sh, dw, dh = 32, 31, 30, 30
    for y in range(dh):
        for x in range(dw):
            acc, tot = [0, 0, 0], 0.0
            y0, y1 = y * sh / dh, (y + 1) * sh / dh
            x0, x1 = x * sw / dw, (x + 1) * sw / dw
            for sy in range(int(y0), min(sh, int(y1) + 1)):
                wy = min(y1, sy + 1) - max(y0, sy)
                for sx in range(int(x0), min(sw, int(x1) + 1)):
                    wx = min(x1, sx + 1) - max(x0, sx)
                    w = wx * wy
                    if w <= 0:
                        continue
                    c = rgb(palette.pal, up[sy + 1][sx])
                    for i in range(3):
                        acc[i] += c[i] * w
                    tot += w
            down[y + 2][x + 1] = palette.nearest([a / tot for a in acc])
    return down


def save_preview(path, frames, pal, ref=None, scale=8):
    shown = list(frames) + (list(ref) if ref else [])
    sheet = Image.new('RGB', (len(shown) * (SIZE * scale + 8), SIZE * scale), (40, 40, 40))
    for i, f in enumerate(shown):
        im = Image.new('RGB', (SIZE, SIZE), (40, 40, 40))
        for y in range(SIZE):
            for x in range(SIZE):
                if f[y][x] is not None:
                    im.putpixel((x, y), rgb(pal, f[y][x]))
        sheet.paste(im.resize((SIZE * scale, SIZE * scale), Image.NEAREST), (i * (SIZE * scale + 8), 0))
    sheet.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('palette', help='act1 pal.dat of the game (256 BGR colors)')
    ap.add_argument('--out', default=os.path.join(FILES, 'SortBtns.dc6'))
    ap.add_argument('--preview', help='also write a PNG of the 2 frames next to the previous/next buttons (needs Pillow)')
    args = ap.parse_args()

    pal = open(args.palette, 'rb').read()
    if len(pal) != 768:
        sys.exit('%s : not a pal.dat (768 bytes expected)' % args.palette)

    bg, stash = make_background()
    palette = Palette(pal, stash)
    up = draw_icon(bg, palette)
    down = make_down(up, palette)
    write_dc6(args.out, [up, down])
    check = read_dc6(args.out)
    assert check == [up, down], 'DC6 encoding check failed'
    print('wrote', args.out)
    if args.preview:
        save_preview(args.preview, [up, down], pal, ref=stash[0:4])
        print('wrote', args.preview)


if __name__ == '__main__':
    main()
