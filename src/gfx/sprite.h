#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "../assets/sprites.h"

// 把索引色 ASCII 精灵画到任意 Adafruit_GFX 画布（本项目就是 HUB75 面板）。
// 规则: '0' = 透明; '1'..'F' = 调色板索引。
// scale: 1/2/3 倍放大（整数倍, 保持像素锐利）。
void sprite_draw(Adafruit_GFX& g, const SpriteDef& s, int16_t cx, int16_t cy,
                 uint8_t scale = 1, uint8_t dim = 255);

// 带垂直偏移的待机浮动（上下 1px 循环），用于 idle 动画。
void sprite_draw_bob(Adafruit_GFX& g, const SpriteDef& s, int16_t cx, int16_t cy,
                     uint32_t now_ms, uint8_t scale = 1, uint8_t dim = 255);
