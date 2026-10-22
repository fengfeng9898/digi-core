#pragma once
#include <Arduino.h>
#include "../gfx/sprite.h"

// 单缓冲脏矩形合成层：
//  - base[] 保存「背景 + 云」的底图，只有场景/云变化才更新
//  - 角色只在上一帧画过的像素上恢复，再画新帧，避免整屏擦除导致闪烁
void screen_init();
void screen_clear();
void screen_scene(uint8_t idx);                       // 0=白天 1=黑夜
void screen_clouds(uint32_t now_ms, uint8_t scene);

void screen_sprite(const SpriteDef* spr, int16_t cx, int16_t cy, uint8_t dim);
void screen_sprite_flash(const SpriteDef* spr, int16_t cx, int16_t cy, uint8_t level);
void screen_forget_sprite();                          // 切场景后调用

// HUD 直接画在面板上（不参与底图）
void screen_text_at(const char* s, int16_t x, int16_t y, uint16_t color);
void screen_text_center(const char* s, int16_t y, uint16_t color);
void screen_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void screen_clear_band(int16_t y, int16_t h);         // 用底图恢复一条横带
