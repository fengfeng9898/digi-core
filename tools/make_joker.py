#!/usr/bin/env python3
"""
make_joker.py — 把「小丑王」三视图合成 4 组动作，输出网页数据和 GIF。

三视图没有中间帧，动作靠「姿态切换 + 位移 + 挤压 + 特效」合成：
  待机 idle  : 正面，呼吸上下浮动
  攻击 attack: 正面蓄力 -> 侧面突进（转向出击）-> 正面收招 + 斩击特效
  受伤 hurt  : 正面左右抖动 + 受击闪白
  胜利 win   : 正面弹跳 + 星星，最后转背面叉手摆姿势

输出:
  web/joker_data.js      window.DIGI_JOKER = { pal, anims }
  docs/joker_*.gif       四组动作 GIF（4 倍放大）
  docs/joker_sheet.png   关键帧拼图（用于预览）
"""
from __future__ import annotations

import os
import random

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "enemy", "joker_src.png")
WEB = os.path.join(ROOT, "web")
DOCS = os.path.join(ROOT, "docs")

CW, CH = 64, 40          # 画布（与其它角色一致）
TARGET_H = 40            # 角色目标高度
COLORS = 15              # 不透明色数
B36 = "0123456789ABCDEF"

# 三视图在素材里的横向范围（按 alpha 扫描得到）
VIEWS = {
    "front": (22, 702),
    "side": (755, 1429),
    "back": (1475, 2142),
}


def load_views():
    im = Image.open(SRC).convert("RGBA")
    out = {}
    for name, (x0, x1) in VIEWS.items():
        crop = im.crop((x0, 0, x1 + 1, im.height))
        bbox = crop.getbbox()
        crop = crop.crop(bbox)
        scale = TARGET_H / crop.height
        w = max(1, round(crop.width * scale))
        out[name] = crop.resize((w, TARGET_H), Image.LANCZOS)
    return out


def make_frame(views, view, dx=0, dy=0, squash=1.0, flash=0.0, effect=None):
    img = views[view]
    w, h = img.size
    if squash != 1.0:
        nh = max(1, round(h * squash))
        img = img.resize((w, nh), Image.LANCZOS)
        h = nh
    canvas = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
    canvas.alpha_composite(img, (CW // 2 - w // 2 + dx, CH - h + dy))

    if flash > 0:
        px = canvas.load()
        for y in range(CH):
            for x in range(CW):
                r, g, b, a = px[x, y]
                if a == 0:
                    continue
                px[x, y] = (round(r + (255 - r) * flash),
                            round(g + (255 - g) * flash),
                            round(b + (255 - b) * flash), a)

    if effect:
        d = ImageDraw.Draw(canvas)
        effect(d)
    return canvas


def slash(cx, cy, size, col=(255, 255, 255, 255)):
    def draw(d):
        for i in range(size):
            t = i / max(1, size - 1)
            yy = cy - size // 2 + i
            xx = cx + round(6 * (1 - abs(t - 0.5) * 2))
            d.point((xx, yy), fill=col)
            d.point((xx + 1, yy), fill=col)
    return draw


def sparkle(seed_x, seed_y):
    def draw(d):
        for (sx, sy) in ((seed_x, seed_y), (seed_x - 12, seed_y + 6), (seed_x + 10, seed_y + 10)):
            for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                d.point((sx + dx, sy + dy), fill=(255, 240, 120, 255))
    return draw


def build_anims(views):
    anims = {}

    # ---- 待机：呼吸 ----
    idle = []
    for dy in (0, -1, -1, 0, 0, 1, 1, 0):
        idle.append(make_frame(views, "front", dy=dy))
    durations = [130] * len(idle)
    anims["idle"] = (idle, durations)

    # ---- 攻击：蓄力 -> 侧身突进 -> 收招 ----
    atk = []
    seq = [
        ("front", -2, 0, 1.0, 0.0, None),
        ("front", -3, 0, 0.97, 0.0, None),
        ("side", 2, 0, 1.0, 0.0, None),
        ("side", 6, 0, 1.0, 0.10, slash(52, 20, 10)),
        ("side", 9, -1, 1.0, 0.18, slash(54, 20, 14)),
        ("side", 7, 0, 1.0, 0.10, slash(52, 20, 10)),
        ("side", 3, 0, 1.0, 0.0, None),
        ("front", 1, 0, 1.0, 0.0, None),
        ("front", 0, 0, 1.0, 0.0, None),
    ]
    for v, dx, dy, sq, fl, eff in seq:
        atk.append(make_frame(views, v, dx=dx, dy=dy, squash=sq, flash=fl, effect=eff))
    anims["attack"] = (atk, [100] * len(atk))

    # ---- 受伤：抖动 + 闪白 ----
    hurt = []
    seq = [(0, 0.75), (-2, 0.30), (2, 0.0), (-2, 0.0), (2, 0.0), (-1, 0.0), (1, 0.0), (0, 0.0)]
    for dx, fl in seq:
        hurt.append(make_frame(views, "front", dx=dx, flash=fl))
    anims["hurt"] = (hurt, [95] * len(hurt))

    # ---- 胜利：弹跳 + 星星，最后背面叉手 ----
    win = []
    for dy in (0, -3, -5, -3, 0, -2, 0):
        win.append(make_frame(views, "front", dy=dy))
    win.append(make_frame(views, "back", dy=-1, effect=sparkle(20, 12)))
    win.append(make_frame(views, "back", dy=0, effect=sparkle(24, 16)))
    win.append(make_frame(views, "back", dy=-1))
    anims["win"] = (win, [120] * len(win))
    return anims


def quantize(frames, colors=COLORS):
    """所有帧共享一套调色板；透明像素索引 0。"""
    samples = []
    for f in frames:
        for r, g, b, a in f.getdata():
            if a > 128:
                samples.append((r // 4 * 4, g // 4 * 4, b // 4 * 4))
    random.seed(7)
    if len(samples) > 60000:
        samples = random.sample(samples, 60000)

    pal_img = Image.new("RGB", (len(samples), 1))
    pal_img.putdata(samples)
    q = pal_img.quantize(colors=colors, method=Image.MEDIANCUT)
    raw = q.getpalette()[: colors * 3]
    # 索引 0 留给透明，实际颜色放在 1..colors
    palette = [(0, 0, 0)] + [tuple(raw[i * 3:i * 3 + 3]) for i in range(colors)]

    def nearest(c):
        best, bi = 1 << 30, 1
        for i, p in enumerate(palette):
            if i == 0:
                continue          # 索引 0 是透明占位
            d = (c[0] - p[0]) ** 2 + (c[1] - p[1]) ** 2 + (c[2] - p[2]) ** 2
            if d < best:
                best, bi = d, i
        return bi

    cache = {}
    rows_all = []
    for f in frames:
        rows = []
        for y in range(CH):
            row = []
            for x in range(CW):
                r, g, b, a = f.getpixel((x, y))
                if a <= 128:
                    row.append("0")
                    continue
                key = (r, g, b)
                idx = cache.get(key)
                if idx is None:
                    idx = nearest(key)
                    cache[key] = idx
                row.append(B36[idx])
            rows.append("".join(row))
        rows_all.append(rows)
    return palette, rows_all


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def main():
    views = load_views()
    for k, v in views.items():
        print(f"view {k}: {v.size}")

    anims = build_anims(views)

    all_frames = []
    for name, (frames, _) in anims.items():
        all_frames.extend(frames)
    palette, rows = quantize(all_frames)

    # 回填索引
    out = {}
    pos = 0
    for name, (frames, durs) in anims.items():
        n = len(frames)
        out[name] = {"dur": durs, "frames": rows[pos:pos + n]}
        pos += n

    pal565 = ",".join(f"0x{rgb565(c):04X}" for c in palette)
    os.makedirs(WEB, exist_ok=True)
    with open(os.path.join(WEB, "joker_data.js"), "w", encoding="utf-8", newline="\n") as f:
        f.write("// AUTO-GENERATED by tools/make_joker.py\n")
        f.write("window.DIGI_JOKER = {\n")
        f.write(f"  pal: [{pal565}],\n")
        f.write("  anims: {\n")
        for name, a in out.items():
            f.write(f'    {name}: {{ dur: [{",".join(str(d) for d in a["dur"])}], frames: [\n')
            for frame_rows in a["frames"]:
                f.write("      [\n")
                for r in frame_rows:
                    f.write(f'        "{r}",\n')
                f.write("      ],\n")
            f.write("    ]},\n")
        f.write("  }\n};\n")
    print("wrote web/joker_data.js", f"{os.path.getsize(os.path.join(WEB,'joker_data.js'))/1024:.0f} KB")

    # GIF（4 倍放大，深色底）
    SC = 4
    for name, (frames, durs) in anims.items():
        imgs = []
        for f in frames:
            bg = Image.new("RGB", (CW, CH), (12, 16, 22))
            bg.paste(f, (0, 0), f)
            imgs.append(bg.resize((CW * SC, CH * SC), Image.NEAREST))
        path = os.path.join(DOCS, f"joker_{name}.gif")
        imgs[0].save(path, save_all=True, append_images=imgs[1:], duration=durs, loop=0, optimize=False)
        print("gif", path, len(frames), "frames")

    # 关键帧拼图
    picks = [("idle", 1), ("attack", 3), ("attack", 4), ("hurt", 1), ("hurt", 2), ("win", 2), ("win", 8)]
    tiles = []
    for name, idx in picks:
        frames, _ = anims[name]
        tiles.append(frames[min(idx, len(frames) - 1)])

    S = 5
    sheet = Image.new("RGB", (len(tiles) * (CW * S + 8) + 8, CH * S + 16), (12, 16, 22))
    for i, f in enumerate(tiles):
        bg = Image.new("RGB", (CW, CH), (12, 16, 22))
        bg.paste(f, (0, 0), f)
        sheet.paste(bg.resize((CW * S, CH * S), Image.NEAREST), (8 + i * (CW * S + 8), 8))
    sheet.save(os.path.join(DOCS, "joker_sheet.png"))
    print("wrote docs/joker_sheet.png")

    # 每组动作的完整帧条（看动态用）
    S3 = 3
    for name, (frames, _) in anims.items():
        strip = Image.new("RGB", (len(frames) * CW * S3, CH * S3), (12, 16, 22))
        for i, fr in enumerate(frames):
            bg = Image.new("RGB", (CW, CH), (12, 16, 22))
            bg.paste(fr, (0, 0), fr)
            strip.paste(bg.resize((CW * S3, CH * S3), Image.NEAREST), (i * CW * S3, 0))
        strip.save(os.path.join(DOCS, f"joker_frames_{name}.png"))
        print("strip", name, len(frames))


if __name__ == "__main__":
    main()
