"""把固件里的动画帧按 64x64 实屏布局渲染出来，检查构图/尺寸。"""
import re
import sys

from PIL import Image, ImageDraw

sys.stdout.reconfigure(encoding="utf-8")

ROOT = "src/assets"
PANEL = 64
SCALE = 6


def load(path):
    return open(path, encoding="utf-8").read()


def parse_pal(txt, name):
    m = re.search(r"APAL_%s\[16\] = \{([^}]*)\}" % name, txt)
    if not m:
        return [(0, 0, 0)] * 16
    vals = [int(v.strip(), 16) for v in m.group(1).split(",") if v.strip()]
    out = []
    for n in vals:
        r = ((n >> 11) & 0x1F) * 255 // 31
        g = ((n >> 5) & 0x3F) * 255 // 63
        b = (n & 0x1F) * 255 // 31
        out.append((r, g, b))
    return out


def parse_frame(txt, name):
    m = re.search(r"SPR_%s\[\] = \{(.*?)\};" % name, txt, re.S)
    return re.findall(r'"([0-9A-F]+)"', m.group(1))


def draw_screen(pal, rows, label):
    img = Image.new("RGB", (PANEL, PANEL), (0, 0, 0))
    px = img.load()

    # 状态栏：4 颗心（左上）+ 心情条（右上）
    for i in range(4):
        x0 = 1 + i * 6
        for row in range(5):
            for col in range(5):
                if [0b01010, 0b11111, 0b11111, 0b01110, 0b00100][row] & (0x10 >> col):
                    px[x0 + col, 1 + row] = (248, 0, 0)
    for x in range(29, 62):
        px[x, 2] = (40, 40, 40)
        px[x, 6] = (40, 40, 40)
    for y in range(2, 7):
        px[29, y] = (40, 40, 40)
        px[61, y] = (40, 40, 40)
    for x in range(30, 55):
        for y in range(3, 6):
            px[x, y] = (63, 185, 80)

    # 动画帧：56x40，中心 (32,29)
    w, h = len(rows[0]), len(rows)
    ox, oy = PANEL // 2 - w // 2, 29 - h // 2
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            idx = int(ch, 16)
            if idx:
                px[ox + x, oy + y] = pal[idx]

    # 底部文字占位
    for x in range(8, 56):
        px[x, 59] = (120, 120, 120)

    d = ImageDraw.Draw(img)
    d.text((2, 66), label, fill=(140, 150, 160))
    return img


def main():
    png = load(f"{ROOT}/anim_data.h")
    shots = []
    for char, frame, label in [
        ("KOROMON", "KOROMON_IDLE_0", "KOROMON 幼年期"),
        ("AGUMON", "AGUMON_IDLE_0", "AGUMON 成长期"),
        ("GREYMON", "GREYMON_IDLE_6", "GREYMON 成熟期"),
        ("AGUMON", "AGUMON_ATTACK_6", "AGUMON 攻击"),
        ("GREYMON", "GREYMON_ATTACK_7", "GREYMON 攻击"),
        ("KOROMON", "KOROMON_HURT_4", "KOROMON 受伤"),
    ]:
        pal = parse_pal(png, char)
        rows = parse_frame(png, frame)
        shots.append(draw_screen(pal, rows, label))

    W = (PANEL * SCALE + 12) * len(shots)
    canvas = Image.new("RGB", (W, PANEL * SCALE + 22), (8, 10, 14))
    x = 0
    for s in shots:
        canvas.paste(s.resize((PANEL * SCALE, PANEL * SCALE), Image.NEAREST), (x, 0))
        x += PANEL * SCALE + 12
    canvas.save("docs/screen_mock.png")
    print("docs/screen_mock.png", canvas.size)


if __name__ == "__main__":
    main()
