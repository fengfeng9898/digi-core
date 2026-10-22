#!/usr/bin/env python3
"""生成 App 图标（mipmap-xhdpi/ic_launcher.png）：夜空底 + 青色数码蛋。"""
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "android", "app", "src", "main", "res", "mipmap-xhdpi")
S = 192

os.makedirs(OUT, exist_ok=True)
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# 圆角底
d.rounded_rectangle([0, 0, S - 1, S - 1], radius=42, fill=(11, 16, 32, 255))
# 一点星空
for (x, y, r) in [(38, 40, 2), (150, 52, 2), (64, 150, 2), (140, 140, 3)]:
    d.ellipse([x - r, y - r, x + r, y + r], fill=(120, 150, 220, 255))

# 蛋：椭圆 + 高光 + 波点
egg = [S // 2 - 45, 42, S // 2 + 45, 156]
d.ellipse(egg, fill=(60, 220, 255, 255))
d.ellipse([S // 2 - 30, 60, S // 2 - 4, 96], fill=(200, 250, 255, 180))
for (x, y, r) in [(S // 2 + 14, 108, 7), (S // 2 - 22, 126, 5), (S // 2 + 24, 76, 4)]:
    d.ellipse([x - r, y - r, x + r, y + r], fill=(18, 90, 130, 255))

img.save(os.path.join(OUT, "ic_launcher.png"))
print("wrote", os.path.join(OUT, "ic_launcher.png"), f"{S}x{S}")
