#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------- 时钟 ----
void     clock_begin();
uint32_t clock_now();          // 当前 epoch 秒（未联网时退化为开机秒数，节奏不变）
bool     clock_synced();
void     clock_warp_hours(int hours);   // 调试：快进，用于验证 15~30 天周期
int32_t  clock_warp_offset();
uint32_t clock_take_jump();    // NTP 校准时的时间跳变量（取走即清零），用于重定宠物时间戳
