#!/usr/bin/env python3
"""vb_probe.py — 从 digimon-assets-VB 里取样几只，缩到面板尺寸看效果。

用法: python tools/vb_probe.py <vb-assets 目录> <输出 png>
"""
import json
import os
import sys

from PIL import Image, ImageDraw

CANVAS_W, CANVAS_H = 64, 40     # 面板角色区
TARGET_H = 40
COLORS = 15

PICKS = ["agu", "grey", "metalgrey_va", "wargrey", "koromon", "betamon",
         "gabu", "patamon", "tailmon", "angewomon", "devimon", "omegamon"]


def collect(root):
    found = {}
    for dj in sorted(__import__("glob").glob(os.path.join(root, "*", "*", "dim.json"))):
        d = json.load(open(dj, encoding="utf-8"))
        base = os.path.dirname(dj)
        for m in d.get("digimon", []):
            sp = m.get("sprites", {})
            if not sp.get("frame1") or m["id"] in found:
                continue
            f1 = os.path.join(base, sp["frame1"])
            f2 = os.path.join(base, sp.get("frame2", "")) if sp.get("frame2") else None
            found[m["id"]] = {
                "name": m.get("name", m["id"]),
                "stage": m.get("stage", ""),
                "f1": f1, "f2": f2 if f2 and os.path.exists(f2) else None,
            }
    return found


def bbox_union(imgs):
    box = None
    for im in imgs:
        b = im.getbbox()
        if not b:
            continue
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]),
                                     max(box[2], b[2]), max(box[3], b[3]))
    return box


def prep(paths, scale_h=TARGET_H):
    """裁到共同包围盒，缩放到高度 scale_h，返回 RGBA 列表"""
    imgs = [Image.open(p).convert("RGBA") for p in paths]
    box = bbox_union(imgs)
    if not box:
        return []
    out = []
    for im in imgs:
        c = im.crop(box)
        s = scale_h / c.height
        w = max(1, round(c.width * s))
        out.append(c.resize((w, scale_h), Image.LANCZOS))
    return out


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "vb-assets/assets/data"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "docs/vb_probe.png"

    db = collect(root)
    picks = [(k, db[k]) for k in PICKS if k in db]
    print(f"库里有 {len(db)} 只；取样 {len(picks)} 只")

    tiles = []
    for k, info in picks:
        imgs = prep([info["f1"]] + ([info["f2"]] if info["f2"] else []))
        if not imgs:
            continue
        tiles.append((f'{info["name"]} ({info["stage"]})', imgs[0]))
        print(f'  {k:16s} {info["name"]:26s} {info["stage"]:3s} {imgs[0].size}')

    S = 5
    cols = 4
    rows = (len(tiles) + cols - 1) // cols
    cell_w, cell_h = CANVAS_W * S + 10, CANVAS_H * S + 26
    sheet = Image.new("RGB", (cols * cell_w + 10, rows * cell_h + 10), (12, 16, 22))
    dr = ImageDraw.Draw(sheet)

    for i, (label, im) in enumerate(tiles):
        cx = 10 + (i % cols) * cell_w
        cy = 10 + (i // cols) * cell_h
        bg = Image.new("RGB", (CANVAS_W, CANVAS_H), (12, 16, 22))
        # 水平居中、底部对齐
        x = (CANVAS_W - im.width) // 2
        y = CANVAS_H - im.height
        bg.paste(im, (x, y), im)
        sheet.paste(bg.resize((CANVAS_W * S, CANVAS_H * S), Image.NEAREST), (cx, cy))
        dr.text((cx + 2, cy + CANVAS_H * S + 4), label[:26], fill=(200, 210, 230))

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    sheet.save(out_path)
    print("wrote", out_path)


if __name__ == "__main__":
    main()
