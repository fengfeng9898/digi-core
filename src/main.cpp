// ============================================================================
//  DIGI-CORE — main
//  ESP32-DevKitC-32E + HUB75E 64x64 : 数码兽养成机
//  无按键：操作来自局域网网页 / 飞书桥接
//  时间：NTP 校准，养成周期按真实天数走
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "panel.h"
#include "game/pet.h"
#include "game/save.h"
#include "game/clock.h"
#include "ui/ui.h"
#include "net/net.h"

MatrixPanel_I2S_DMA* g_matrix = nullptr;
PetState g_pet;

static uint32_t s_last_draw = 0;
static uint32_t s_last_save = 0;
static uint32_t s_btn_down  = 0;

// 后台事件 -> 屏幕提示。动作类事件由 net 层负责提示，避免重复。
void app_on_event(uint8_t kind) {
    switch (kind) {
        case EV_EVOLVE:  ui_toast("EVOLVED!"); save_store(g_pet); break;
        case EV_POOP:    ui_toast("POOP!");    break;
        case EV_SICK:    ui_toast("SICK...");  break;
        case EV_FOUND:   ui_toast("FOUND!");   break;
        case EV_CALL:    ui_toast("HEY HEY!"); break;
        case EV_IGNORED: ui_toast("IGNORED");  break;
        case EV_DEATH:   ui_toast("GAME OVER"); save_store(g_pet); break;
        default: break;
    }
}

static_assert(PANEL_LATCH_BLANK >= 1, "latch_blanking 必须 >=1，0 会让库 uint8 下溢");

void setup() {
    Serial.begin(115200);
    delay(150);
    randomSeed(esp_random());
    clock_begin();

    Serial.printf("\n=== %s %s ===\n", FW_NAME, FW_VERSION);

    // ---------------- 面板 ----------------
    HUB75_I2S_CFG::i2s_pins pins = {
        PIN_R1, PIN_G1, PIN_B1,
        PIN_R2, PIN_G2, PIN_B2,
        PIN_A,  PIN_B,  PIN_C, PIN_D, PIN_E,
        PIN_LAT, PIN_OE, PIN_CLK
    };

    HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, PANEL_CHAIN, pins,
                      HUB75_I2S_CFG::SHIFTREG,
                      /*double_buff=*/true,
                      HUB75_I2S_CFG::HZ_15M,
                      /*latch_blanking=*/PANEL_LATCH_BLANK,
                      /*clkphase=*/true,
                      PANEL_MIN_REFRESH);

    g_matrix = new MatrixPanel_I2S_DMA(cfg);
    if (!g_matrix->begin()) {
        Serial.println("[panel] begin() FAILED — 检查接线/供电");
    }
    g_matrix->setBrightness8(PANEL_BRIGHTNESS);
    g_matrix->fillScreen(0);
    Serial.printf("[panel] %dx%d depth=%d refresh=%dHz freeheap=%u\n",
                  PANEL_W, PANEL_H, PANEL_COLOR_DEPTH,
                  g_matrix->calculated_refresh_rate, ESP.getFreeHeap());

    ui_boot(FW_NAME, "BOOT...");

    // ---------------- 存档 ----------------
    bool loaded = save_load(g_pet, clock_now());
    Serial.printf("[save] %s stage=%s species=%s\n",
                  loaded ? "loaded" : "new",
                  pet_stage_name(g_pet.stage),
                  pet_species_name(g_pet.species));

    pinMode(PIN_BTN_RESET, INPUT_PULLUP);

    // ---------------- 网络 ----------------
    net_begin(&g_pet);

    s_last_draw = millis();
    s_last_save = millis();
    Serial.printf("[boot] done freeheap=%u synced=%d\n", ESP.getFreeHeap(), clock_synced());
}

void loop() {
    net_loop();

    // ---- 宠物主循环：按 epoch 结算（1 秒粒度由 pet_tick 内部处理）----
    uint32_t now = clock_now();
    uint32_t jump = clock_take_jump();     // NTP 校准导致的跳变
    if (jump) {
        pet_rebase(g_pet, jump);
        Serial.printf("[clock] ntp rebase %lus\n", (unsigned long)jump);
    }
    pet_tick(g_pet, now);

    const uint32_t ms = millis();

    // ---- 15 FPS 渲染 ----
    if (ms - s_last_draw >= 66) {
        s_last_draw = ms;
        ui_render(g_pet, ms);
    }

    // ---- 5 分钟兜底存档 ----
    if (ms - s_last_save >= 300000u) {
        s_last_save = ms;
        save_store(g_pet);
    }

    // ---- BOOT 键长按 3s = 清空 WiFi 配置 ----
    if (digitalRead(PIN_BTN_RESET) == LOW) {
        if (s_btn_down == 0) {
            s_btn_down = ms ? ms : 1;
        } else if (ms - s_btn_down > 3000u) {
            Serial.println("[btn] wifi reset");
            save_wifi_wipe();
            delay(120);
            ESP.restart();
        }
    } else {
        s_btn_down = 0;
    }

    delay(50);
}
