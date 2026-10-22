import io
import os
import re
import zipfile

import numpy as np
from PIL import Image, ImageDraw

ZIP = r"C:\Users\Administrator\Downloads\DigiviceColorModifier.zip"
OUT = r"C:\Users\Administrator\deepseekwork\esp32-digimon\docs\toy16.png"
TMP = r"C:\Users\Administrator\deepseekwork\toy16"
os.makedirs(TMP, exist_ok=True)

GROUPS = {487: "GREYMON?", 121: "?", 96: "?"}
SEL = []
with zipfile.ZipFile(ZIP) as z:
    for e in z.namelist():
        if "__MACOSX" in e or "/d3_color_sprites/" not in e or not e.endswith(".png"):
            continue
        m = re.search(r"/(\d+)_(\d+)_(\d+)\.png$", e)
        if m and int(m.group(1)) in GROUPS:
            SEL.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), e))
    print("命中帧:", len(SEL))
    rows = []
    for gid, label in GROUPS.items():
        frames = sorted([s for s in SEL if s[0] == gid], key=lambda s: (s[1], s[2]))[:16]
        strip = []
        for _, _, _, name in frames:
            data = z.read(name)
            im = Image.open(io.BytesIO(data)).convert("RGB")
            a = np.array(im)
            cols, cnt = np.unique(a.reshape(-1, 3), axis=0, return_counts=True)
            bg = cols[cnt.argmax()]
            mask = (np.abs(a.astype(int) - bg).sum(axis=2) > 12)
            if mask.sum() < 40:
                continue
            ys, xs = np.where(mask)
            rgba = im.convert("RGBA")
            rgba.putalpha(Image.fromarray(mask.astype(np.uint8) * 255))
            c = rgba.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
            s = 46 / c.height
            strip.append(c.resize((max(1, round(c.width * s)), 46), Image.LANCZOS))
        rows.append((label, frames[0][0] if frames else gid, strip))

W = 16 * 50 + 10
sheet = Image.new("RGB", (W, len(rows) * 76 + 10), (12, 16, 22))
d = ImageDraw.Draw(sheet)
for i, (label, gid, strip) in enumerate(rows):
    y = 10 + i * 76
    d.text((10, y), f"id={gid} {label}  {len(strip)} 帧", fill=(200, 210, 230))
    for j, im in enumerate(strip):
        sheet.paste(im, (10 + j * 50, y + 22), im)
sheet.save(OUT)
print("wrote", OUT)
