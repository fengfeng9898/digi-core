#!/usr/bin/env python3
"""
gen_sprites.py — 程序化像素精灵生成器 → src/assets/sprites.h

为什么程序化: 项目早期没有画师, 但 UI/状态机/战斗需要真实的动画数据来验证。
用 SDF(有符号距离场) 画圆形/椭圆, 输出 16 色索引 ASCII 图, 由固件 blitter
逐像素解码。以后有 PNG 素材时, 只要保持同样的输出格式即可替换。

用法:
    python tools/gen_sprites.py
"""
from __future__ import annotations

import math
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "src", "assets", "sprites.h")

# ---------------------------------------------------------------- palette ----
# 索引 0 恒为透明。每只数码兽一个 16 色调色板。
PAL_EGG = [
    (0, 0, 0), (26, 16, 32), (240, 232, 210), (255, 250, 235), (206, 194, 170),
    (250, 210, 120), (190, 150, 70), (120, 90, 50), (0, 0, 0), (0, 0, 0),
    (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0),
]
PAL_BABY = [
    (0, 0, 0), (22, 18, 34), (108, 214, 140), (156, 240, 176), (58, 158, 100),
    (250, 250, 255), (24, 20, 40), (230, 90, 110), (255, 214, 92), (0, 0, 0),
    (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0),
]
PAL_CHILD = [
    (0, 0, 0), (20, 16, 32), (96, 190, 235), (150, 226, 255), (46, 128, 180),
    (250, 250, 255), (18, 16, 34), (235, 96, 110), (255, 210, 84), (250, 236, 150),
    (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0),
]
PAL_TEEN = [
    (0, 0, 0), (20, 14, 30), (176, 128, 240), (214, 180, 255), (112, 70, 178),
    (250, 250, 255), (16, 12, 30), (240, 120, 90), (255, 220, 100), (255, 240, 170),
    (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0),
]
PAL_DEAD = [
    (0, 0, 0), (28, 26, 32), (86, 84, 92), (116, 112, 122), (58, 56, 64),
    (196, 196, 204), (24, 22, 30), (120, 40, 40), (0, 0, 0), (0, 0, 0),
    (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0), (0, 0, 0),
]


def rgb565(c: tuple[int, int, int]) -> int:
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


class Grid:
    """索引色画布。0 = 透明。"""

    def __init__(self, w: int, h: int):
        self.w, self.h = w, h
        self.px = [[0] * w for _ in range(h)]

    def inside(self, x: int, y: int) -> bool:
        return 0 <= x < self.w and 0 <= y < self.h

    def get(self, x: int, y: int) -> int:
        return self.px[y][x] if self.inside(x, y) else 0

    def set(self, x: int, y: int, v: int) -> None:
        if self.inside(x, y):
            self.px[y][x] = v

    def fill_shape(self, test, color: int) -> None:
        for y in range(self.h):
            for x in range(self.w):
                if test(x + 0.5, y + 0.5):
                    self.px[y][x] = color

    def outline(self, color: int) -> None:
        """给所有实体像素加上 1px 外轮廓（4 邻域）。"""
        solid = [(x, y) for y in range(self.h) for x in range(self.w) if self.px[y][x] != 0]
        for x, y in solid:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if self.inside(nx, ny) and self.px[ny][nx] == 0:
                    self.px[ny][nx] = color

    def as_rows(self) -> list[str]:
        hexd = "0123456789ABCDEF"
        return ["".join(hexd[v] for v in row) for row in self.px]


def ellipse(cx: float, cy: float, rx: float, ry: float):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def circle(cx: float, cy: float, r: float):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def rotated_ellipse(cx, cy, rx, ry, ang):
    ca, sa = math.cos(ang), math.sin(ang)

    def test(x, y):
        dx, dy = x - cx, y - cy
        u = dx * ca + dy * sa
        v = -dx * sa + dy * ca
        return (u / rx) ** 2 + (v / ry) ** 2 <= 1.0

    return test


# ------------------------------------------------------------------ 形状 ----
def make_egg() -> Grid:
    g = Grid(24, 24)
    g.fill_shape(ellipse(12, 13, 8.2, 10.2), 2)
    # 上方略窄的蛋形
    g.fill_shape(ellipse(12, 8.5, 6.2, 6.0), 2)
    # 底部阴影（下缘暗一档）
    g.fill_shape(ellipse(12, 20, 7.0, 5.0), 4)
    # 花纹：细锯齿纹（1px），避免糊成一条“缝”
    for x in range(7, 18):
        y = 11 + ((x // 2) % 2)
        g.set(x, y, 6)
    g.fill_shape(circle(8.5, 16.5, 1.2), 5)
    g.fill_shape(circle(15.5, 18.0, 1.0), 5)
    # 高光
    g.fill_shape(ellipse(8.2, 7.6, 2.2, 1.6), 3)
    g.outline(1)
    return g


def draw_face(g: Grid, cx: float, cy: float, eye_dx: float, eye_r: float, pal_mouth: int = 7):
    for sx in (-1, 1):
        ex = cx + sx * eye_dx
        g.fill_shape(ellipse(ex, cy, eye_r, eye_r * 1.15), 5)          # 眼白
        g.fill_shape(circle(ex + sx * 0.15, cy + 0.15, eye_r * 0.62), 6)  # 瞳孔
        g.set(int(ex - 0.4), int(cy - 0.6), 5)                          # 高光点
    # 嘴：上扬的弧线（微笑）
    for x in range(int(cx - 2.5), int(cx + 3)):
        y = cy + 4.4 - (abs(x - cx) ** 1.6) * 0.38
        g.set(x, int(y), pal_mouth)


def make_baby() -> Grid:
    g = Grid(24, 24)
    g.fill_shape(ellipse(12, 13.5, 8.0, 8.6), 2)
    # 身体下缘阴影
    g.fill_shape(ellipse(12, 18.5, 7.2, 4.6), 4)
    # 头顶小芽（单株，居中）
    g.fill_shape(rotated_ellipse(12.0, 5.2, 2.4, 1.2, 0.0), 2)
    g.fill_shape(rotated_ellipse(12.0, 3.6, 1.0, 1.6, 0.0), 3)
    draw_face(g, 12, 12.0, 3.4, 2.2)
    g.fill_shape(ellipse(9.0, 8.6, 1.6, 1.1), 3)
    g.outline(1)
    return g


def make_child() -> Grid:
    g = Grid(32, 32)
    # 身体
    g.fill_shape(ellipse(16, 19.5, 10.0, 9.4), 2)
    g.fill_shape(ellipse(16, 25.0, 8.6, 4.4), 4)
    # 两只叶耳
    g.fill_shape(rotated_ellipse(10.5, 8.0, 5.2, 2.4, 0.65), 2)
    g.fill_shape(rotated_ellipse(21.5, 8.0, 5.2, 2.4, -0.65), 2)
    g.fill_shape(rotated_ellipse(10.0, 7.2, 3.4, 1.3, 0.65), 3)
    g.fill_shape(rotated_ellipse(22.0, 7.2, 3.4, 1.3, -0.65), 3)
    # 小手
    g.fill_shape(ellipse(5.6, 20.0, 2.6, 2.0), 2)
    g.fill_shape(ellipse(26.4, 20.0, 2.6, 2.0), 2)
    # 脚
    g.fill_shape(ellipse(11.0, 28.6, 3.2, 2.0), 2)
    g.fill_shape(ellipse(21.0, 28.6, 3.2, 2.0), 2)
    draw_face(g, 16, 17.0, 4.4, 2.6)
    # 额前高光（不挡眼睛）
    g.fill_shape(ellipse(11.0, 12.6, 2.0, 1.2), 3)
    g.outline(1)
    return g


def make_teen() -> Grid:
    g = Grid(40, 40)
    g.fill_shape(ellipse(20, 25.0, 12.6, 11.6), 2)
    g.fill_shape(ellipse(20, 32.0, 10.6, 5.0), 4)
    # 双翼
    g.fill_shape(rotated_ellipse(6.0, 20.0, 6.4, 3.2, 0.9), 2)
    g.fill_shape(rotated_ellipse(34.0, 20.0, 6.4, 3.2, -0.9), 2)
    g.fill_shape(rotated_ellipse(5.4, 18.6, 4.0, 1.6, 0.9), 3)
    g.fill_shape(rotated_ellipse(34.6, 18.6, 4.0, 1.6, -0.9), 3)
    # 头冠
    for dx, h in ((-4, 5), (0, 7), (4, 5)):
        g.fill_shape(rotated_ellipse(20 + dx, 9.0 - h * 0.2, 1.6, h * 0.55, 0.0), 8)
    draw_face(g, 20, 21.0, 5.2, 3.4)
    # 胸口纹章
    g.fill_shape(circle(20, 28.5, 2.2), 8)
    g.fill_shape(circle(20, 28.5, 1.0), 7)
    g.fill_shape(circle(13.0, 18.0, 2.4), 3)
    g.outline(1)
    return g


def make_dead() -> Grid:
    g = Grid(24, 24)
    g.fill_shape(ellipse(12, 15.0, 8.0, 7.4), 2)
    # 墓碑形
    g.fill_shape(ellipse(12, 10.0, 6.0, 5.0), 2)
    g.fill_shape(ellipse(12, 12.0, 5.2, 3.0), 3)
    # X 眼
    for sx in (-1, 1):
        ex = 12 + sx * 3.4
        for i in range(-2, 3):
            g.set(int(ex + i), int(12 + i), 6)
            g.set(int(ex + i), int(12 - i), 6)
    g.outline(1)
    return g


# ------------------------------------------------------------------ 输出 ----
FIELDS = ("w", "h")


def emit(name: str, grid: Grid, pal, out: list[str]) -> None:
    rows = grid.as_rows()
    assert all(len(r) == grid.w for r in rows), f"{name}: row width mismatch"
    out.append(f"// {name} {grid.w}x{grid.h}")
    pal_vals = ", ".join(f"0x{rgb565(c):04X}" for c in pal)
    out.append(f"static const uint16_t PAL_{name}[16] = {{ {pal_vals} }};")
    out.append(f"static const char* const SPR_{name}[] = {{")
    for r in rows:
        out.append(f'    "{r}",')
    out.append("};")
    out.append("")


def main() -> None:
    sprites = [
        ("EGG", make_egg(), PAL_EGG),
        ("BABY", make_baby(), PAL_BABY),
        ("CHILD", make_child(), PAL_CHILD),
        ("TEEN", make_teen(), PAL_TEEN),
        ("DEAD", make_dead(), PAL_DEAD),
    ]

    out: list[str] = []
    out.append("// AUTO-GENERATED by tools/gen_sprites.py — do not edit by hand.")
    out.append("#pragma once")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("struct SpriteDef {")
    out.append("    uint8_t w, h;")
    out.append("    const char* const* rows;   // 每行一个十六进制字符串, '0' = 透明")
    out.append("    const uint16_t* pal;       // 16 色 RGB565")
    out.append("};")
    out.append("")
    for name, grid, pal in sprites:
        emit(name, grid, pal, out)

    out.append("static const SpriteDef SPRITES[] = {")
    for name, grid, _ in sprites:
        out.append(f"    {{ {grid.w}, {grid.h}, SPR_{name}, PAL_{name} }},")
    out.append("};")
    out.append("")
    out.append("enum SpriteId { SPR_ID_EGG = 0, SPR_ID_BABY, SPR_ID_CHILD, SPR_ID_TEEN, SPR_ID_DEAD, SPR_ID_COUNT };")
    out.append("")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))

    print(f"wrote {os.path.abspath(OUT)}")
    for name, grid, _ in sprites:
        print(f"  {name:6s} {grid.w}x{grid.h}  {len(grid.as_rows())} rows")


if __name__ == "__main__":
    main()
