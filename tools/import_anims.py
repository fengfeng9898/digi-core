#!/usr/bin/env python3
"""
import_anims.py — 把角色的 GIF 动画包转成固件动画数据

输入:  Downloads 下的 zip（内含 GIF，可能有嵌套 zip）
输出:  src/assets/anim_data.h     动画帧（索引色 ASCII，和 sprites.h 同格式）
       docs/anim_preview/*.png    预览图（4 倍放大，用于肉眼检查）

处理要点:
  1. 同一角色的所有动画共用一套缩放和锚点，否则待机和攻击会「跳尺寸」。
  2. 缩放基准取「待机」包围盒高度，宽度超出画布的攻击特效会被裁掉（可接受）。
  3. 每个角色单独量化到 15 色（0 号恒为透明），避免多角色共用调色板导致偏色。

用法: python tools/import_anims.py
"""
from __future__ import annotations

import io
import os
import zipfile

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT_H = os.path.join(ROOT, "src", "assets", "anim_data.h")
PREVIEW = os.path.join(ROOT, "docs", "anim_preview")

ZIPS = [
    r"C:\Users\Administrator\Downloads\滚球兽.zip",
    r"C:\Users\Administrator\Downloads\亚古兽.zip",
    r"C:\Users\Administrator\Downloads\暴龙兽.zip",
]

CANVAS_W, CANVAS_H = 64, 40

CHARS = [
    # key, 中文, 目标身高(px), 静态精灵 fallback
    ("KOROMON", "滚球兽", 30),
    ("AGUMON", "亚古兽", 34),
    ("GREYMON", "暴龙兽", 40),
    ("METALGREYMON", "机械暴龙兽", 40),
]

ANIM_KEYS = [("待机", "IDLE"), ("攻击", "ATTACK"), ("受伤", "HURT"), ("胜利", "WIN")]
ANIM_ORDER = ["IDLE", "ATTACK", "HURT", "WIN"]


def detect_char(name: str) -> str | None:
    if "机械暴龙兽" in name:
        return "METALGREYMON"
    if "滚球兽" in name:
        return "KOROMON"
    if "亚古兽" in name:
        return "AGUMON"
    if "暴龙兽" in name:
        return "GREYMON"
    return None


def detect_anim(name: str) -> str | None:
    for cn, en in ANIM_KEYS:
        if cn in name:
            return en
    return None


def load_gif_frames(data: bytes):
    im = Image.open(io.BytesIO(data))
    frames, durs = [], []
    try:
        while True:
            frames.append(im.convert("RGBA"))
            durs.append(max(20, im.info.get("duration", 100)))
            im.seek(im.tell() + 1)
    except EOFError:
        pass
    return frames, durs


def crop_and_scale(f: Image.Image, x0: float, y0: float, win_w: float, win_h: float) -> Image.Image:
    """按源图坐标裁窗口（超出部分补透明），再缩到画布尺寸。"""
    cw, ch = int(np.ceil(win_w)), int(np.ceil(win_h))
    canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    canvas.paste(f, (int(round(-x0)), int(round(-y0))))
    return canvas.resize((CANVAS_W, CANVAS_H), Image.LANCZOS)


def collect_from_zip(zf: zipfile.ZipFile, default_char: str | None, out: dict):
    for info in zf.infolist():
        if info.is_dir():
            continue
        base = os.path.basename(info.filename)
        data = zf.read(info.filename)

        if base.lower().endswith(".zip"):
            inner_char = detect_char(base) or default_char
            with zipfile.ZipFile(io.BytesIO(data)) as iz:
                collect_from_zip(iz, inner_char, out)
            continue

        if not base.lower().endswith(".gif"):
            continue

        ch = detect_char(base) or default_char
        an = detect_anim(base)
        if not ch or not an:
            print(f"  [skip] 无法识别: {base} (char={ch} anim={an})")
            continue
        frames, durs = load_gif_frames(data)
        out.setdefault(ch, {})[an] = (frames, durs)
        print(f"  {ch:14s} {an:6s} frames={len(frames):2d} dur={durs[0]}ms")


def bbox_union(frames):
    box = None
    for f in frames:
        bb = f.getbbox()
        if bb:
            box = bb if box is None else (min(box[0], bb[0]), min(box[1], bb[1]),
                                          max(box[2], bb[2]), max(box[3], bb[3]))
    return box


def quantize_palette(pixels: np.ndarray, k: int = 15, iters: int = 14, seed: int = 7):
    rng = np.random.default_rng(seed)
    if len(pixels) > 24000:
        idx = rng.choice(len(pixels), 24000, replace=False)
        sample = pixels[idx]
    else:
        sample = pixels
    # k-means++ 简化：随机取 k 个样本
    cent = sample[rng.choice(len(sample), k, replace=False)].astype(np.float32)
    for _ in range(iters):
        d = ((sample[:, None, :] - cent[None, :, :]) ** 2).sum(axis=2)
        lab = d.argmin(axis=1)
        for j in range(k):
            m = lab == j
            if m.any():
                cent[j] = sample[m].mean(axis=0)
    return cent


def nearest_index(rgb: np.ndarray, cent: np.ndarray) -> np.ndarray:
    flat = rgb.reshape(-1, 3).astype(np.float32)
    out = np.zeros(len(flat), dtype=np.uint8)
    for i in range(0, len(flat), 4096):
        chunk = flat[i:i + 4096]
        d = ((chunk[:, None, :] - cent[None, :, :]) ** 2).sum(axis=2)
        out[i:i + 4096] = d.argmin(axis=1)
    return out.reshape(rgb.shape[:2])


def hex_rows(idx: np.ndarray) -> list[str]:
    hx = "0123456789ABCDEF"
    return ["".join(hx[v] for v in row) for row in idx]


def main():
    raw: dict[str, dict] = {}
    for path in ZIPS:
        print("=" * 60)
        print(os.path.basename(path))
        with zipfile.ZipFile(path) as z:
            collect_from_zip(z, detect_char(os.path.basename(path)), raw)

    # 只保留四组齐全的角色
    ready = {}
    for key, cn, _ in CHARS:
        if key not in raw:
            print(f"[warn] {cn} 没有素材")
            continue
        missing = [a for a in ANIM_ORDER if a not in raw[key]]
        if missing:
            print(f"[warn] {cn} 缺动画: {missing}")
            continue
        ready[key] = raw[key]

    os.makedirs(PREVIEW, exist_ok=True)
    os.makedirs(os.path.dirname(OUT_H), exist_ok=True)
    lines: list[str] = [
        "// AUTO-GENERATED by tools/import_anims.py - do not edit by hand.",
        "#pragma once",
        "#include <stdint.h>",
        '#include "../gfx/anim_types.h"',
        "",
    ]

    stats = []
    for key, cn, target_h in CHARS:
        if key not in ready:
            continue
        anims = ready[key]

        # ---- 统一缩放：以待机包围盒高度为准 ----
        idle_box = bbox_union(anims["IDLE"][0])
        bw = idle_box[2] - idle_box[0]
        bh = idle_box[3] - idle_box[1]
        scale = target_h / float(bh)
        anchor_x = (idle_box[0] + idle_box[2]) / 2.0
        anchor_y = idle_box[3]
        # 画布在源图里的窗口
        win_w = CANVAS_W / scale
        win_h = CANVAS_H / scale
        x0 = anchor_x - win_w / 2.0
        y0 = anchor_y - win_h

        # ---- 光栅化所有帧 ----
        rasters = {}   # (anim, i) -> (rgba array HxW, dur)
        all_px = []
        for an in ANIM_ORDER:
            frames, durs = anims[an]
            for i, f in enumerate(frames):
                crop = crop_and_scale(f, x0, y0, win_w, win_h)
                arr = np.array(crop)
                alpha = arr[:, :, 3]
                rgb = arr[:, :, :3]
                rasters[(an, i)] = (rgb, alpha, durs[i])
                m = alpha >= 128
                if m.any():
                    all_px.append(rgb[m])

        pixels = np.concatenate(all_px, axis=0)
        cent = quantize_palette(pixels, 15)

        # ---- 量化 + 生成 ----
        per_anim = {}
        for an in ANIM_ORDER:
            frames, durs = anims[an]
            sprites, durations = [], []
            for i in range(len(frames)):
                rgb, alpha, dur = rasters[(an, i)]
                idx = nearest_index(rgb, cent).astype(np.uint8) + 1   # 0 是透明
                idx[alpha < 128] = 0
                name = f"{key}_{an}_{i}"
                rows = hex_rows(idx)
                lines.append(f"static const char* const SPR_{name}[] = {{")
                for r in rows:
                    lines.append(f'    "{r}",')
                lines.append("};")
                sprites.append(name)
                durations.append(dur)
            per_anim[an] = (sprites, durations)

        # 调色板
        pal_vals = ["0x0000"] + [f"0x{((int(c[0]) >> 3) << 11) | ((int(c[1]) >> 2) << 5) | (int(c[2]) >> 3):04X}"
                                 for c in cent]
        lines.append(f"static const uint16_t APAL_{key}[16] = {{ {', '.join(pal_vals)} }};")
        lines.append("")

        for an in ANIM_ORDER:
            sprites, durations = per_anim[an]
            lines.append(f"static const SpriteDef {key}_{an}_F[] = {{")
            for s in sprites:
                lines.append(f"    {{ {CANVAS_W}, {CANVAS_H}, SPR_{s}, APAL_{key} }},")
            lines.append("};")
            lines.append(f"static const uint16_t {key}_{an}_D[] = {{ "
                         + ", ".join(str(d) for d in durations) + " };")
            lines.append(f"static const AnimClip {key}_{an} = {{ "
                         f"{len(sprites)}, {key}_{an}_F, {key}_{an}_D }};")
            lines.append("")

        stats.append((key, cn, sum(len(per_anim[a][0]) for a in ANIM_ORDER)))

        # ---- 预览 ----
        strip = Image.new("RGB", (len(anims["IDLE"][0]) * CANVAS_W, CANVAS_H * 4), (10, 12, 16))
        for ai, an in enumerate(ANIM_ORDER):
            row = Image.new("RGB", (len(anims[an][0]) * CANVAS_W, CANVAS_H), (10, 12, 16))
            for i in range(len(anims[an][0])):
                rgb, alpha, _ = rasters[(an, i)]
                idx = nearest_index(rgb, cent).astype(np.uint8) + 1
                idx[alpha < 128] = 0
                pal = np.array([[0, 0, 0]] + [c for c in cent], dtype=np.uint8)
                tile = Image.fromarray(pal[idx])
                row.paste(tile, (i * CANVAS_W, 0))
            strip.paste(row, (0, ai * CANVAS_H))
        strip = strip.resize((strip.width * 2, strip.height * 2), Image.NEAREST)
        strip.save(os.path.join(PREVIEW, f"{key}.png"))
        print(f"[preview] {key}.png  {strip.width}x{strip.height}")

    # 角色表（带 CharId，缺素材的角色直接不出现）
    present = [k for k, _, _ in CHARS if k in ready]
    lines.append("static const CharAnimEntry CHAR_ANIMS[] = {")
    for key in present:
        lines.append(
            f"    {{ CH_{key}, {{ &{key}_IDLE, &{key}_ATTACK, &{key}_HURT, &{key}_WIN }} }},"
        )
    lines.append("};")
    lines.append("")

    with open(OUT_H, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    print("\n输出:", OUT_H, f"({os.path.getsize(OUT_H)/1024:.0f} KB)")
    for key, cn, n in stats:
        print(f"  {cn:8s} {n:3d} 帧")


if __name__ == "__main__":
    main()
