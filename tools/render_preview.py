"""把 sprites.h 里的索引色数据渲染成 PNG，方便肉眼检查美术。"""
import re
from PIL import Image, ImageDraw

SRC = "src/assets/sprites.h"
SCALE = 5
CELL = 44
NAMES = [("EGG", 24), ("BABY", 24), ("CHILD", 32), ("TEEN", 40), ("DEAD", 24)]

txt = open(SRC, encoding="utf-8").read()


def parse(name):
    pal = re.search(r"PAL_%s\[16\] = \{([^}]*)\}" % name, txt).group(1)
    colors = []
    for v in pal.split(","):
        v = v.strip()
        if not v:
            continue
        n = int(v, 16)
        r = ((n >> 11) & 0x1F) * 255 // 31
        g = ((n >> 5) & 0x3F) * 255 // 63
        b = (n & 0x1F) * 255 // 31
        colors.append((r, g, b))
    rows = re.findall(r'"([0-9A-F]+)"', re.search(r"SPR_%s\[\] = \{(.*?)\};" % name, txt, re.S).group(1))
    return colors, rows


W = CELL * SCALE * len(NAMES)
H = CELL * SCALE + 26
img = Image.new("RGB", (W, H), (8, 10, 14))
d = ImageDraw.Draw(img)

for i, (name, size) in enumerate(NAMES):
    pal, rows = parse(name)
    tile = Image.new("RGB", (size, size), (8, 10, 14))
    px = tile.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            idx = int(ch, 16)
            if idx and idx < len(pal):
                px[x, y] = pal[idx]
    tile = tile.resize((size * SCALE, size * SCALE), Image.NEAREST)
    ox = i * CELL * SCALE + (CELL - size) * SCALE // 2
    oy = (CELL - size) * SCALE // 2 + 6
    img.paste(tile, (ox, oy))
    d.text((i * CELL * SCALE + 8, H - 20), name, fill=(120, 140, 160))

img.save("docs/sprites_preview.png")
print("wrote docs/sprites_preview.png", img.size)
