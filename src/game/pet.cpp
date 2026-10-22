#include "pet.h"
#include "../config.h"
#include "../assets/sprites.h"
#include <string.h>

#define PET_MAGIC   0x44494749u   // 'DIGI'
#define PET_VERSION 2             // v2: epoch 时间基座 + 随机事件
#define D 86400u
#define H 3600u
#define MN 60u

// main.cpp 实现：把事件映射成屏幕提示 / 存档
extern void app_on_event(uint8_t kind);

// ---------------------------------------------------------------- 工具 ----
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

static uint32_t rr(uint32_t lo, uint32_t hi) {
    if (hi <= lo) return lo;
    return lo + (uint32_t)random(0, (long)(hi - lo));
}

// ---------------------------------------------------------------- 事件日志 ----
struct EvRec { uint32_t t; uint8_t k; };
static EvRec  s_log[16];
static uint8_t s_head = 0, s_count = 0;

static void log_ev(uint8_t k, uint32_t now) {
    s_log[s_head].t = now;
    s_log[s_head].k = k;
    s_head = (uint8_t)((s_head + 1) % 16);
    if (s_count < 16) s_count++;
    app_on_event(k);
}

int pet_log_count() { return s_count; }

static int log_index(int idx) {   // idx 0 = 最新
    if (idx < 0 || idx >= s_count) return -1;
    int raw = (int)s_head - 1 - idx;
    while (raw < 0) raw += 16;
    return raw;
}

uint32_t pet_log_time(int idx) {
    int i = log_index(idx);
    return i < 0 ? 0 : s_log[i].t;
}

uint8_t pet_log_kind(int idx) {
    int i = log_index(idx);
    return i < 0 ? 0 : s_log[i].k;
}

// ---------------------------------------------------------------- 名称 ----
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
        case SP_EGG:           return "EGG";
        case SP_KOROMON:       return "KOROMON";
        case SP_AGUMON:        return "AGUMON";
        case SP_GREYMON:       return "GREYMON";
        case SP_METALGREYMON:  return "METALGREY";
        case SP_WARGREYMON:    return "WARGREYMON";
        default:               return "UNKNOWN";
    }
}

const char* pet_species_name_cn(uint8_t species) {
    switch (species) {
        case SP_EGG:           return "数码蛋";
        case SP_KOROMON:       return "滚球兽";
        case SP_AGUMON:        return "亚古兽";
        case SP_GREYMON:       return "暴龙兽";
        case SP_METALGREYMON:  return "机械暴龙兽";
        case SP_WARGREYMON:    return "战斗暴龙兽";
        default:               return "未知";
    }
}

uint8_t pet_sprite_id(const PetState& p) {
    if (p.stage == ST_DEAD) return SPR_ID_DEAD;
    switch (p.species) {
        case SP_EGG:           return SPR_ID_EGG;
        case SP_KOROMON:       return SPR_ID_KOROMON;
        case SP_AGUMON:        return SPR_ID_AGUMON;
        case SP_GREYMON:       return SPR_ID_GREYMON;
        case SP_METALGREYMON:  return SPR_ID_METALGREYMON;
        case SP_WARGREYMON:    return SPR_ID_WARGREYMON;
        default:               return SPR_ID_EGG;
    }
}

uint32_t pet_age_s(const PetState& p, uint32_t now) {
    return (now > p.birth_epoch) ? (now - p.birth_epoch) : 0;
}

uint32_t pet_evolve_in(const PetState& p, uint32_t now) {
    return (p.evolve_at > now) ? (p.evolve_at - now) : 0;
}

uint16_t pet_wins_needed(uint8_t stage) {
    switch (stage) {
        case ST_CHILD: return 1;
        case ST_TEEN:  return 2;
        case ST_ADULT: return 5;
        default:       return 0;
    }
}

// ---------------------------------------------------------------- 节奏 ----
// 目标：蛋 → 究极体 = 15 ~ 30 天真实时间
//   蛋 0.5~1d + 幼年期 2~4d + 成长期 3~6d + 成熟期 3.5~7d + 完全体 6~12d
//   理论下界 15.0 天，上界 30.0 天，中位约 22.5 天
//   再叠加胜场门槛（1/3/8），不训练会更久，训练到位就在区间内
static uint32_t stage_duration(uint8_t stage) {
    switch (stage) {
        case ST_EGG:   return rr(12 * H, 24 * H);
        case ST_BABY:  return rr(2 * D, 4 * D);
        case ST_CHILD: return rr(3 * D, 6 * D);
        case ST_TEEN:  return rr(7 * D / 2, 7 * D);
        case ST_ADULT: return rr(6 * D, 12 * D);
        default:       return rr(1 * H, 3 * H);
    }
}

static uint32_t event_gap() {                 // 20 分钟 ~ 3 小时
    return rr(20 * MN, 180 * MN);
}

void pet_new(PetState& p, uint32_t now) {
    memset(&p, 0, sizeof(p));
    p.magic = PET_MAGIC;
    p.version = PET_VERSION;
    p.stage = ST_EGG;
    p.species = SP_EGG;
    p.alive = 1;
    p.hunger = 4;
    p.mood = 80;
    p.hp = 100;
    p.atk = 5;
    p.def = 5;
    p.weight = 10;
    p.discipline = 50;

    p.birth_epoch = now;
    p.stage_start = now;
    p.evolve_at   = now + stage_duration(ST_EGG);
    p.last_settle = now;
    p.next_event  = now + event_gap();
}

// ---------------------------------------------------------------- 结算 ----
static void die(PetState& p, uint32_t now) {
    if (!p.alive) return;
    p.stage = ST_DEAD;
    p.alive = 0;
    p.species = SP_EGG;
    p.pending = PEND_NONE;
    log_ev(EV_DEATH, now);
}

static void settle_online(PetState& p, uint32_t dt) {
    if (p.sleeping) {
        p.acc_mood = (uint16_t)(p.acc_mood + dt);
        while (p.acc_mood >= 1800) {          // 睡觉：每 30 分钟心情/体力 +1
            p.acc_mood -= 1800;
            p.mood = clamp16((int)p.mood + 1, 0, 100);
            p.hp   = clamp16((int)p.hp + 1, 0, 100);
        }
        return;
    }

    p.acc_hunger = (uint16_t)(p.acc_hunger + dt);
    while (p.acc_hunger >= HUNGER_DROP_S) {
        p.acc_hunger -= HUNGER_DROP_S;
        if (p.hunger > 0) {
            p.hunger--;
        } else {
            p.mood = clamp16((int)p.mood - 4, 0, 100);
        }
    }

    if (p.hunger == 0 || p.poops >= 2) {
        p.acc_mood = (uint16_t)(p.acc_mood + dt);
        while (p.acc_mood >= MOOD_DECAY_S) {
            p.acc_mood -= MOOD_DECAY_S;
            p.mood = clamp16((int)p.mood - 1, 0, 100);
        }
    } else {
        p.acc_mood = 0;
    }

    p.acc_hp = (uint16_t)(p.acc_hp + dt);
    if (p.sick) {
        while (p.acc_hp >= HP_SICK_S) {
            p.acc_hp -= HP_SICK_S;
            p.hp = clamp16((int)p.hp - 1, 0, 100);
        }
    } else if (p.hunger > 0) {
        while (p.acc_hp >= HP_REGEN_S) {
            p.acc_hp -= HP_REGEN_S;
            p.hp = clamp16((int)p.hp + 1, 0, 100);
        }
    }
}

// 离线（关机/断网）期间也要按“托管”处理，否则关机就停表
static void settle_offline(PetState& p, uint32_t dt) {
    uint32_t hungry = dt / HUNGER_DROP_S;
    p.hunger = (hungry >= p.hunger) ? 0 : (uint16_t)(p.hunger - hungry);

    uint32_t mood_loss = dt / 1800;           // 30 分钟掉 1 点
    p.mood = clamp16((int)p.mood - (int)mood_loss, 0, 100);

    uint32_t poop_add = dt / (4 * H);
    if (p.poops + poop_add > 3) p.poops = 3;
    else p.poops = (uint8_t)(p.poops + poop_add);

    p.mistakes = (uint16_t)(p.mistakes + dt / (6 * H));

    if (p.hunger == 0) {
        uint32_t loss = dt / (2 * H);
        p.hp = clamp16((int)p.hp - (int)loss, 0, 100);
    }
    p.acc_hunger = p.acc_mood = p.acc_hp = 0;
}

// ---------------------------------------------------------------- 随机事件 ----
static void fire_event(PetState& p, uint32_t now) {
    p.next_event = now + event_gap();
    if (p.sleeping) return;

    int w_poop  = (p.poops < 3) ? 30 : 0;
    int w_wild  = (p.hp > 25 && p.stage >= ST_CHILD) ? 22 : 0;
    int w_sick  = p.sick ? 0 : (p.poops >= 2 ? 16 : 6);
    int w_found = 12;
    int w_call  = (p.mood < 70) ? 20 : 8;
    int total = w_poop + w_wild + w_sick + w_found + w_call;
    if (total <= 0) return;

    int r = random(0, total);
    if ((r -= w_poop) < 0) {
        p.poops++;
        log_ev(EV_POOP, now);
        return;
    }
    if ((r -= w_wild) < 0) {
        p.pending = PEND_WILD;
        p.pending_until = now + 30 * MN;
        log_ev(EV_WILD, now);
        return;
    }
    if ((r -= w_sick) < 0) {
        p.sick = 1;
        log_ev(EV_SICK, now);
        return;
    }
    if ((r -= w_found) < 0) {
        p.mood = clamp16((int)p.mood + 10, 0, 100);
        p.weight = clamp16((int)p.weight + 2, 1, 999);
        log_ev(EV_FOUND, now);
        return;
    }
    if ((r -= w_call) < 0) {
        p.pending = PEND_CALL;
        p.pending_until = now + 20 * MN;
        log_ev(EV_CALL, now);
        return;
    }
}

// ---------------------------------------------------------------- 进化 ----
static bool try_evolve(PetState& p, uint32_t now) {
    if (now < p.evolve_at) return false;             // 时间闸门没开
    if (p.wins < pet_wins_needed(p.stage)) return false;

    uint8_t next_stage = ST_DEAD;
    uint8_t next_species = SP_EGG;

    switch (p.stage) {
        case ST_EGG:   next_stage = ST_BABY;  next_species = SP_KOROMON;      break;
        case ST_BABY:  next_stage = ST_CHILD; next_species = SP_AGUMON;       break;
        case ST_CHILD: next_stage = ST_TEEN;  next_species = SP_GREYMON;      break;
        case ST_TEEN:  next_stage = ST_ADULT; next_species = SP_METALGREYMON; break;
        case ST_ADULT:
            if (!(p.items & ITEM_CARD_COURAGE)) return false;   // 究极体硬门槛
            next_stage = ST_PERFECT;
            next_species = SP_WARGREYMON;
            break;
        default:
            return false;
    }

    p.stage = next_stage;
    p.species = next_species;
    p.stage_start = now;
    p.evolve_at = now + stage_duration(next_stage);
    p.pending = PEND_NONE;

    switch (next_stage) {
        case ST_BABY:  p.hp = 100; p.atk = 8;  p.def = 6;  p.weight = 14; break;
        case ST_CHILD: p.atk += 4;  p.def += 3;  p.weight += 10;
                       if (p.mistakes > 4) { p.atk -= 2; p.mood -= 10; } break;
        case ST_TEEN:  p.atk += 6;  p.def += 5;  p.weight += 16; break;
        case ST_ADULT: p.atk += 10; p.def += 8;  p.weight += 30; break;
        case ST_PERFECT: p.atk += 15; p.def += 12; break;
        default: break;
    }
    p.hp = clamp16(p.hp + 20, 0, 100);
    log_ev(EV_EVOLVE, now);
    return true;
}

// ---------------------------------------------------------------- tick ----
void pet_rebase(PetState& p, uint32_t delta) {
    if (delta == 0) return;
    p.birth_epoch += delta;
    p.stage_start += delta;
    p.evolve_at   += delta;
    p.last_settle += delta;
    p.next_event  += delta;
    if (p.pending_until) p.pending_until += delta;
}

void pet_tick(PetState& p, uint32_t now) {
    if (!p.alive || p.stage == ST_DEAD) { p.last_settle = now; return; }
    if (p.last_settle == 0) { p.last_settle = now; return; }

    uint32_t dt = (now > p.last_settle) ? (now - p.last_settle) : 0;
    if (dt == 0) return;

    if (dt > 30 * D) {           // 时钟校准跳变，不当作托管
        p.last_settle = now;
        return;
    }
    if (dt > 2 * H) {
        settle_offline(p, dt);
    } else {
        settle_online(p, dt);
    }
    p.last_settle = now;

    // 挂起事件超时未响应
    if (p.pending != PEND_NONE && now > p.pending_until) {
        p.pending = PEND_NONE;
        p.mood = clamp16((int)p.mood - 8, 0, 100);
        p.mistakes++;
        log_ev(EV_IGNORED, now);
    }

    // 随机事件
    int guard = 0;
    while (p.alive && now >= p.next_event && guard++ < 8) {
        fire_event(p, now);
    }

    // 生病恶化
    if (p.sick && p.poops == 0 && p.hunger > 0 && random(0, 100) < 2) {
        p.sick = 0;               // 条件变好有小概率自愈
    }

    if (try_evolve(p, now)) {
        // 进化后重新排事件
        p.next_event = now + event_gap();
    }

    if (p.hp == 0) die(p, now);
}

// ---------------------------------------------------------------- 动作 ----
bool pet_action(PetState& p, uint32_t now, const char* cmd, char* msg, size_t n) {
    if (!cmd) return false;
    if (!p.alive) { copy_msg(msg, n, "GONE..."); return false; }

    if (!strcmp(cmd, "feed")) {
        if (p.sleeping) { copy_msg(msg, n, "ZZZ..."); return false; }
        if (p.hunger >= 4) { copy_msg(msg, n, "NOT HUNGRY"); return false; }
        p.hunger++;
        p.weight = clamp16((int)p.weight + 1, 0, 999);
        p.mood = clamp16((int)p.mood + 3, 0, 100);
        log_ev(EV_FEED, now);
        copy_msg(msg, n, "YUM!");
        return true;
    }

    if (!strcmp(cmd, "clean")) {
        if (p.poops == 0) { copy_msg(msg, n, "ITS CLEAN"); return false; }
        p.poops = 0;
        p.mood = clamp16((int)p.mood + 5, 0, 100);
        log_ev(EV_CLEAN, now);
        copy_msg(msg, n, "CLEAN!");
        return true;
    }

    if (!strcmp(cmd, "pet")) {                 // 安抚：响应“呼唤”
        p.pending = (p.pending == PEND_CALL) ? PEND_NONE : p.pending;
        p.mood = clamp16((int)p.mood + 8, 0, 100);
        log_ev(EV_PET, now);
        copy_msg(msg, n, "HAPPY!");
        return true;
    }

    if (!strcmp(cmd, "train")) {
        if (p.sleeping) { copy_msg(msg, n, "ZZZ..."); return false; }
        if (p.hunger == 0) { copy_msg(msg, n, "TOO HUNGRY"); p.mistakes++; return false; }
        if (p.hp < 10) { copy_msg(msg, n, "TOO TIRED"); return false; }
        p.atk = clamp16((int)p.atk + 1, 1, 99);
        p.def = clamp16((int)p.def + (int)random(0, 2), 1, 99);
        p.weight = clamp16((int)p.weight - 1, 1, 999);
        p.hp = clamp16((int)p.hp - 4, 0, 100);
        p.mood = clamp16((int)p.mood - 2, 0, 100);
        log_ev(EV_TRAIN, now);
        copy_msg(msg, n, "TRAINED!");
        return true;
    }

    if (!strcmp(cmd, "battle")) {
        if (p.sleeping) { copy_msg(msg, n, "ZZZ..."); return false; }
        if (p.hp < 15) { copy_msg(msg, n, "TOO TIRED"); return false; }
        if (p.hunger == 0) { copy_msg(msg, n, "TOO HUNGRY"); return false; }

        bool ambush = (p.pending == PEND_WILD);
        if (ambush) p.pending = PEND_NONE;

        // 对手强度以自身攻防为基准（同水平），胜率约 50%；遭遇战 +4。
        // 早期不会因为 atk 低就必输，避免被「1 胜」门槛拖成几十天。
        int base  = p.atk * 2 + p.def;
        int power = base + (int)random(-6, 7) + (ambush ? 4 : 0);
        int enemy = base + (int)random(-10, 12);
        bool win = power >= enemy;

        if (win) {
            p.wins++;
            p.mood = clamp16((int)p.mood + 10, 0, 100);
            p.atk = clamp16((int)p.atk + 1, 1, 99);
            p.hp = clamp16((int)p.hp - (int)random(4, 10), 0, 100);
            log_ev(EV_WIN, now);
            copy_msg(msg, n, ambush ? "AMBUSH WIN!" : "YOU WIN!");
        } else {
            p.losses++;
            p.mood = clamp16((int)p.mood - 8, 0, 100);
            p.hp = clamp16((int)p.hp - (int)random(10, 18), 0, 100);
            p.mistakes++;
            log_ev(EV_LOSE, now);
            copy_msg(msg, n, "YOU LOSE..");
        }
        if (p.hp == 0) { die(p, now); copy_msg(msg, n, "KO..."); }
        return true;
    }

    if (!strcmp(cmd, "sleep")) {
        if (p.sleeping) { copy_msg(msg, n, "ASLEEP"); return false; }
        p.sleeping = 1;
        p.pending = PEND_NONE;
        log_ev(EV_SLEEP, now);
        copy_msg(msg, n, "GOOD NIGHT");
        return true;
    }

    if (!strcmp(cmd, "wake")) {
        if (!p.sleeping) { copy_msg(msg, n, "AWAKE"); return false; }
        p.sleeping = 0;
        p.acc_hunger = 0;
        log_ev(EV_WAKE, now);
        copy_msg(msg, n, "GOOD DAY");
        return true;
    }

    if (!strcmp(cmd, "evolve")) {              // 调试：立即结算
        if (try_evolve(p, now)) { copy_msg(msg, n, "EVOLVED!"); return true; }
        if (now < p.evolve_at) { copy_msg(msg, n, "TOO EARLY"); return false; }
        if (p.wins < pet_wins_needed(p.stage)) { copy_msg(msg, n, "NEED WINS"); return false; }
        copy_msg(msg, n, "NEED CARD");
        return false;
    }

    copy_msg(msg, n, "UNKNOWN");
    return false;
}

// ---------------------------------------------------------------- 卡片 ----
bool pet_apply_card(PetState& p, uint32_t now, const char* id, char* msg, size_t n) {
    if (!id) return false;

    if (!strcmp(id, "courage")) {
        p.items |= ITEM_CARD_COURAGE;
        log_ev(EV_CARD, now);
        copy_msg(msg, n, "COURAGE!");
        return true;
    }
    if (!strcmp(id, "power")) {
        if (p.items & ITEM_STRENGTH) { copy_msg(msg, n, "USED"); return false; }
        p.items |= ITEM_STRENGTH;
        p.atk = clamp16((int)p.atk + 5, 1, 99);
        log_ev(EV_CARD, now);
        copy_msg(msg, n, "POWER UP!");
        return true;
    }
    if (!strcmp(id, "guard")) {
        if (p.items & ITEM_GUARD) { copy_msg(msg, n, "USED"); return false; }
        p.items |= ITEM_GUARD;
        p.def = clamp16((int)p.def + 5, 1, 99);
        log_ev(EV_CARD, now);
        copy_msg(msg, n, "GUARD UP!");
        return true;
    }
    if (!strcmp(id, "heal")) {
        p.hp = 100;
        p.mood = 100;
        p.sick = 0;
        log_ev(EV_HEAL, now);
        copy_msg(msg, n, "FULL HEAL");
        return true;
    }
    if (!strcmp(id, "revive")) {
        if (p.alive) { copy_msg(msg, n, "NO NEED"); return false; }
        p.alive = 1;
        p.stage = ST_BABY;
        p.species = SP_KOROMON;
        p.stage_start = now;
        p.evolve_at = now + stage_duration(ST_BABY);
        p.last_settle = now;
        p.next_event = now + event_gap();
        p.hp = 60;
        p.mood = 60;
        p.hunger = 3;
        p.poops = 0;
        p.sick = 0;
        p.pending = PEND_NONE;
        p.items |= ITEM_REVIVE;
        log_ev(EV_HEAL, now);
        copy_msg(msg, n, "REVIVED!");
        return true;
    }

    copy_msg(msg, n, "NO CARD");
    return false;
}
