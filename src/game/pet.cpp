#include "pet.h"
#include "../config.h"
#include "../assets/sprites.h"
#include <string.h>

#define PET_MAGIC 0x44494749u   // 'DIGI'
#define PET_VERSION 1

static uint16_t clamp16(int v, int lo, int hi) {
    if (v < lo) return (uint16_t)lo;
    if (v > hi) return (uint16_t)hi;
    return (uint16_t)v;
}

static void copy_msg(char* dst, size_t n, const char* src) {
    if (!dst || n == 0) return;
    strncpy(dst, src, n - 1);
    dst[n - 1] = 0;
}

const char* pet_stage_name(uint8_t stage) {
    switch (stage) {
        case ST_EGG:     return "EGG";
        case ST_BABY:    return "BABY";
        case ST_CHILD:   return "CHILD";
        case ST_TEEN:    return "TEEN";
        case ST_ADULT:   return "ADULT";
        case ST_PERFECT: return "ULTIMATE";
        case ST_DEAD:    return "DEAD";
        default:         return "?";
    }
}

const char* pet_stage_name_cn(uint8_t stage) {
    switch (stage) {
        case ST_EGG:     return "数码蛋";
        case ST_BABY:    return "幼年期";
        case ST_CHILD:   return "成长期";
        case ST_TEEN:    return "成熟期";
        case ST_ADULT:   return "完全体";
        case ST_PERFECT: return "究极体";
        case ST_DEAD:    return "已死亡";
        default:         return "未知";
    }
}

const char* pet_species_name(uint8_t species) {
    switch (species) {
        case SP_EGG:         return "EGG";
        case SP_SPROUT:      return "SPROUT";
        case SP_LEAFLING:    return "LEAFLING";
        case SP_BLOOMON:     return "BLOOMON";
        case SP_GUARDIAN:    return "GUARDIAN";
        case SP_OMEGA_FIRE:  return "BLAZEDRAKE";
        case SP_OMEGA_WATER: return "TIDELORD";
        case SP_OMEGA_NATURE:return "VERDANT";
        default:             return "UNKNOWN";
    }
}

const char* pet_species_name_cn(uint8_t species) {
    switch (species) {
        case SP_EGG:         return "数码蛋";
        case SP_SPROUT:      return "芽仔";
        case SP_LEAFLING:    return "叶芽兽";
        case SP_BLOOMON:     return "花冠兽";
        case SP_GUARDIAN:    return "守护兽";
        case SP_OMEGA_FIRE:  return "炽焰龙";
        case SP_OMEGA_WATER: return "深潮兽";
        case SP_OMEGA_NATURE:return "森罗兽";
        default:             return "未知";
    }
}

uint8_t pet_sprite_id(const PetState& p) {
    if (p.stage == ST_DEAD) return SPR_ID_DEAD;
    switch (p.species) {
        case SP_EGG:         return SPR_ID_EGG;
        case SP_SPROUT:      return SPR_ID_BABY;
        case SP_LEAFLING:    return SPR_ID_CHILD;
        case SP_BLOOMON:     return SPR_ID_TEEN;
        case SP_GUARDIAN:    return SPR_ID_TEEN;      // v0.2 独立素材
        case SP_OMEGA_FIRE:  return SPR_ID_ULT_FIRE;
        case SP_OMEGA_WATER: return SPR_ID_ULT_WATER;
        case SP_OMEGA_NATURE:return SPR_ID_ULT_NATURE;
        default:             return SPR_ID_EGG;
    }
}

void pet_new(PetState& p) {
    memset(&p, 0, sizeof(p));
    p.magic = PET_MAGIC;
    p.version = PET_VERSION;
    p.stage = ST_EGG;
    p.species = SP_EGG;
    p.alive = 1;
    p.sleeping = 0;
    p.hunger = 4;
    p.mood = 80;
    p.hp = 100;
    p.atk = 5;
    p.def = 5;
    p.weight = 10;
    p.discipline = 50;
}

// ------------------------------------------------------------------ 进化 ----
// 表驱动：条件全部满足才进化，一步一个阶段。
static bool try_evolve(PetState& p, char* msg, size_t n) {
    switch (p.stage) {
        case ST_EGG:
            if (p.stage_age_s >= EGG_HATCH_S * TIME_SCALE) {
                p.stage = ST_BABY;
                p.species = SP_SPROUT;
                p.stage_age_s = 0;
                p.hp = 100;
                p.atk = 8; p.def = 6; p.weight = 14;
                copy_msg(msg, n, "HATCHED!");
                return true;
            }
            break;

        case ST_BABY:
            if (p.stage_age_s >= BABY_EVOLVE_S * TIME_SCALE) {
                p.stage = ST_CHILD;
                p.species = SP_LEAFLING;
                p.stage_age_s = 0;
                p.atk += 4; p.def += 3; p.weight += 10;
                // 失误多 -> 属性惩罚
                if (p.mistakes > 4) { p.atk -= 2; p.mood -= 10; }
                copy_msg(msg, n, "EVOLVED!");
                return true;
            }
            break;

        case ST_CHILD:
            if (p.stage_age_s >= CHILD_EVOLVE_S * TIME_SCALE && p.wins >= 1) {
                p.stage = ST_TEEN;
                p.species = SP_BLOOMON;
                p.stage_age_s = 0;
                p.atk += 6; p.def += 5; p.weight += 16;
                copy_msg(msg, n, "EVOLVED!!");
                return true;
            }
            break;

        case ST_TEEN:
            if (p.stage_age_s >= TEEN_EVOLVE_S * TIME_SCALE && p.wins >= 3) {
                p.stage = ST_ADULT;
                p.species = SP_GUARDIAN;
                p.stage_age_s = 0;
                p.atk += 10; p.def += 8; p.weight += 30;
                copy_msg(msg, n, "ADULT!");
                return true;
            }
            break;

        case ST_ADULT:
            // 完全体 → 究极体：需要 8 场胜利 + 一张进化卡
            // 卡的属性决定进化成哪一只究极体（火 > 水 > 自然）
            if (p.wins >= 8 && (p.items & (ITEM_CARD_FIRE | ITEM_CARD_WATER | ITEM_CARD_NATURE))) {
                p.stage = ST_PERFECT;
                if (p.items & ITEM_CARD_FIRE) {
                    p.species = SP_OMEGA_FIRE;
                } else if (p.items & ITEM_CARD_WATER) {
                    p.species = SP_OMEGA_WATER;
                } else {
                    p.species = SP_OMEGA_NATURE;
                }
                p.stage_age_s = 0;
                p.atk += 15; p.def += 12;
                copy_msg(msg, n, "ULTIMATE!");
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

// -------------------------------------------------------------------- tick ----
void pet_tick(PetState& p, uint32_t dt_s) {
    if (!p.alive || p.stage == ST_DEAD) return;
    if (dt_s == 0) return;

    const uint32_t scaled = dt_s * TIME_SCALE;
    p.age_s += scaled;
    p.stage_age_s += scaled;
    p.play_s += dt_s;

    // 饥饿与心情
    if (!p.sleeping) {
        p.acc_hunger = (uint16_t)(p.acc_hunger + dt_s);
        while (p.acc_hunger >= HUNGER_DROP_S) {
            p.acc_hunger -= HUNGER_DROP_S;
            if (p.hunger > 0) {
                p.hunger--;
            } else {
                p.mood = clamp16((int)p.mood - 3, 0, 100);
                p.mistakes++;
            }
        }
        if (p.hunger == 0) {
            p.acc_mood = (uint16_t)(p.acc_mood + dt_s);
            if (p.acc_mood >= MOOD_DECAY_S) {
                p.acc_mood = 0;
                p.mood = clamp16((int)p.mood - 1, 0, 100);
            }
        }
        // 便便
        p.acc_poop = (uint16_t)(p.acc_poop + dt_s);
        if (p.acc_poop >= POOP_INTERVAL_S) {
            p.acc_poop = 0;
            if (p.poops < 3) p.poops++;
        }
    } else {
        // 睡觉：心情缓慢恢复
        p.acc_mood = (uint16_t)(p.acc_mood + dt_s);
        if (p.acc_mood >= 30) {
            p.acc_mood = 0;
            p.mood = clamp16((int)p.mood + 1, 0, 100);
            p.hp = clamp16((int)p.hp + 1, 0, 100);
        }
    }

    // 便便不清理会掉心情
    if (p.poops > 1) {
        static uint16_t dirty_acc = 0;
        dirty_acc = (uint16_t)(dirty_acc + dt_s);
        if (dirty_acc >= 20) {
            dirty_acc = 0;
            p.mood = clamp16((int)p.mood - 1, 0, 100);
        }
    }

    char ev[24];
    if (try_evolve(p, ev, sizeof(ev))) {
        // 进化本身不在这里发通知，交给 UI 层轮询 stage 变化
    }

    // 死亡判定：长期 0 心 0 心情
    if (p.hunger == 0 && p.mood == 0) {
        p.hp = clamp16((int)p.hp - 1, 0, 100);
        if (p.hp == 0) {
            p.stage = ST_DEAD;
            p.species = SP_EGG;
            p.alive = 0;
        }
    }
}

// ------------------------------------------------------------------ 动作 ----
bool pet_action(PetState& p, const char* cmd, char* msg, size_t n) {
    if (!cmd) return false;

    if (!p.alive) {
        copy_msg(msg, n, "GONE...");
        return false;
    }

    if (!strcmp(cmd, "feed")) {
        if (p.sleeping) { copy_msg(msg, n, "ZZZ..."); return false; }
        if (p.hunger >= 4) { copy_msg(msg, n, "NOT HUNGRY"); return false; }
        p.hunger++;
        p.weight = clamp16((int)p.weight + 1, 0, 999);
        p.mood = clamp16((int)p.mood + 3, 0, 100);
        copy_msg(msg, n, "YUM!");
        return true;
    }

    if (!strcmp(cmd, "clean")) {
        if (p.poops == 0) { copy_msg(msg, n, "ITS CLEAN"); return false; }
        p.poops = 0;
        p.mood = clamp16((int)p.mood + 5, 0, 100);
        copy_msg(msg, n, "CLEAN!");
        return true;
    }

    if (!strcmp(cmd, "train")) {
        if (p.sleeping) { copy_msg(msg, n, "ZZZ..."); return false; }
        if (p.hunger == 0) { copy_msg(msg, n, "TOO HUNGRY"); p.mistakes++; return false; }
        if (p.hp < 10) { copy_msg(msg, n, "TOO TIRED"); return false; }
        p.atk = clamp16((int)p.atk + 1, 1, 99);
        p.def = clamp16((int)p.def + (random(0, 2)), 1, 99);
        p.weight = clamp16((int)p.weight - 1, 1, 999);
        p.hp = clamp16((int)p.hp - 3, 0, 100);
        p.mood = clamp16((int)p.mood - 2, 0, 100);
        copy_msg(msg, n, "TRAINED!");
        return true;
    }

    if (!strcmp(cmd, "battle")) {
        if (p.sleeping) { copy_msg(msg, n, "ZZZ..."); return false; }
        if (p.hp < 15) { copy_msg(msg, n, "TOO TIRED"); return false; }
        if (p.hunger == 0) { copy_msg(msg, n, "TOO HUNGRY"); return false; }

        // 对手强度随阶段提升；胜负由属性 + 运气决定
        int power = p.atk * 2 + p.def + (int)random(-6, 7);
        int enemy = 18 + p.stage * 9 + (int)random(-8, 9);
        bool win = power >= enemy;

        if (win) {
            p.wins++;
            p.mood = clamp16((int)p.mood + 10, 0, 100);
            p.atk = clamp16((int)p.atk + 1, 1, 99);
            p.hp = clamp16((int)p.hp - (int)random(6, 14), 0, 100);
            copy_msg(msg, n, "YOU WIN!");
        } else {
            p.losses++;
            p.mood = clamp16((int)p.mood - 8, 0, 100);
            p.hp = clamp16((int)p.hp - (int)random(14, 26), 0, 100);
            p.mistakes++;
            copy_msg(msg, n, "YOU LOSE..");
        }

        if (p.hp == 0) {
            p.stage = ST_DEAD;
            p.alive = 0;
            copy_msg(msg, n, "KO...");
        }
        return true;
    }

    if (!strcmp(cmd, "sleep")) {
        if (p.sleeping) { copy_msg(msg, n, "ASLEEP"); return false; }
        p.sleeping = 1;
        copy_msg(msg, n, "GOOD NIGHT");
        return true;
    }

    if (!strcmp(cmd, "wake")) {
        if (!p.sleeping) { copy_msg(msg, n, "AWAKE"); return false; }
        p.sleeping = 0;
        p.acc_hunger = 0;
        copy_msg(msg, n, "GOOD DAY");
        return true;
    }

    if (!strcmp(cmd, "evolve")) {   // 调试用：立刻结算一次进化条件
        if (try_evolve(p, msg, n)) return true;
        copy_msg(msg, n, "NOT READY");
        return false;
    }

    copy_msg(msg, n, "UNKNOWN");
    return false;
}

// ------------------------------------------------------------------ 卡片 ----
bool pet_apply_card(PetState& p, const char* id, char* msg, size_t n) {
    if (!id) return false;

    if (!strcmp(id, "fire")) {
        p.items |= ITEM_CARD_FIRE;
        copy_msg(msg, n, "FIRE CARD");
        return true;
    }
    if (!strcmp(id, "water")) {
        p.items |= ITEM_CARD_WATER;
        copy_msg(msg, n, "WATER CARD");
        return true;
    }
    if (!strcmp(id, "nature")) {
        p.items |= ITEM_CARD_NATURE;
        copy_msg(msg, n, "NATURE!");
        return true;
    }
    if (!strcmp(id, "power")) {
        if (p.items & ITEM_STRENGTH) { copy_msg(msg, n, "USED"); return false; }
        p.items |= ITEM_STRENGTH;
        p.atk = clamp16((int)p.atk + 5, 1, 99);
        copy_msg(msg, n, "POWER UP!");
        return true;
    }
    if (!strcmp(id, "guard")) {
        if (p.items & ITEM_GUARD) { copy_msg(msg, n, "USED"); return false; }
        p.items |= ITEM_GUARD;
        p.def = clamp16((int)p.def + 5, 1, 99);
        copy_msg(msg, n, "GUARD UP!");
        return true;
    }
    if (!strcmp(id, "heal")) {
        p.hp = 100;
        p.mood = 100;
        copy_msg(msg, n, "FULL HEAL");
        return true;
    }
    if (!strcmp(id, "revive")) {
        if (p.alive) { copy_msg(msg, n, "NO NEED"); return false; }
        p.alive = 1;
        p.stage = ST_BABY;
        p.species = SP_SPROUT;
        p.stage_age_s = 0;
        p.hp = 60;
        p.mood = 60;
        p.hunger = 3;
        p.poops = 0;
        p.items |= ITEM_REVIVE;
        copy_msg(msg, n, "REVIVED!");
        return true;
    }

    copy_msg(msg, n, "NO CARD");
    return false;
}
