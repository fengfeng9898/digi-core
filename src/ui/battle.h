#pragma once
#include <Arduino.h>
#include "../game/pet.h"

// 回合制战斗演出（切镜）：每镜只有一个角色占满屏
void battle_start(const BattleOutcome& out, uint32_t now_ms);
bool battle_active();
void battle_render(uint32_t now_ms);
