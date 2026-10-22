#include "ui.h"
#include "../config.h"
#include "../panel.h"
#include "../gfx/sprite.h"
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

void ui_set_brightness(uint8_t v) {
    s_brightness = v;
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
    const SpriteDef& sp = SPRITES[pet_sprite_id(p)];
    uint8_t dim = 255;
    if (p.sleeping) dim = 96;
    if (!p.alive) dim = 150;

    sprite_draw_bob(*g_matrix, sp, PANEL_W / 2, 34, now_ms, 1, dim);

    // 便便指示（右下角 3 个小块）
    for (int i = 0; i < p.poops && i < 3; i++) {
        g_matrix->fillRect(56 + i * 3, 58, 2, 2, 0x8200);
    }

    // ---------------- 底部文字 ----------------
    if (now_ms < s_toast_until) {
        draw_text_centered(s_toast, PANEL_H - 7, C_WHITE);
    } else if (!p.alive) {
        draw_text_centered("R.I.P.", PANEL_H - 7, C_GRAY);
    } else if (p.sleeping) {
        draw_text_centered("ZZZ...", PANEL_H - 7, C_CYAN);
    } else if (p.stage == ST_EGG) {
        char buf[12];
        int left = (int)(EGG_HATCH_S * TIME_SCALE) - (int)p.stage_age_s;
        if (left < 0) left = 0;
        snprintf(buf, sizeof(buf), "EGG %d S", left);
        draw_text_centered(buf, PANEL_H - 7, C_CYAN);
    } else {
        draw_text_centered(pet_stage_name(p.stage), PANEL_H - 7, C_GRAY);
    }

    g_matrix->flipDMABuffer();
}
