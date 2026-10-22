#include "sprite.h"

static inline uint8_t hexval(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

// dim: 255 = 原色; 其它值按比例压暗（睡眠/受伤时用），不做浮点运算。
static inline uint16_t scale_color(uint16_t c, uint8_t dim) {
    if (dim >= 255) return c;
    uint16_t r = (c >> 11) & 0x1F;
    uint16_t g = (c >> 5) & 0x3F;
    uint16_t b = c & 0x1F;
    r = (r * dim) >> 8;
    g = (g * dim) >> 8;
    b = (b * dim) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void sprite_draw(Adafruit_GFX& g, const SpriteDef& s, int16_t cx, int16_t cy,
                 uint8_t scale, uint8_t dim) {
    if (scale < 1) scale = 1;
    const int16_t ox = cx - (int16_t)(s.w * scale) / 2;
    const int16_t oy = cy - (int16_t)(s.h * scale) / 2;

    // 预先把调色板压暗一次，避免每个像素重复计算
    uint16_t pal[16];
    for (int i = 0; i < 16; i++) pal[i] = scale_color(s.pal[i], dim);

    for (uint8_t row = 0; row < s.h; row++) {
        const char* line = s.rows[row];
        for (uint8_t col = 0; col < s.w; col++) {
            uint8_t idx = hexval(line[col]);
            if (idx == 0) continue;                 // 透明
            uint16_t color = pal[idx];
            int16_t x = ox + (int16_t)col * scale;
            int16_t y = oy + (int16_t)row * scale;
            if (scale == 1) {
                g.drawPixel(x, y, color);
            } else {
                g.fillRect(x, y, scale, scale, color);
            }
        }
    }
}

// 受击闪白：把每个像素朝白色插值（level 0..255）
void sprite_draw_flash(Adafruit_GFX& g, const SpriteDef& s, int16_t cx, int16_t cy,
                       uint8_t level) {
    const int16_t ox = cx - (int16_t)s.w / 2;
    const int16_t oy = cy - (int16_t)s.h / 2;

    uint16_t pal[16];
    for (int i = 0; i < 16; i++) {
        uint16_t c = s.pal[i];
        uint16_t r = (c >> 11) & 0x1F;
        uint16_t gr = (c >> 5) & 0x3F;
        uint16_t b = c & 0x1F;
        r = (uint16_t)(r + ((31 - r) * level) / 255);
        gr = (uint16_t)(gr + ((63 - gr) * level) / 255);
        b = (uint16_t)(b + ((31 - b) * level) / 255);
        pal[i] = (uint16_t)((r << 11) | (gr << 5) | b);
    }

    for (uint8_t row = 0; row < s.h; row++) {
        const char* line = s.rows[row];
        for (uint8_t col = 0; col < s.w; col++) {
            uint8_t idx = hexval(line[col]);
            if (idx == 0) continue;
            g.drawPixel(ox + col, oy + row, pal[idx]);
        }
    }
}

void sprite_draw_bob(Adafruit_GFX& g, const SpriteDef& s, int16_t cx, int16_t cy,
                     uint32_t now_ms, uint8_t scale, uint8_t dim) {
    // 2 秒一个循环，上下各 1px：比正弦便宜，肉眼效果一样
    int8_t bob = ((now_ms / 1000) & 1) ? 1 : 0;
    sprite_draw(g, s, cx, cy + bob, scale, dim);
}
