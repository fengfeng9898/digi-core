#!/usr/bin/env python3
"""
gen_sprites.py — 程序化像素精灵生成器 → src/assets/sprites.h

第一条角色链（亚古兽系）:
    蛋 → 滚球兽 → 亚古兽 → 暴龙兽 → 机械暴龙兽 → 战斗暴龙兽

做法: 用 SDF(有符号距离场) 拼椭圆/三角/矩形, 输出 16 色索引 ASCII 图,
固件 blitter 逐像素解码。后续换成手绘 PNG 时, 只要保持同样的输出格式。

用法: python tools/gen_sprites.py
"""
from __future__ import annotations

import math
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "src", "assets", "sprites.h")

# ---------------------------------------------------------------- palette ----
# 索引 0 恒为透明。1=描边, 2=主体, 3=亮部, 4=暗部, 5=白, 6=黑, 7/8/9=强调色
PAL = {
    "EGG": [(0, 0, 0), (30, 22, 18), (246, 238, 220), (255, 252, 240), (214, 200, 178),
            (255, 255, 255), (226, 122, 40), (246, 160, 60), (96, 150, 220), (0, 0, 0)],
    "KOROMON": [(0, 0, 0), (20, 16, 26), (48, 42, 62), (74, 66, 92), (30, 25, 40),
                (250, 250, 255), (18, 14, 24), (236, 64, 96), (255, 140, 160), (0, 0, 0)],
    "AGUMON": [(0, 0, 0), (36, 20, 10), (242, 152, 52), (255, 196, 108), (196, 108, 30),
               (255, 252, 240), (30, 20, 14), (96, 208, 120), (120, 78, 40), (0, 0, 0)],
    "GREYMON": [(0, 0, 0), (34, 20, 12), (196, 122, 66), (232, 168, 110), (146, 84, 44),
                (250, 240, 214), (28, 18, 12), (214, 64, 52), (86, 140, 220), (0, 0, 0)],
    "METALGREYMON": [(0, 0, 0), (22, 22, 28), (168, 104, 58), (206, 142, 88), (120, 72, 40),
                     (236, 240, 248), (18, 18, 24), (206, 62, 50), (150, 160, 178), (206, 214, 228)],
    "WARGREYMON": [(0, 0, 0), (26, 20, 10), (232, 178, 44), (255, 216, 108), (176, 126, 26),
                   (248, 250, 255), (24, 18, 10), (176, 54, 46), (188, 196, 214), (255, 255, 255)],
    "DEAD": [(0, 0, 0), (28, 26, 32), (86, 84, 92), (116, 112, 122), (58, 56, 64),
             (196, 196, 204), (24, 22, 30), (120, 40, 40), (0, 0, 0), (0, 0, 0)],
}


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


class Grid:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[0] * w for _ in range(h)]

    def inside(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h

    def get(self, x, y):
        return self.px[y][x] if self.inside(x, y) else 0

    def set(self, x, y, v):
        if self.inside(x, y):
            self.px[y][x] = v

    def paint(self, test, color):
        for y in range(self.h):
            for x in range(self.w):
                if test(x + 0.5, y + 0.5):
                    self.px[y][x] = color

    def outline(self, color=1):
        solid = [(x, y) for y in range(self.h) for x in range(self.w) if self.px[y][x] != 0]
        for x, y in solid:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if self.inside(nx, ny) and self.px[ny][nx] == 0:
                    self.px[ny][nx] = color

    def rows(self):
        hexd = "0123456789ABCDEF"
        return ["".join(hexd[v] for v in row) for row in self.px]


# ------------------------------------------------------------------ 图元 ----
def ellipse(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def circle(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def rot_ellipse(cx, cy, rx, ry, ang):
    ca, sa = math.cos(ang), math.sin(ang)

    def test(x, y):
        dx, dy = x - cx, y - cy
        u = dx * ca + dy * sa
        v = -dx * sa + dy * ca
        return (u / rx) ** 2 + (v / ry) ** 2 <= 1.0

    return test


def tri(p0, p1, p2):
    def test(x, y):
        d1 = (x - p1[0]) * (p0[1] - p1[1]) - (p0[0] - p1[0]) * (y - p1[1])
        d2 = (x - p2[0]) * (p1[1] - p2[1]) - (p1[0] - p2[0]) * (y - p2[1])
        d3 = (x - p0[0]) * (p2[1] - p0[1]) - (p2[0] - p0[0]) * (y - p0[1])
        neg = d1 < 0 or d2 < 0 or d3 < 0
        pos = d1 > 0 or d2 > 0 or d3 > 0
        return not (neg and pos)

    return test


def rect(x0, y0, x1, y1):
    return lambda x, y: x0 <= x <= x1 and y0 <= y <= y1


def eyes(g, cx, cy, dx, r, iris=7, pupil=6, white=5, tilt=0.0):
    for sx in (-1, 1):
        ex = cx + sx * dx
        g.paint(ellipse(ex, cy, r * 1.05, r * 1.12), white)
        g.paint(ellipse(ex, cy + r * 0.08, r * 0.7, r * 0.86), iris)
        g.paint(ellipse(ex, cy + r * 0.1, r * 0.3, r * 0.66), pupil)
        g.set(int(ex - sx * r * 0.35), int(cy - r * 0.55), white)


def smile(g, cx, cy, half=2.6, depth=0.35, color=6):
    for x in range(int(cx - half), int(cx + half) + 1):
        y = cy - (abs(x - cx) ** 1.5) * depth
        g.set(x, int(y), color)


# ------------------------------------------------------------- 1. 数码蛋 ----
def make_egg():
    g = Grid(22, 22)
    g.paint(ellipse(11, 12, 7.2, 9.2), 2)
    g.paint(ellipse(11, 8.0, 5.4, 5.2), 2)
    g.paint(ellipse(11, 17.5, 6.0, 4.2), 4)
    # 橙色锯齿斑纹
    for x in range(5, 18):
        y = 10 + (1 if (x // 2) % 2 else 0)
        g.set(x, y, 6)
        g.set(x, y + 1, 7)
    g.paint(circle(8.0, 16.0, 1.1), 6)
    g.paint(rot_ellipse(7.6, 6.6, 1.8, 1.2, -0.4), 3)
    g.outline(1)
    return g


# ----------------------------------------------------------- 2. 滚球兽 ----
def make_koromon():
    g = Grid(24, 24)
    # 长耳朵
    g.paint(rot_ellipse(4.6, 7.0, 5.4, 2.2, 0.72), 2)
    g.paint(rot_ellipse(19.4, 7.0, 5.4, 2.2, -0.72), 2)
    g.paint(rot_ellipse(4.8, 6.6, 3.2, 0.9, 0.72), 3)
    g.paint(rot_ellipse(19.2, 6.6, 3.2, 0.9, -0.72), 3)
    # 身体
    g.paint(circle(12, 14.0, 8.6), 2)
    g.paint(ellipse(12, 19.0, 7.0, 4.0), 4)
    # 眼睛：大而圆，红色虹膜
    eyes(g, 12, 13.2, 3.7, 2.9, iris=7, pupil=6)
    g.paint(ellipse(12, 20.0, 2.2, 1.0), 6)
    g.outline(1)
    return g


# ------------------------------------------------------------ 3. 亚古兽 ----
def make_agumon():
    g = Grid(30, 30)
    # 尾巴
    g.paint(rot_ellipse(25.0, 24.0, 5.6, 2.6, 0.55), 2)
    # 后腿 / 脚
    g.paint(ellipse(11.0, 26.5, 3.4, 2.6), 2)
    g.paint(ellipse(19.0, 26.5, 3.4, 2.6), 2)
    # 躯干
    g.paint(ellipse(15.0, 21.5, 6.2, 6.6), 2)
    g.paint(ellipse(15.0, 23.0, 4.4, 4.8), 3)      # 浅色腹部
    # 手臂
    g.paint(rot_ellipse(8.4, 20.0, 2.4, 3.2, 0.3), 2)
    g.paint(rot_ellipse(21.6, 20.0, 2.4, 3.2, -0.3), 2)
    # 头（略大，和躯干分开）
    g.paint(circle(15.0, 10.5, 7.6), 2)
    g.paint(ellipse(15.0, 14.0, 4.8, 3.0), 3)      # 吻部
    # 背脊深色条纹
    g.paint(ellipse(15.0, 7.2, 3.6, 1.4), 4)
    # 眼睛：绿色虹膜
    eyes(g, 15.0, 9.8, 3.0, 2.2, iris=7, pupil=6)
    # 鼻孔 + 嘴
    g.set(14, 13, 6)
    g.set(16, 13, 6)
    smile(g, 15.0, 16.0, half=2.0, depth=0.45, color=6)
    # 爪子
    for dx in (-1, 0, 1):
        g.set(int(6.5 + dx), 23, 5)
        g.set(int(23.5 + dx), 23, 5)
    # 脚趾
    for dx in (-1, 0, 1):
        g.set(int(10.0 + dx), 29, 5)
        g.set(int(20.0 + dx), 29, 5)
    g.outline(1)
    return g


# ----------------------------------------------------------- 4. 暴龙兽 ----
def make_greymon():
    g = Grid(36, 36)
    # 尾巴
    g.paint(rot_ellipse(30.0, 30.0, 6.5, 3.2, 0.5), 2)
    # 腿
    g.paint(ellipse(13.0, 32.0, 4.2, 3.0), 2)
    g.paint(ellipse(23.0, 32.0, 4.2, 3.0), 2)
    # 躯干
    g.paint(ellipse(18.0, 25.0, 8.2, 8.0), 2)
    g.paint(ellipse(18.0, 27.0, 5.8, 5.6), 3)
    # 红色鬃毛
    g.paint(ellipse(18.0, 16.5, 7.4, 4.0), 7)
    # 头 + 吻
    g.paint(circle(18.0, 10.5, 7.4), 2)
    g.paint(ellipse(18.0, 13.5, 4.8, 2.8), 3)
    # 蓝色头盔（盖住上半头）
    g.paint(ellipse(18.0, 6.6, 7.2, 4.6), 8)
    g.paint(ellipse(18.0, 6.2, 6.0, 3.0), 9 if False else 3)
    # 双角
    g.paint(tri((11.5, 7.5), (13.0, 1.0), (15.5, 6.0)), 5)
    g.paint(tri((24.5, 7.5), (23.0, 1.0), (20.5, 6.0)), 5)
    # 眼睛（在头盔下沿）
    eyes(g, 18.0, 11.2, 3.4, 1.9, iris=5, pupil=6)
    # 嘴 / 牙
    smile(g, 18.0, 16.0, half=3.0, depth=0.42, color=6)
    g.set(16, 16, 5)
    g.set(20, 16, 5)
    # 手爪
    for dx in (-1, 0, 1):
        g.set(int(7.5 + dx), 26, 5)
        g.set(int(28.5 + dx), 26, 5)
    g.outline(1)
    return g


# ------------------------------------------------------- 5. 机械暴龙兽 ----
def make_metalgreymon():
    g = Grid(42, 42)
    # 尾巴
    g.paint(rot_ellipse(35.0, 35.0, 7.0, 3.4, 0.5), 2)
    # 腿
    g.paint(ellipse(15.0, 37.5, 4.6, 3.2), 2)
    g.paint(ellipse(26.0, 37.5, 4.6, 3.2), 2)
    # 躯干
    g.paint(ellipse(21.0, 29.0, 9.2, 9.0), 2)
    g.paint(ellipse(21.0, 31.0, 6.4, 6.4), 3)
    # 红色鬃毛
    g.paint(ellipse(21.0, 19.5, 8.6, 4.4), 7)
    # 头
    g.paint(circle(21.0, 12.0, 8.0), 2)
    g.paint(ellipse(21.0, 15.5, 5.2, 3.0), 3)
    # 金属头盔
    g.paint(ellipse(21.0, 8.0, 8.0, 5.2), 8)
    g.paint(ellipse(21.0, 7.4, 6.2, 3.0), 9)
    # 单角
    g.paint(tri((17.0, 6.0), (21.0, -1.0), (25.0, 6.0)), 5)
    # 机械下颚
    g.paint(rect(16, 16, 26, 18), 8)
    for x in range(17, 26, 2):
        g.set(x, 18, 5)
    # 眼睛（红色，机械感）
    eyes(g, 21.0, 13.0, 3.6, 1.8, iris=7, pupil=6)
    # 右臂：金属炮
    g.paint(rect(29, 22, 38, 27), 8)
    g.paint(circle(38.0, 24.5, 3.0), 9)
    g.paint(circle(38.0, 24.5, 1.3), 6)
    # 左臂：爪子
    g.paint(rot_ellipse(11.0, 25.0, 3.0, 4.0, -0.2), 2)
    for dx in (-1, 0, 1):
        g.set(int(9.0 + dx), 29, 5)
    g.outline(1)
    return g


# ------------------------------------------------------- 6. 战斗暴龙兽 ----
def make_wargreymon():
    g = Grid(46, 46)
    # 双翼（背后）
    g.paint(rot_ellipse(8.0, 20.0, 7.5, 3.0, 0.75), 7)
    g.paint(rot_ellipse(38.0, 20.0, 7.5, 3.0, -0.75), 7)
    g.paint(rot_ellipse(8.5, 18.6, 4.6, 1.4, 0.75), 4)
    g.paint(rot_ellipse(37.5, 18.6, 4.6, 1.4, -0.75), 4)
    # 腿
    g.paint(rect(17, 36, 21, 42), 2)
    g.paint(rect(25, 36, 29, 42), 2)
    g.paint(ellipse(19.0, 42.0, 3.6, 1.8), 8)
    g.paint(ellipse(27.0, 42.0, 3.6, 1.8), 8)
    # 背盾（先画，被躯干挡住大部分，只露边缘）
    g.paint(ellipse(23.0, 22.0, 9.5, 4.0), 8)
    # 躯干（金甲）
    g.paint(ellipse(23.0, 30.0, 7.0, 8.5), 2)
    g.paint(ellipse(23.0, 31.0, 4.4, 5.6), 3)
    # 胸口徽记
    g.paint(circle(23.0, 29.0, 2.2), 8)
    g.paint(circle(23.0, 29.0, 1.0), 7)
    # 头盔（银）
    g.paint(circle(23.0, 12.0, 7.0), 8)
    g.paint(ellipse(23.0, 15.5, 4.2, 2.4), 3)
    # 独角
    g.paint(tri((20.0, 8.0), (23.0, 2.0), (26.0, 8.0)), 5)
    # 眼睛
    eyes(g, 23.0, 12.6, 3.0, 1.8, iris=2, pupil=6)
    # 巨爪护手
    g.paint(ellipse(11.0, 28.0, 4.0, 4.6), 2)
    g.paint(ellipse(35.0, 28.0, 4.0, 4.6), 2)
    for i in range(3):
        g.paint(tri((8.0 + i * 2.2, 31.5), (8.6 + i * 2.2, 36.0), (10.4 + i * 2.2, 31.5)), 5)
        g.paint(tri((32.0 + i * 2.2, 31.5), (32.6 + i * 2.2, 36.0), (34.4 + i * 2.2, 31.5)), 5)
    # 背盾（已在上方先画，此处不再覆盖胸口）
    g.paint(ellipse(23.0, 11.0, 7.2, 2.6), 4)   # 头盔下沿阴影
    g.outline(1)
    return g


# --------------------------------------------------------------- 死亡 ----
def make_dead():
    g = Grid(24, 24)
    g.paint(ellipse(12, 15.0, 8.0, 7.2), 2)
    g.paint(ellipse(12, 10.0, 6.0, 5.0), 2)
    g.paint(ellipse(12, 12.0, 5.2, 3.0), 3)
    for sx in (-1, 1):
        ex = 12 + sx * 3.4
        for i in range(-2, 3):
            g.set(int(ex + i), int(12 + i), 6)
            g.set(int(ex + i), int(12 - i), 6)
    g.outline(1)
    return g


# ------------------------------------------------------------------ 输出 ----
def emit(name, grid, pal, out):
    rows = grid.rows()
    assert all(len(r) == grid.w for r in rows), f"{name}: 行宽不一致"
    out.append(f"// {name} {grid.w}x{grid.h}")
    vals = ", ".join(f"0x{rgb565(c):04X}" for c in pal)
    out.append(f"static const uint16_t PAL_{name}[16] = {{ {vals} }};")
    out.append(f"static const char* const SPR_{name}[] = {{")
    for r in rows:
        out.append(f'    "{r}",')
    out.append("};")
    out.append("")


def main():
    sprites = [
        ("EGG", make_egg(), PAL["EGG"]),
        ("KOROMON", make_koromon(), PAL["KOROMON"]),
        ("AGUMON", make_agumon(), PAL["AGUMON"]),
        ("GREYMON", make_greymon(), PAL["GREYMON"]),
        ("METALGREYMON", make_metalgreymon(), PAL["METALGREYMON"]),
        ("WARGREYMON", make_wargreymon(), PAL["WARGREYMON"]),
        ("DEAD", make_dead(), PAL["DEAD"]),
    ]

    out = ["// AUTO-GENERATED by tools/gen_sprites.py — do not edit by hand.",
           "#pragma once", "#include <stdint.h>", "",
           "struct SpriteDef {", "    uint8_t w, h;",
           "    const char* const* rows;   // 每行一个十六进制字符串, '0' = 透明",
           "    const uint16_t* pal;       // 16 色 RGB565", "};", ""]

    for name, grid, pal in sprites:
        emit(name, grid, pal, out)

    out.append("static const SpriteDef SPRITES[] = {")
    for name, grid, _ in sprites:
        out.append(f"    {{ {grid.w}, {grid.h}, SPR_{name}, PAL_{name} }},")
    out.append("};")
    out.append("")
    ids = ", ".join(
        (f"SPR_ID_{name} = 0" if i == 0 else f"SPR_ID_{name}")
        for i, (name, _, _) in enumerate(sprites)
    )
    out.append(f"enum SpriteId {{ {ids}, SPR_ID_COUNT }};")
    out.append("")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))

    print(f"wrote {os.path.abspath(OUT)}")
    for name, grid, _ in sprites:
        print(f"  {name:14s} {grid.w}x{grid.h}")


if __name__ == "__main__":
    main()
