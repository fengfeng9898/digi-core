"""把 test_bg.js 导出的完整画面拼成预览 PNG。"""
import json
import os

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "scene_frames.json")
OUT = os.path.join(ROOT, "docs", "scene_preview.png")
S = 5

with open(SRC, encoding="utf-8") as f:
    frames = json.load(f)


def to_img(flat):
    img = Image.new("RGB", (64, 64))
    px = img.load()
    for y in range(64):
        for x in range(64):
            i = (y * 64 + x) * 3
            px[x, y] = (flat[i], flat[i + 1], flat[i + 2])
    return img


cols = len(frames)
W = cols * (64 * S + 10) + 10
H = 64 * S + 30
out = Image.new("RGB", (W, H), (10, 12, 16))
dr = ImageDraw.Draw(out)
for i, fr in enumerate(frames):
    out.paste(to_img(fr["px"]).resize((64 * S, 64 * S), Image.NEAREST), (10 + i * (64 * S + 10), 10))
    dr.text((10 + i * (64 * S + 10), 10 + 64 * S + 4), fr["label"], fill=(150, 165, 180))

out.save(OUT)
print("wrote", OUT, out.size)
