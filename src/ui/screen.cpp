#include "screen.h"
#include "../panel.h"
#include "../assets/bg_data.h"
#include <string.h>

static uint16_t s_base[64 * 64];          // 背景 + 云（8KB）
static uint8_t  s_scene = 0xFF;

// 上一帧角色（用于像素级差分：只写真正变化的像素）
static const SpriteDef* s_prev = nullptr;
static int16_t  s_prev_cx = 0, s_prev_cy = 0;
static uint8_t  s_prev_dim = 255;

// 云状态
static const char* const CL_A[3] = {"...1111...", "..111111..", ".11111111."};
static const char* const CL_B[3] = {"....111111.....", "..1111111111...", "1111111111111."};

struct Cloud { const char* const* shape; uint8_t w, y; float x, sp; int16_t last_ix; };
static Cloud s_cl[3] = {
    {CL_B, 15, 3, 6.0f, 2.2f, -999},
    {CL_A, 10, 9, 34.0f, 1.4f, -999},
    {CL_B, 15, 15, 52.0f, 1.0f, -999},
};
static uint32_t s_cloud_last = 0;

static inline uint8_t bg_idx(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'V') return (uint8_t)(c - 'A' + 10);
    return 0;
}

static inline uint8_t hex_idx(char ch) {
    if (ch >= '0' && ch <= '9') return (uint8_t)(ch - '0');
    if (ch >= 'A' && ch <= 'F') return (uint8_t)(ch - 'A' + 10);
    if (ch >= 'a' && ch <= 'f') return (uint8_t)(ch - 'a' + 10);
    return 0;
}

static inline uint16_t dim_color(uint16_t c, uint8_t dim) {
    if (dim >= 255) return c;
    uint16_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    r = (uint16_t)((r * dim) >> 8);
    g = (uint16_t)((g * dim) >> 8);
    b = (uint16_t)((b * dim) >> 8);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static inline void put_base(int16_t x, int16_t y, uint16_t c) {
    if (x < 0 || y < 0 || x >= 64 || y >= 64) return;
    s_base[y * 64 + x] = c;
}

static void blit_rect(int16_t x0, int16_t y0, int16_t w, int16_t h) {
    for (int16_t y = y0; y < y0 + h; y++) {
        if (y < 0 || y >= 64) continue;
        for (int16_t x = x0; x < x0 + w; x++) {
            if (x < 0 || x >= 64) continue;
            g_matrix->drawPixel(x, y, s_base[y * 64 + x]);
        }
    }
}

static void build_scene(uint8_t idx) {
    const SceneDef& s = BG_SCENES[idx];
    for (int16_t y = 0; y < 64; y++) {
        for (int16_t x = 0; x < 64; x++) {
            uint16_t c = 0x0000;
            if (y >= 8 && y < 8 + s.h) {
                const char* row = s.rows[y - 8];
                c = s.pal[bg_idx(row[x])];
            }
            s_base[y * 64 + x] = c;
        }
    }
}

void screen_init() {
    memset(s_base, 0, sizeof(s_base));
    s_scene = 0xFF;
    s_prev = nullptr;
    s_prev_dim = 255;
    for (int i = 0; i < 3; i++) s_cl[i].last_ix = -999;
    s_cloud_last = 0;
}

void screen_clear() {
    g_matrix->fillScreen(0x0000);
    s_prev = nullptr;
}

void screen_forget_sprite() { s_prev = nullptr; }

void screen_scene(uint8_t idx) {
    if (idx == s_scene) return;
    s_scene = idx;
    build_scene(idx);
    s_cl[0].x = 6;  s_cl[1].x = 34; s_cl[2].x = 52;
    for (int i = 0; i < 3; i++) s_cl[i].last_ix = -999;
    s_prev = nullptr;
    blit_rect(0, 0, 64, 64);
}

static void stamp_cloud(Cloud& c, uint16_t main_c, uint16_t shade_c) {
    int16_t ox = (int16_t)c.x;
    for (int yy = 0; yy < 3; yy++) {
        const char* row = c.shape[yy];
        for (int xx = 0; xx < c.w; xx++) {
            if (row[xx] != '1') continue;
            put_base(ox + xx, 8 + c.y + yy, (yy == 2) ? shade_c : main_c);
        }
    }
}

static void erase_cloud(Cloud& c) {
    const SceneDef& s = BG_SCENES[s_scene];
    int16_t ox = (int16_t)c.x;
    for (int yy = 0; yy < 3; yy++) {
        int16_t y = 8 + c.y + yy;
        if (y < 8 || y >= 8 + s.h) continue;
        const char* row = s.rows[y - 8];
        for (int16_t xx = -2; xx < c.w + 2; xx++) {
            int16_t x = ox + xx;
            if (x < 0 || x >= 64) continue;
            put_base(x, y, s.pal[bg_idx(row[x])]);
        }
    }
}

void screen_clouds(uint32_t now_ms, uint8_t scene) {
    if (s_scene != scene) return;

    float dt = 0;
    if (s_cloud_last) dt = (now_ms - s_cloud_last) / 1000.0f;
    s_cloud_last = now_ms;
    if (dt < 0 || dt > 0.5f) dt = 0;

    const uint16_t c_main = (scene == 0) ? 0xFFFF : 0x18C6;
    const uint16_t c_shade = (scene == 0) ? 0xD6FF : 0x1082;

    for (int i = 0; i < 3; i++) {
        Cloud& c = s_cl[i];
        c.x += c.sp * dt;
        if (c.x > 64 + c.w) c.x = -(float)c.w;

        int16_t ix = (int16_t)c.x;
        if (ix == c.last_ix) continue;
        c.last_ix = ix;

        erase_cloud(c);
        stamp_cloud(c, c_main, c_shade);
        blit_rect(ix - 2, 8 + c.y, c.w + 4, 3);
        // 云盖到角色区域时，让下一帧重画角色
        if (s_prev) {
            int16_t px0 = s_prev_cx - (int16_t)s_prev->w / 2;
            int16_t px1 = px0 + s_prev->w;
            if (ix + c.w >= px0 && ix - 2 <= px1) s_prev = nullptr;
        }
    }
}

// 角色像素级差分：只有颜色真正变化的像素才写面板
void screen_sprite(const SpriteDef* spr, int16_t cx, int16_t cy, uint8_t dim) {
    if (!spr) return;

    // 同一帧、同位置、同亮度 → 一个像素都不写
    if (spr == s_prev && cx == s_prev_cx && cy == s_prev_cy && dim == s_prev_dim) return;

    const int16_t ox = cx - (int16_t)spr->w / 2;
    const int16_t oy = cy - (int16_t)spr->h / 2;

    bool same_box = s_prev && s_prev_cx == cx && s_prev_cy == cy &&
                    s_prev->w == spr->w && s_prev->h == spr->h;

    if (!same_box) {
        if (s_prev) {
            const int16_t pox = s_prev_cx - (int16_t)s_prev->w / 2;
            const int16_t poy = s_prev_cy - (int16_t)s_prev->h / 2;
            for (uint8_t row = 0; row < s_prev->h; row++) {
                const char* line = s_prev->rows[row];
                int16_t y = poy + row;
                if (y < 0 || y >= 64) continue;
                for (uint8_t col = 0; col < s_prev->w; col++) {
                    if (hex_idx(line[col]) == 0) continue;
                    int16_t x = pox + col;
                    if (x < 0 || x >= 64) continue;
                    g_matrix->drawPixel(x, y, s_base[y * 64 + x]);
                }
            }
        }
        for (uint8_t row = 0; row < spr->h; row++) {
            const char* line = spr->rows[row];
            int16_t y = oy + row;
            if (y < 0 || y >= 64) continue;
            for (uint8_t col = 0; col < spr->w; col++) {
                uint8_t idx = hex_idx(line[col]);
                if (idx == 0) continue;
                int16_t x = ox + col;
                if (x < 0 || x >= 64) continue;
                g_matrix->drawPixel(x, y, dim_color(spr->pal[idx], dim));
            }
        }
    } else {
        // 同一画布内换帧：只写颜色不同的像素
        for (uint8_t row = 0; row < spr->h; row++) {
            const char* nl = spr->rows[row];
            const char* ol = s_prev->rows[row];
            int16_t y = oy + row;
            if (y < 0 || y >= 64) continue;
            for (uint8_t col = 0; col < spr->w; col++) {
                int16_t x = ox + col;
                if (x < 0 || x >= 64) continue;

                uint8_t ni = hex_idx(nl[col]);
                uint8_t oi = hex_idx(ol[col]);
                uint16_t bg = s_base[y * 64 + x];
                uint16_t nc = ni ? dim_color(spr->pal[ni], dim) : bg;
                uint16_t oc = oi ? dim_color(s_prev->pal[oi], s_prev_dim) : bg;
                if (nc == oc) continue;
                g_matrix->drawPixel(x, y, nc);
            }
        }
    }

    s_prev = spr;
    s_prev_cx = cx;
    s_prev_cy = cy;
    s_prev_dim = dim;
}

void screen_sprite_flash(const SpriteDef* spr, int16_t cx, int16_t cy, uint8_t level) {
    if (!spr) return;

    if (s_prev) {
        const int16_t pox = s_prev_cx - (int16_t)s_prev->w / 2;
        const int16_t poy = s_prev_cy - (int16_t)s_prev->h / 2;
        for (uint8_t row = 0; row < s_prev->h; row++) {
            const char* line = s_prev->rows[row];
            int16_t y = poy + row;
            if (y < 0 || y >= 64) continue;
            for (uint8_t col = 0; col < s_prev->w; col++) {
                if (hex_idx(line[col]) == 0) continue;
                int16_t x = pox + col;
                if (x < 0 || x >= 64) continue;
                g_matrix->drawPixel(x, y, s_base[y * 64 + x]);
            }
        }
    }

    const int16_t ox = cx - (int16_t)spr->w / 2;
    const int16_t oy = cy - (int16_t)spr->h / 2;
    uint16_t pal[16];
    for (int i = 0; i < 16; i++) {
        uint16_t c = spr->pal[i];
        uint16_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
        r = (uint16_t)(r + ((31 - r) * level) / 255);
        g = (uint16_t)(g + ((63 - g) * level) / 255);
        b = (uint16_t)(b + ((31 - b) * level) / 255);
        pal[i] = (uint16_t)((r << 11) | (g << 5) | b);
    }
    for (uint8_t row = 0; row < spr->h; row++) {
        const char* line = spr->rows[row];
        int16_t y = oy + row;
        if (y < 0 || y >= 64) continue;
        for (uint8_t col = 0; col < spr->w; col++) {
            uint8_t idx = hex_idx(line[col]);
            if (idx == 0) continue;
            int16_t x = ox + col;
            if (x < 0 || x >= 64) continue;
            g_matrix->drawPixel(x, y, pal[idx]);
        }
    }

    s_prev = spr;
    s_prev_cx = cx;
    s_prev_cy = cy;
    s_prev_dim = 255;
}

void screen_text_at(const char* s, int16_t x, int16_t y, uint16_t color) {
    if (!s) return;
    g_matrix->setTextColor(color);
    g_matrix->setTextSize(1);
    g_matrix->setCursor(x, y);
    g_matrix->print(s);
}

void screen_text_center(const char* s, int16_t y, uint16_t color) {
    if (!s) return;
    int n = (int)strlen(s);
    if (n > 10) n = 10;
    int x = (64 - n * 6) / 2;
    g_matrix->setTextColor(color);
    g_matrix->setTextSize(1);
    g_matrix->setCursor(x, y);
    g_matrix->print(s);
}

void screen_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    g_matrix->fillRect(x, y, w, h, color);
}

void screen_clear_band(int16_t y, int16_t h) {
    blit_rect(0, y, 64, h);
}
