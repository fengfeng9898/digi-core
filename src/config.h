// ============================================================================
//  config.h — 一块板子的全部硬件/玩法参数都在这里
//  DIGI-CORE : ESP32-DevKitC-32E + HUB75E 64x64
// ============================================================================
#pragma once
#include <Arduino.h>

#define FW_NAME     "DIGI-CORE"
#define FW_VERSION  "0.1.0"

// ------------------------------------------------------------------ 屏幕 ----
#define PANEL_W 64
#define PANEL_H 64
#define PANEL_CHAIN 1

// 64x64 是 1/32 扫描: 每帧并行输出 2 行 -> ROWS_PER_FRAME = 64/2 = 32
#define PANEL_COLOR_DEPTH 6      // 5..7 都行; 6 在 16MHz 下约 180Hz 刷新
#define PANEL_MIN_REFRESH 120
#define PANEL_BRIGHTNESS  70     // 0..255, 首次点亮建议 <= 80

// ------------------------------------------------------------------ 引脚 ----
// 避开: GPIO6-11(Flash) / 34-39(仅输入) / 0,2,12,15(strapping)
#define PIN_R1   25
#define PIN_G1   26
#define PIN_B1   27
#define PIN_R2   14
#define PIN_G2   13
#define PIN_B2    4
#define PIN_A    23
#define PIN_B    19
#define PIN_C     5
#define PIN_D    17
#define PIN_E    16     // 64x64 必接, 不接只有一半画面
#define PIN_LAT  21
#define PIN_OE   22
#define PIN_CLK  18

#define PIN_BUZZER   2
#define PIN_BTN_RESET 0   // 板载 BOOT 键: 长按 3 秒清空 WiFi 配置

// ------------------------------------------------------------------ 网络 ----
#define WIFI_AP_PREFIX  "DIGI-CORE-"
#define WIFI_AP_PASS    "digipet1"
#define WIFI_STA_TIMEOUT_MS 20000

// ------------------------------------------------------------ 玩法节奏 ----
// 目标：蛋 → 究极体 = 15 ~ 30 天真实时间。
// 各阶段时长不写死，在 pet.cpp 的 stage_duration() 里按区间随机，
// 所以每次养出来的节奏都不一样。
#define HUNGER_DROP_S   7200      // 每 2 小时掉 1 颗心（4 心 = 8 小时，一天看 2~3 次够用）
#define MOOD_DECAY_S    600       // 饥饿/脏污时每 10 分钟掉 1 心情
#define HP_REGEN_S      1800      // 吃饱时每 30 分钟回 1 体力
#define HP_SICK_S       600       // 生病时每 10 分钟掉 1 体力

// ------------------------------------------------------------------ 存档 ----
#define SAVE_NS       "digi"
#define SAVE_KEY      "pet"
#define SAVE_WIFI_SSID "wifi_ssid"
#define SAVE_WIFI_PASS "wifi_pass"
