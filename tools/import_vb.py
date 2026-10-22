#!/usr/bin/env python3
"""import_vb.py — 从 digimon-assets-VB 导入角色到固件/网页。

源: https://github.com/kongtaehun/digimon-assets-VB  (72x72 彩色 GIF, frame1/frame2)
目标: 64x40 画布 / 16 色索引 / ASCII 行（sprite.h 的格式）

每只角色合成 4 组动作（源只有 2 帧，其余靠位移+闪白+特技合成）：
  idle   2 帧  frame1 / frame2 交替（VB 原生呼吸）
  attack 3 帧  后撤 -> 突进+斩击 -> 前倾
  hurt   3 帧  闪白 -> 左抖 -> 右抖
  win    4 帧  上浮 x2 -> 星星 x2

用法:
  python tools/import_vb.py --src ../vb-assets/assets/data --ids metalgrey_va,wargrey
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import random

from PIL import Image, ImageDraw, ImageEnhance

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CW, CH = 64, 40
TARGET_H = 40
COLORS = 15
B36 = "0123456789ABCDEF"

# id -> 固件里的枚举名（CH_xxx）
CHAR_NAMES = {
    "bota": "VB_BOTA", "koro": "VB_KORO", "agu": "VB_AGU", "grey": "VB_GREY",
    "metalgrey_va": "VB_METALGREY", "wargrey": "VB_WARGREY",
    "chibi": "VB_CHIBI", "v": "VB_V", "xv": "VB_XV",
    "paildra": "VB_PAILDRA", "imperialdra_f": "VB_IMPERIAL",
    "pata": "VB_PATA", "ange": "VB_ANGE", "holyange": "VB_HOLYANGE", "seraphi": "VB_SERAPHI",
    "gummy": "VB_GUMMY", "terrier": "VB_TERRIER", "galgo": "VB_GALGO",
    "rapid": "VB_RAPID", "saintgalgo": "VB_SAINTGALGO",
    "tail_c": "VB_TAILMON", "neferti": "VB_NEFERTI", "silphy": "VB_SILPHY", "holydra": "VB_HOLYDRA",
    "skullgrey": "VB_SKULLGREY", "gabu": "VB_GABU",
    "egggggg": "VB_EGG",
}


def collect(root):
    db = {}
    for dj in sorted(glob.glob(os.path.join(root, "*", "*", "dim.json"))):
        d = json.load(open(dj, encoding="utf-8"))
        base = os.path.dirname(dj)
        for m in d.get("digimon", []):
            sp = m.get("sprites", {})
            if not sp.get("frame1") or m["id"] in db:
                continue
            f1 = os.path.join(base, sp["frame1"])
            f2 = os.path.join(base, sp["frame2"]) if sp.get("frame2") else None
            if not os.path.exists(f1):
                continue
            db[m["id"]] = {
                "name": m.get("name", m["id"]),
                "stage": m.get("stage", ""),
                "attr": m.get("attribute", ""),
                "stats": m.get("stats", {}),
                "f1": f1,
                "f2": f2 if f2 and os.path.exists(f2) else f1,
            }
    return db


def bbox_union(imgs):
    box = None
    for im in imgs:
        b = im.getbbox()
        if not b:
            continue
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]),
                                     max(box[2], b[2]), max(box[3], b[3]))
    return box


def load_pair(info):
    """裁到共同包围盒 -> 缩放到高 TARGET_H -> 返回两张 RGBA"""
    imgs = [Image.open(info["f1"]).convert("RGBA"), Image.open(info["f2"]).convert("RGBA")]
    box = bbox_union(imgs)
    out = []
    for im in imgs:
        c = im.crop(box)
        s = TARGET_H / c.height
        w = max(1, round(c.width * s))
        # 预乘 alpha 再缩放，避免透明像素的黑色渗进边缘（黑边）
        c = c.convert("RGBa").resize((w, TARGET_H), Image.LANCZOS).convert("RGBA")
        # 点阵屏上要更分明：提高饱和度与对比度
        _rgb = c.convert("RGB")
        _rgb = ImageEnhance.Color(_rgb).enhance(1.55)
        _rgb = ImageEnhance.Contrast(_rgb).enhance(1.30)
        c = Image.merge("RGBA", _rgb.split() + (c.split()[3],))
        out.append(c)
    return out


def place(im, dx=0, dy=0, effect=None):
    canvas = Image.new("RGBA", (CW, CH), (0, 0, 0, 0))
    x = (CW - im.width) // 2 + dx
    y = CH - im.height + dy
    canvas.alpha_composite(im, (max(0, x), max(0, y)))
    return canvas


def flash(im, level):
    px = im.load()
    for y in range(CH):
        for x in range(CW):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            px[x, y] = (round(r + (255 - r) * level),
                        round(g + (255 - g) * level),
                        round(b + (255 - b) * level), a)
    return im


def slash(cx, cy, size, col=(255, 255, 255, 255)):
    def d(draw):
        for i in range(size):
            t = i / max(1, size - 1)
            xx = cx + round(5 * (1 - abs(t - 0.5) * 2))
            yy = cy - size // 2 + i
            draw.point((xx, yy), fill=col)
            draw.point((xx + 1, yy), fill=col)
    return d


def sparkle(pts):
    def d(draw):
        for (sx, sy) in pts:
            for ddx, ddy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                x, y = sx + ddx, sy + ddy
                if 0 <= x < CW and 0 <= y < CH:
                    draw.point((x, y), fill=(255, 240, 120, 255))
    return d


def build_anims(a, b):
    """a = frame1, b = frame2（已裁好缩放）"""
    anims = {}

    idle = [place(a), place(b, dy=-1)]
    anims["idle"] = (idle, [700, 700])

    atk = [
        place(a, dx=-3),
        place(b, dx=4, effect=slash(52, 20, 12)),
        place(a, dx=2, dy=-1),
    ]
    anims["attack"] = (atk, [110, 130, 120])

    hurt = [
        flash(place(a), 0.72),
        place(b, dx=-2),
        place(a, dx=2),
    ]
    anims["hurt"] = (hurt, [90, 100, 100])

    win = [
        place(a, dy=-2),
        place(b, dy=-4),
        place(a, dy=-3, effect=sparkle([(18, 12), (46, 10), (54, 22)])),
        place(b, dy=-5, effect=sparkle([(22, 8), (48, 16), (38, 6)])),
    ]
    anims["win"] = (win, [160, 160, 180, 180])
    return anims


def quantize(frames, colors=COLORS):
    samples = []
    for f in frames:
        for r, g, b, al in f.getdata():
            if al > 128:
                samples.append((r // 4 * 4, g // 4 * 4, b // 4 * 4))
    random.seed(11)
    if len(samples) > 40000:
        samples = random.sample(samples, 40000)
    if not samples:
        raise SystemExit("没有任何不透明像素")

    strip = Image.new("RGB", (len(samples), 1))
    strip.putdata(samples)
    q = strip.quantize(colors=colors, method=Image.MEDIANCUT)
    raw = q.getpalette()[: colors * 3]
    palette = [(0, 0, 0)] + [tuple(raw[i * 3:i * 3 + 3]) for i in range(colors)]

    def nearest(c):
        best, bi = 1 << 30, 1
        for i, p in enumerate(palette):
            if i == 0:
                continue
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
                r, g, b, al = f.getpixel((x, y))
                if al <= 128:
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
    return ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.path.join(ROOT, "..", "vb-assets", "assets", "data"))
    ap.add_argument("--ids", default="metalgrey_va,wargrey")
    ap.add_argument("--egg", default=os.path.join(ROOT, "..", "vb-assets", "assets", "data",
                                                  "bundled", "dro", "digitama.gif"))
    ap.add_argument("--out-h", default=os.path.join(ROOT, "src", "assets", "vb_data.h"))
    ap.add_argument("--out-js", default=os.path.join(ROOT, "web", "vb_data.js"))
    ap.add_argument("--preview", default=os.path.join(ROOT, "docs", "vb_imported.png"))
    args = ap.parse_args()

    src = os.path.abspath(args.src)
    db = collect(src)
    ids = [s.strip() for s in args.ids.split(",") if s.strip()]

    # 数码蛋：用 VB 的 digitama.gif（单帧 -> 当两帧用）
    egg = os.path.abspath(args.egg) if args.egg else ""
    if egg and os.path.exists(egg):
        db["egggggg"] = {"name": "Digitama", "stage": "0", "attr": "Free",
                         "stats": {}, "f1": egg, "f2": egg}
        if "egggggg" not in ids:
            ids.append("egggggg")
    else:
        print("  注意: 没找到蛋素材", egg)
    print(f"库内 {len(db)} 只，导入 {len(ids)} 只: {ids}")

    hdr, js = [], []
    hdr.append("// AUTO-GENERATED by tools/import_vb.py - do not edit")
    hdr.append("// 数据来源: digimon-assets-VB (Bandai 版权，仅个人非商业使用)")
    hdr.append("#pragma once")
    hdr.append('#include "../gfx/anim_types.h"')
    hdr.append("")
    js.append("// AUTO-GENERATED by tools/import_vb.py")
    js.append("window.DIGI_VB = window.DIGI_VB || {};")

    tiles = []
    for cid in ids:
        if cid not in db:
            print(f"  跳过 {cid}: 库里没有")
            continue
        info = db[cid]
        enum = CHAR_NAMES.get(cid, "VB_" + cid.upper())
        a, b = load_pair(info)
        anims = build_anims(a, b)

        all_frames = []
        for name in ("idle", "attack", "hurt", "win"):
            frames, _ = anims[name]
            all_frames.extend(frames)
        pal, rows = quantize(all_frames)

        pal_str = ", ".join(f"0x{rgb565(c):04X}" for c in pal)
        hdr.append(f"static const uint16_t AVB_{enum}[16] = {{ {pal_str} }};")
        hdr.append("")

        pos = 0
        js_anims = []
        for name in ("idle", "attack", "hurt", "win"):
            frames, durs = anims[name]
            n = len(frames)
            chunk = rows[pos:pos + n]
            pos += n
            up = name.upper()
            for i, fr in enumerate(chunk):
                hdr.append(f"static const char* const {enum}_{up}_{i}[] = {{")
                for r in fr:
                    hdr.append(f'    "{r}",')
                hdr.append("};")
            hdr.append(f"static const SpriteDef {enum}_{up}_F[{n}] = {{")
            for i in range(n):
                hdr.append(f"    {{ {CW}, {CH}, {enum}_{up}_{i}, AVB_{enum} }},")
            hdr.append("};")
            hdr.append(f"static const uint16_t {enum}_{up}_D[{n}] = {{ {', '.join(str(d) for d in durs)} }};")
            hdr.append(f"static const AnimClip {enum}_{up} = {{ {n}, {enum}_{up}_F, {enum}_{up}_D }};")
            hdr.append("")
            js_anims.append((name, chunk, durs))

        hdr.append(f"static const CharAnimSet {enum}_SET = "
                   f"{{ &{enum}_IDLE, &{enum}_ATTACK, &{enum}_HURT, &{enum}_WIN }};")
        hdr.append("")

        js.append(f'DIGI_VB["{cid}"] = {{')
        js.append(f'  pal: [{pal_str}], name: "{info["name"]}", stage: "{info["stage"]}",')
        js.append('  anims: {')
        for name, chunk, durs in js_anims:
            js.append(f'    {name}: {{ dur: [{",".join(str(d) for d in durs)}], frames: [')
            for fr in chunk:
                js.append('      [' + ",".join(f'"{r}"' for r in fr) + "],")
            js.append("    ]},")
        js.append("  }")
        js.append("};")
        print(f"  {cid:14s} {info['name']:24s} {info['stage']:3s} 帧数 {len(all_frames)} {a.size}")
        tiles.append((info, a))

    os.makedirs(os.path.dirname(args.out_h), exist_ok=True)
    with open(args.out_h, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(hdr))
    with open(args.out_js, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(js))

    # 预览图
    if tiles:
        S = 5
        cols = len(tiles)
        sheet = Image.new("RGB", (cols * (CW * S + 10) + 10, CH * S + 34), (12, 16, 22))
        dr = ImageDraw.Draw(sheet)
        for i, (info, im) in enumerate(tiles):
            bg = Image.new("RGB", (CW, CH), (12, 16, 22))
            x = (CW - im.width) // 2
            bg.paste(im, (max(0, x), CH - im.height), im)
            sheet.paste(bg.resize((CW * S, CH * S), Image.NEAREST), (10 + i * (CW * S + 10), 10))
            dr.text((12 + i * (CW * S + 10), CH * S + 16),
                    f'{info["name"]} {info["stage"]}', fill=(200, 210, 230))
        sheet.save(args.preview)

    print("wrote", args.out_h)
    print("wrote", args.out_js)
    if tiles:
        print("wrote", args.preview)


if __name__ == "__main__":
    main()
