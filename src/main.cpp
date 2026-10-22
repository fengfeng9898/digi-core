// ============================================================================
//  DIGI-CORE — main
//  ESP32-DevKitC-32E + HUB75E 64x64 : 数码兽养成机
//  无按键设计：所有操作来自局域网网页 / 飞书桥接
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "panel.h"
#include "game/pet.h"
#include "game/save.h"
#include "ui/ui.h"
#include "net/net.h"

MatrixPanel_I2S_DMA* g_matrix = nullptr;
PetState g_pet;

static uint32_t s_last_tick   = 0;
static uint32_t s_last_draw   = 0;
static uint32_t s_last_save   = 0;
static uint8_t  s_last_stage  = 255;
static uint32_t s_btn_down    = 0;

void setup() {
    Serial.begin(115200);
    delay(150);
    Serial.printf("\n=== %s %s ===\n", FW_NAME, FW_VERSION);

    // ---------------- 面板 ----------------
    HUB75_I2S_CFG::i2s_pins pins = {
        PIN_R1, PIN_G1, PIN_B1,
        PIN_R2, PIN_G2, PIN_B2,
        PIN_A,  PIN_B,  PIN_C, PIN_D, PIN_E,
        PIN_LAT, PIN_OE, PIN_CLK
    };

    // 3.0.5 API: 色深是编译期 PIXEL_COLOR_DEPTH_BITS（见 platformio.ini）
    HUB75_I2S_CFG cfg(PANEL_W, PANEL_H, PANEL_CHAIN, pins,
                      HUB75_I2S_CFG::SHIFTREG,
                      /*double_buff=*/true,
                      HUB75_I2S_CFG::HZ_15M,
                      /*latch_blanking=*/0,
                      /*clkphase=*/true,
                      PANEL_MIN_REFRESH);

    g_matrix = new MatrixPanel_I2S_DMA(cfg);
    if (!g_matrix->begin()) {
        Serial.println("[panel] begin() FAILED — 检查接线/供电");
    }
    g_matrix->setBrightness8(PANEL_BRIGHTNESS);
    g_matrix->fillScreen(0);
    Serial.printf("[panel] %dx%d chain=%d depth=%d refresh=%dHz freeheap=%u\n",
                  PANEL_W, PANEL_H, PANEL_CHAIN, PANEL_COLOR_DEPTH,
                  g_matrix->calculated_refresh_rate, ESP.getFreeHeap());

    ui_boot(FW_NAME, "BOOT...");

    // ---------------- 存档 ----------------
    bool loaded = save_load(g_pet);
    Serial.printf("[save] %s  stage=%s species=%s age=%lus\n",
                  loaded ? "loaded" : "new",
                  pet_stage_name(g_pet.stage),
                  pet_species_name(g_pet.species),
                  (unsigned long)g_pet.age_s);
    s_last_stage = g_pet.stage;

    // ---------------- 恢复出厂按键 ----------------
    pinMode(PIN_BTN_RESET, INPUT_PULLUP);

    // ---------------- 网络 ----------------
    net_begin(&g_pet);

    s_last_tick = millis();
    s_last_save = millis();
    Serial.printf("[boot] done, freeheap=%u\n", ESP.getFreeHeap());
}

void loop() {
    net_loop();
    const uint32_t now = millis();

    // ---- 1 Hz 逻辑 tick（累计秒数，避免 millis 抖动丢帧）----
    uint32_t dt = (now - s_last_tick) / 1000u;
    if (dt >= 1) {
        s_last_tick += dt * 1000u;
        pet_tick(g_pet, dt);

        if (g_pet.stage != s_last_stage) {
            s_last_stage = g_pet.stage;
            ui_toast(g_pet.alive ? "EVOLVED!" : "GAME OVER");
            save_store(g_pet);
            Serial.printf("[pet] -> stage=%s\n", pet_stage_name(g_pet.stage));
        }
    }

    // ---- 15 FPS 渲染 ----
    if (now - s_last_draw >= 66) {
        s_last_draw = now;
        ui_render(g_pet, now);
    }

    // ---- 30s 自动存档 ----
    if (now - s_last_save >= 30000u) {
        s_last_save = now;
        save_store(g_pet);
    }

    // ---- BOOT 键长按 3s = 清空 WiFi 配置（无按键设计的最后保险）----
    if (digitalRead(PIN_BTN_RESET) == LOW) {
        if (s_btn_down == 0) {
            s_btn_down = now ? now : 1;
        } else if (now - s_btn_down > 3000u) {
            Serial.println("[btn] factory wifi reset");
            save_wifi_wipe();
            delay(120);
            ESP.restart();
        }
    } else {
        s_btn_down = 0;
    }

    delay(2);
}
