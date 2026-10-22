#include "ui.h"
#include "../config.h"
#include "../panel.h"
#include "../gfx/sprite.h"
#include "../gfx/anim.h"
#include "../game/clock.h"
#include "../assets/bg_data.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------- 颜色 ----
#define C_BLACK  0x0000
#define C_WHITE  0xFFFF
#define C_GRAY   0x7BEF
#define C_DIM    0x39E7
#define C_RED    0xF800
#define C_DKRD   0x6000
#define C_GREEN  0x07E0
#define C_AMBER  0xFD20
#define C_CYAN   0x07FF

static uint8_t  s_brightness = PANEL_BRIGHTNESS;
static char     s_toast[24] = {0};
static uint32_t s_toast_until = 0;

// 5x5 爱心
static const uint8_t HEART[5] = {0b01010, 0b11111, 0b11111, 0b01110, 0b00100};

static void draw_heart(int16_t x, int16_t y, bool filled) {
    const uint16_t c = filled ? C_RED : C_DKRD;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            if (HEART[row] & (0x10 >> col)) {
                g_matrix->drawPixel(x + col, y + row, c);
            }
        }
    }
}

static void draw_text_centered(const char* s, int16_t y, uint16_t color) {
    if (!s) return;
    char buf[11];
    size_t n = strlen(s);
    if (n > 10) {                 // 64px / 6px = 10 字符，超出就截断
        memcpy(buf, s, 10);
        buf[10] = 0;
        s = buf;
        n = 10;
    }
    int w = (int)n * 6;           // Adafruit 默认 5x7 字体, 6px 步进
    int x = (PANEL_W - w) / 2;
    if (x < 0) x = 0;
    g_matrix->setTextColor(color);
    g_matrix->setTextSize(1);
    g_matrix->setCursor(x, y);
    g_matrix->print(s);
}

// ------------------------------------------------------- 背景 / 云 / 时钟 ----
static int s_scene = 0;      // 0=白天 1=黑夜

static inline uint8_t bg_idx(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'V') return (uint8_t)(c - 'A' + 10);
    return 0;
}

// 按真实北京时间选场景（6:00-18:00 白天）
static void pick_scene() {
    uint32_t bj = (clock_now() + 8UL * 3600UL) % 86400UL;
    int hour = (int)(bj / 3600);
    s_scene = (hour >= 6 && hour < 18) ? 0 : 1;
}

// 背景从 y=8 开始画（顶部 8px 留给状态栏）
static void draw_bg() {
    const SceneDef& s = BG_SCENES[s_scene];
    for (int y = 0; y < s.h; y++) {
        const char* row = s.rows[y];
        for (int x = 0; x < s.w; x++) {
            uint8_t i = bg_idx(row[x]);
            g_matrix->drawPixel(x, 8 + y, s.pal[i]);
        }
    }
}

// 三朵云，只有它们会动
static const char* const CL_A[3] = {"...1111...", "..111111..", ".11111111."};
static const char* const CL_B[3] = {"....111111.....", "..1111111111...", "1111111111111."};

struct Cloud { const char* const* shape; uint8_t w, y; float x, sp; };
static Cloud s_clouds[3] = {
    {CL_B, 15, 3, 6.0f, 2.2f},
    {CL_A, 10, 9, 34.0f, 1.4f},
    {CL_B, 15, 15, 52.0f, 1.0f},
};
static uint32_t s_cloud_last = 0;

static void draw_clouds(uint32_t now_ms) {
    float dt = 0;
    if (s_cloud_last) dt = (now_ms - s_cloud_last) / 1000.0f;
    s_cloud_last = now_ms;
    if (dt < 0 || dt > 0.5f) dt = 0;      // 掉帧/重启时不要瞬移

    const uint16_t c_main = (s_scene == 0) ? 0xFFFF : 0x18C6;
    const uint16_t c_shade = (s_scene == 0) ? 0xD6FF : 0x1082;

    for (int i = 0; i < 3; i++) {
        Cloud& cl = s_clouds[i];
        cl.x += cl.sp * dt;
        if (cl.x > PANEL_W + cl.w) cl.x = -(float)cl.w;
        int ox = (int)cl.x;
        for (int yy = 0; yy < 3; yy++) {
            const char* row = cl.shape[yy];
            for (int xx = 0; xx < cl.w; xx++) {
                if (row[xx] != '1') continue;
                int px = ox + xx;
                if (px < 0 || px >= PANEL_W) continue;
                g_matrix->drawPixel(px, 8 + cl.y + yy, (yy == 2) ? c_shade : c_main);
            }
        }
    }
}

static void draw_text_right(const char* s, int16_t y, uint16_t color) {
    if (!s) return;
    int n = (int)strlen(s);
    int x = PANEL_W - n * 6;
    if (x < 0) x = 0;
    g_matrix->setTextColor(color);
    g_matrix->setTextSize(1);
    g_matrix->setCursor(x, y);
    g_matrix->print(s);
}

static const char* short_stage(uint8_t st) {
    switch (st) {
        case ST_EGG:     return "EGG";
        case ST_BABY:    return "BABY";
        case ST_CHILD:   return "CHILD";
        case ST_TEEN:    return "TEEN";
        case ST_ADULT:   return "ADULT";
        case ST_PERFECT: return "ULT";
        default:         return "RIP";
    }
}

// 左下角 HH:MM（北京时间）
static void draw_clock() {
    uint32_t bj = (clock_now() + 8UL * 3600UL) % 86400UL;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u",
             (unsigned)(bj / 3600), (unsigned)((bj % 3600) / 60));
    g_matrix->setTextColor(C_CYAN);
    g_matrix->setTextSize(1);
    g_matrix->setCursor(1, 56);
    g_matrix->print(buf);
}

void ui_set_brightness(uint8_t v) {    s_brightness = v;
    if (g_matrix) g_matrix->setBrightness8(v);
}

uint8_t ui_get_brightness() { return s_brightness; }

void ui_toast(const char* msg) {
    if (!msg) return;
    strncpy(s_toast, msg, sizeof(s_toast) - 1);
    s_toast[sizeof(s_toast) - 1] = 0;
    s_toast_until = millis() + 2000;
}

static uint32_t s_hold_until = 0;

void ui_boot(const char* l1, const char* l2) {
    if (!g_matrix) return;
    g_matrix->fillScreen(C_BLACK);
    draw_text_centered(l1, 20, C_GREEN);
    if (l2) draw_text_centered(l2, 36, C_GRAY);
    g_matrix->flipDMABuffer();
    s_hold_until = millis() + 2600;   // 启动/配网横幅停留 2.6 秒
}

void ui_render(PetState& p, uint32_t now_ms) {
    if (!g_matrix) return;
    if (now_ms < s_hold_until) return;   // 启动横幅期间不覆盖画面

    g_matrix->fillScreen(C_BLACK);

    // ---------------- 背景（白天/黑夜 + 飘云）----------------
    pick_scene();
    draw_bg();
    draw_clouds(now_ms);

    // ---------------- 状态栏: 饥饿心 + 心情条 ----------------
    for (int i = 0; i < 4; i++) {
        draw_heart(1 + i * 6, 1, i < p.hunger);
    }

    // 心情条 34x5
    const int16_t mb_x = 28, mb_y = 1, mb_w = 35, mb_h = 5;
    g_matrix->drawRect(mb_x, mb_y, mb_w, mb_h, C_DIM);
    int fill = (int)((mb_w - 2) * (p.mood > 100 ? 100 : p.mood) / 100);
    uint16_t mc = (p.mood > 60) ? C_GREEN : (p.mood > 30 ? C_AMBER : C_RED);
    if (fill > 0) g_matrix->fillRect(mb_x + 1, mb_y + 1, fill, mb_h - 2, mc);

    // ---------------- 主体精灵 ----------------
    uint8_t dim = 255;
    if (p.sleeping) dim = 96;
    if (!p.alive) dim = 150;

    // 有 GIF 动画的角色播动画；没有的（机械暴龙兽/战斗暴龙兽/蛋/墓碑）用静态精灵
    const CharId ch = anim_char_for_species(p.species);
    if (ch != CH_NONE) {
        if (anim_char() != ch) anim_play(ch, AN_IDLE, true);
        anim_update(now_ms);
    }
    const SpriteDef* af = anim_frame();
    if (af) {
        sprite_draw(*g_matrix, *af, PANEL_W / 2, 29, 1, dim);
    } else {
        const SpriteDef& sp = SPRITES[pet_sprite_id(p)];
        sprite_draw_bob(*g_matrix, sp, PANEL_W / 2, 34, now_ms, 1, dim);
    }

    // 待响应事件：左上角闪一个感叹号
    if (p.pending != 0 && ((now_ms / 400) & 1)) {
        g_matrix->setTextColor(C_AMBER);
        g_matrix->setTextSize(1);
        g_matrix->setCursor(0, 8);
        g_matrix->print("!");
    }

    // 便便指示（右下角 3 个小块）
    for (int i = 0; i < p.poops && i < 3; i++) {
        g_matrix->fillRect(56 + i * 3, 58, 2, 2, 0x8200);
    }

    // ---------------- 底部：左时钟 / 右阶段 ----------------
    if (now_ms < s_toast_until) {
        draw_text_centered(s_toast, PANEL_H - 7, C_WHITE);
    } else {
        draw_clock();
        if (!p.alive) {
            draw_text_right("RIP", PANEL_H - 7, C_GRAY);
        } else if (p.sleeping) {
            draw_text_right("ZZZ", PANEL_H - 7, C_CYAN);
        } else {
            draw_text_right(short_stage(p.stage), PANEL_H - 7, C_WHITE);
        }
    }

    g_matrix->flipDMABuffer();
}
