"""把 test_battle.js 导出的战斗逐帧数据拼成 PNG。"""
import json
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "docs", "battle_frames.json")
OUT = os.path.join(ROOT, "docs", "battle_preview.png")
S = 5
GAP = 6

with open(DATA, encoding="utf-8") as f:
    d = json.load(f)


def frame_to_img(flat):
    img = Image.new("RGB", (64, 64))
    px = img.load()
    for y in range(64):
        for x in range(64):
            i = (y * 64 + x) * 3
            px[x, y] = (flat[i], flat[i + 1], flat[i + 2])
    return img


LANES = [("win", "胜利"), ("lose", "失败"), ("card", "战斗卡（临时进化）")]
rows = [(label, d[key]) for key, label in LANES if key in d]
cols = max(len(v) for _, v in rows)
W = cols * (64 * S + GAP) + GAP
H = len(rows) * (64 * S + 28) + 10
out = Image.new("RGB", (W, H), (10, 12, 16))
dr = ImageDraw.Draw(out)

for r, (label, frames) in enumerate(rows):
    y = 10 + r * (64 * S + 28)
    dr.text((GAP, y - 2), label, fill=(150, 165, 180))
    for c, flat in enumerate(frames):
        img = frame_to_img(flat).resize((64 * S, 64 * S), Image.NEAREST)
        out.paste(img, (GAP + c * (64 * S + GAP), y + 14))

out.save(OUT)
print("wrote", OUT, out.size)
