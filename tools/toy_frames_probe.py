"""把 D-3 玩具拆帧的 384x384 16 色帧，转成面板尺寸并拼成 GIF/预览。"""
import glob
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

SRC = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\Administrator\deepseekwork\toy_anim\121"
OUT = sys.argv[2] if len(sys.argv) > 2 else r"C:\Users\Administrator\deepseekwork\esp32-digimon\docs\toy_anim.png"
TARGET_H = 40

files = sorted(glob.glob(os.path.join(SRC, "*.png")),
               key=lambda p: int(os.path.basename(p).split("_")[2].split(".")[0]))
print(f"帧数: {len(files)}")

frames = []
for p in files:
    im = Image.open(p).convert("RGB")
    a = np.array(im)
    colors, counts = np.unique(a.reshape(-1, 3), axis=0, return_counts=True)
    bg = colors[counts.argmax()]          # 出现最多的颜色当背景
    mask = (np.abs(a.astype(int) - bg).sum(axis=2) > 12)
    if mask.sum() < 30:                    # 空白帧
        continue
    ys, xs = np.where(mask)
    box = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    rgba = im.convert("RGBA")
    rgba.putalpha(Image.fromarray(mask.astype(np.uint8) * 255))
    crop = rgba.crop(box)
    s = TARGET_H / crop.height
    w = max(1, round(crop.width * s))
    frames.append(crop.resize((w, TARGET_H), Image.LANCZOS))

print(f"有效帧: {len(frames)}  尺寸: {[f.size for f in frames][:5]}")

if frames:
    W = max(f.width for f in frames) + 6
    canvas = [Image.new("RGBA", (W, TARGET_H + 2), (0, 0, 0, 0)) for _ in frames]
    for c, f in zip(canvas, frames):
        c.alpha_composite(f, ((W - f.width) // 2, 2))

    # GIF（4 倍）
    SC = 4
    gif_frames = []
    for c in canvas:
        bg = Image.new("RGB", (W, TARGET_H + 2), (12, 16, 22))
        bg.paste(c, (0, 0), c)
        gif_frames.append(bg.resize((W * SC, (TARGET_H + 2) * SC), Image.NEAREST))
    gif_path = OUT.replace(".png", ".gif")
    gif_frames[0].save(gif_path, save_all=True, append_images=gif_frames[1:],
                       duration=110, loop=0, optimize=False)
    print("wrote", gif_path)

    # 帧条预览
    S = 5
    cols = min(len(canvas), 16)
    cell = W * S + 6
    sheet = Image.new("RGB", (cols * cell + 6, (TARGET_H + 2) * S + 26), (12, 16, 22))
    d = ImageDraw.Draw(sheet)
    for i, c in enumerate(canvas[:cols]):
        bg = Image.new("RGB", (W, TARGET_H + 2), (12, 16, 22))
        bg.paste(c, (0, 0), c)
        sheet.paste(bg.resize((W * S, (TARGET_H + 2) * S), Image.NEAREST), (6 + i * cell, 6))
    d.text((8, (TARGET_H + 2) * S + 8), f"{len(frames)} frames", fill=(200, 210, 230))
    sheet.save(OUT)
    print("wrote", OUT)
