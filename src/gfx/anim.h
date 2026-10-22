#pragma once
#include <Arduino.h>
#include "anim_types.h"

// 播放一段动画。loop=false 时播完自动回到待机。
void anim_play(CharId c, AnimId a, bool loop);

// 战斗序列：先播攻击，再播胜利/受伤，最后回待机。
void anim_play_battle(CharId c, bool win);

// 每帧调用（传入 millis）
void anim_update(uint32_t ms);

// 当前该画哪一帧；返回 nullptr 表示该角色没有动画，调用方用静态精灵兜底
const SpriteDef* anim_frame();

CharId anim_char();

// 种族 -> 动画角色
CharId anim_char_for_species(uint8_t species);
