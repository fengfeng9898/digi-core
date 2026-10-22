#include "screen.h"
#include "../panel.h"
#include "../assets/bg_data.h"
#include <string.h>

static uint16_t s_base[64 * 64];          // 背景 + 云（8KB）
static uint8_t  s_scene = 0xFF;

// 上一帧画过的角色，用于精确恢复
static const SpriteDef* s_prev = nullptr;
static int16_t  s_prev_cx = 0, s_prev_cy = 0;

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

static inline void put_base(int16_t x, int16_t y, uint16_t c) {
    if (x < 0 || y < 0 || x >= 64 || y >= 64) return;
    s_base[y * 64 + x] = c;
}

// 从底图刷一个矩形到面板
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
    // 云位置重置到场景初始，避免跨场景错位
    s_cl[0].x = 6;  s_cl[1].x = 34; s_cl[2].x = 52;
    for (int i = 0; i < 3; i++) s_cl[i].last_ix = -999;
    s_prev = nullptr;
    blit_rect(0, 0, 64, 64);
}

// 把某朵云按当前整数位置画进底图
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

// 用背景重填某朵云占据的条带
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
        if (ix == c.last_ix) continue;          // 没跨像素就不重画
        c.last_ix = ix;

        erase_cloud(c);
        stamp_cloud(c, c_main, c_shade);
        blit_rect(ix - 2, 8 + c.y, c.w + 4, 3);
    }
}

// 把上一帧角色覆盖的像素从底图恢复
static void restore_prev() {
    if (!s_prev) return;
    const int16_t ox = s_prev_cx - (int16_t)s_prev->w / 2;
    const int16_t oy = s_prev_cy - (int16_t)s_prev->h / 2;
    for (uint8_t row = 0; row < s_prev->h; row++) {
        const char* line = s_prev->rows[row];
        int16_t y = oy + row;
        if (y < 0 || y >= 64) continue;
        for (uint8_t col = 0; col < s_prev->w; col++) {
            if (line[col] == '0') continue;
            int16_t x = ox + col;
            if (x < 0 || x >= 64) continue;
            g_matrix->drawPixel(x, y, s_base[y * 64 + x]);
        }
    }
}

static inline uint16_t dim_color(uint16_t c, uint8_t dim) {
    if (dim >= 255) return c;
    uint16_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    r = (uint16_t)((r * dim) >> 8);
    g = (uint16_t)((g * dim) >> 8);
    b = (uint16_t)((b * dim) >> 8);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void screen_sprite(const SpriteDef* spr, int16_t cx, int16_t cy, uint8_t dim) {
    if (!spr) return;
    restore_prev();

    const int16_t ox = cx - (int16_t)spr->w / 2;
    const int16_t oy = cy - (int16_t)spr->h / 2;
    uint16_t pal[16];
    for (int i = 0; i < 16; i++) pal[i] = dim_color(spr->pal[i], dim);

    for (uint8_t row = 0; row < spr->h; row++) {
        const char* line = spr->rows[row];
        int16_t y = oy + row;
        if (y < 0 || y >= 64) continue;
        for (uint8_t col = 0; col < spr->w; col++) {
            uint8_t idx;
            char ch = line[col];
            if (ch >= '0' && ch <= '9') idx = (uint8_t)(ch - '0');
            else if (ch >= 'A' && ch <= 'F') idx = (uint8_t)(ch - 'A' + 10);
            else if (ch >= 'a' && ch <= 'f') idx = (uint8_t)(ch - 'a' + 10);
            else idx = 0;
            if (idx == 0) continue;
            int16_t x = ox + col;
            if (x < 0 || x >= 64) continue;
            g_matrix->drawPixel(x, y, pal[idx]);
        }
    }

    s_prev = spr;
    s_prev_cx = cx;
    s_prev_cy = cy;
}

void screen_sprite_flash(const SpriteDef* spr, int16_t cx, int16_t cy, uint8_t level) {
    if (!spr) return;
    restore_prev();

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
            uint8_t idx;
            char ch = line[col];
            if (ch >= '0' && ch <= '9') idx = (uint8_t)(ch - '0');
            else if (ch >= 'A' && ch <= 'F') idx = (uint8_t)(ch - 'A' + 10);
            else if (ch >= 'a' && ch <= 'f') idx = (uint8_t)(ch - 'a' + 10);
            else idx = 0;
            if (idx == 0) continue;
            int16_t x = ox + col;
            if (x < 0 || x >= 64) continue;
            g_matrix->drawPixel(x, y, pal[idx]);
        }
    }

    s_prev = spr;
    s_prev_cx = cx;
    s_prev_cy = cy;
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
