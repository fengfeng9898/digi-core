import io
import os
import zipfile

from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "assets_probe")
os.makedirs(OUT, exist_ok=True)

ZIPS = [
    r"C:\Users\Administrator\Downloads\滚球兽.zip",
    r"C:\Users\Administrator\Downloads\亚古兽.zip",
    r"C:\Users\Administrator\Downloads\暴龙兽.zip",
]


def probe_gif(name, data):
    im = Image.open(io.BytesIO(data))
    frames = []
    try:
        while True:
            frames.append(im.convert("RGBA"))
            im.seek(im.tell() + 1)
    except EOFError:
        pass
    durs = []
    im.seek(0)
    for i in range(len(frames)):
        im.seek(i)
        durs.append(im.info.get("duration", 0))

    # 所有帧的并集包围盒
    union = None
    for f in frames:
        bb = f.getbbox()
        if bb:
            union = bb if union is None else (
                min(union[0], bb[0]), min(union[1], bb[1]),
                max(union[2], bb[2]), max(union[3], bb[3]))

    print(f"  {name}")
    print(f"    frames={len(frames)} durations(ms)={durs} canvas={frames[0].size} bbox={union}")
    if frames:
        w = union[2] - union[0]
        h = union[3] - union[1]
        print(f"    content={w}x{h}")

    # 拼一张横向 contact sheet（每帧裁到 bbox，等比缩放到 128 高）
    if frames and union:
        thumbs = []
        for f in frames:
            c = f.crop(union)
            r = 128 / c.height
            thumbs.append(c.resize((max(1, int(c.width * r)), 128), Image.LANCZOS))
        sheet = Image.new("RGBA", (sum(t.width for t in thumbs) + 4 * len(thumbs), 132), (20, 24, 30, 255))
        x = 0
        for t in thumbs:
            sheet.alpha_composite(t, (x, 2))
            x += t.width + 4
        safe = "".join(ch if ch.isalnum() else "_" for ch in os.path.splitext(name)[0])
        sheet.convert("RGB").save(os.path.join(OUT, safe + ".png"))
    return len(frames)


for path in ZIPS:
    print("=" * 70)
    z = zipfile.ZipFile(path)
    for info in z.infolist():
        if info.is_dir():
            continue
        data = z.read(info.filename)
        base = os.path.basename(info.filename)
        if base.lower().endswith(".zip"):
            print(f"  [nested zip] {base} ({len(data)} bytes)")
            inner = zipfile.ZipFile(io.BytesIO(data))
            for ii in inner.infolist():
                if ii.is_dir():
                    continue
                d = inner.read(ii.filename)
                probe_gif(os.path.basename(ii.filename), d)
        elif base.lower().endswith(".gif"):
            probe_gif(base, data)

print("\ncontact sheets ->", os.path.abspath(OUT))
