#include "ui.h"
#include "../config.h"
#include "../panel.h"
#include "../gfx/sprite.h"
#include "../gfx/anim.h"
#include "../game/clock.h"
#include "../assets/bg_data.h"
#include "screen.h"
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

// HUD 缓存：值没变就一个像素都不写
static uint16_t s_top_key = 0xFFFF;
static uint32_t s_bot_key = 0xFFFFFFFF;

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

// 按真实北京时间选场景（6:00-18:00 白天）
static uint8_t current_scene() {
    uint32_t bj = (clock_now() + 8UL * 3600UL) % 86400UL;
    int hour = (int)(bj / 3600);
    return (hour >= 6 && hour < 18) ? 0 : 1;
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

static void draw_clock() {
    uint32_t bj = (clock_now() + 8UL * 3600UL) % 86400UL;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u",
             (unsigned)(bj / 3600), (unsigned)((bj % 3600) / 60));
    screen_text_at(buf, 1, 56, C_CYAN);
}

void ui_invalidate() {
    screen_invalidate();
    s_top_key = 0xFFFF;
    s_bot_key = 0xFFFFFFFF;
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
    s_bot_key = 0xFFFFFFFF;      // 强制刷新底栏
}

static uint32_t s_hold_until = 0;

void ui_boot(const char* l1, const char* l2) {
    if (!g_matrix) return;
    screen_init();
    screen_clear();
    screen_text_center(l1, 20, C_GREEN);
    if (l2) screen_text_center(l2, 36, C_GRAY);
    s_hold_until = millis() + 2600;   // 启动/配网横幅停留 2.6 秒
    s_top_key = 0xFFFF;
    s_bot_key = 0xFFFFFFFF;
}

void ui_render(PetState& p, uint32_t now_ms) {
    if (!g_matrix) return;
    if (now_ms < s_hold_until) return;   // 启动横幅期间不覆盖画面

    // ---- 背景（场景切换才整屏重建）+ 云（跨像素才重画）----
    const uint8_t scene = current_scene();
    screen_scene(scene);
    screen_clouds(now_ms, scene);

    // ---- 顶部状态栏：只有饥饿/心情变化才重画 ----
    uint16_t topk = (uint16_t)p.hunger | ((uint16_t)(p.mood > 100 ? 100 : p.mood) << 4);
    if (topk != s_top_key) {
        s_top_key = topk;
        screen_clear_band(0, 8);
        for (int i = 0; i < 4; i++) draw_heart(1 + i * 6, 1, i < p.hunger);

        const int16_t mb_x = 28, mb_y = 1, mb_w = 35, mb_h = 5;
        g_matrix->drawRect(mb_x, mb_y, mb_w, mb_h, C_DIM);
        int fill = (int)((mb_w - 2) * (p.mood > 100 ? 100 : p.mood) / 100);
        uint16_t mc = (p.mood > 60) ? C_GREEN : (p.mood > 30 ? C_AMBER : C_RED);
        if (fill > 0) g_matrix->fillRect(mb_x + 1, mb_y + 1, fill, mb_h - 2, mc);
    }

    // ---- 主体：像素级差分，不变化的帧一个像素都不写 ----
    uint8_t dim = 255;
    if (p.sleeping) dim = 96;
    if (!p.alive) dim = 150;

    const CharId ch = anim_char_for_species(p.species);
    if (ch != CH_NONE) {
        if (anim_char() != ch) anim_play(ch, AN_IDLE, true);
        anim_update(now_ms);
    }
    const SpriteDef* af = anim_frame();
    if (af) {
        screen_sprite(af, PANEL_W / 2, 29, dim);
    } else {
        const SpriteDef& sp = SPRITES[pet_sprite_id(p)];
        int8_t bob = ((now_ms / 1000) & 1) ? 1 : 0;
        screen_sprite(&sp, PANEL_W / 2, 34 + bob, dim);
    }

    // ---- 底部：时钟 / 阶段 / toast，变化时才重画 ----
    const bool toast_on = (now_ms < s_toast_until && s_toast[0] != 0);
    uint32_t bj = (clock_now() + 8UL * 3600UL) % 86400UL;
    uint32_t botk = (bj / 60)
                  | ((uint32_t)p.stage << 20)
                  | ((uint32_t)p.sleeping << 24)
                  | ((uint32_t)p.alive << 25)
                  | ((uint32_t)p.poops << 26)
                  | ((uint32_t)(toast_on ? 1 : 0) << 28);
    if (botk != s_bot_key) {
        s_bot_key = botk;
        screen_clear_band(56, 8);
        if (toast_on) {
            screen_text_center(s_toast, 56, C_WHITE);
        } else {
            draw_clock();
            if (!p.alive) {
                screen_text_at("RIP", 52, 56, C_GRAY);
            } else if (p.sleeping) {
                screen_text_at("ZZZ", 52, 56, C_CYAN);
            } else {
                const char* st = short_stage(p.stage);
                int n = (int)strlen(st);
                screen_text_at(st, 63 - n * 6, 56, C_WHITE);
            }
        }
        for (int i = 0; i < p.poops && i < 3; i++) {
            screen_rect(56 + i * 3, 58, 2, 2, 0x8200);
        }
    }
}
