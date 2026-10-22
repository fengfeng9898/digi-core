#pragma once
#include <Arduino.h>
#include "game/pet.h"

void ui_boot(const char* l1, const char* l2);
void ui_toast(const char* msg);

// 强制整屏重画（切旋转方向后用）
void ui_invalidate();                 // 强制整屏重画
void ui_render(PetState& p, uint32_t now_ms);   // 每帧调用
void ui_set_brightness(uint8_t v);
uint8_t ui_get_brightness();
