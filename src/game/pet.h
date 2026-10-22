#pragma once

#define PET_MAGIC   0x44494749u   // 'DIGI'
#define PET_VERSION 2             // 存档结构版本
#include <Arduino.h>

// ---------------------------------------------------------------- 阶段/种族 ----
enum PetStage : uint8_t {
    ST_EGG = 0, ST_BABY, ST_CHILD, ST_TEEN, ST_ADULT, ST_PERFECT, ST_DEAD
};

enum PetSpecies : uint8_t {
    SP_EGG = 0,
    // 线 0：亚古兽
    SP_KOROMON, SP_AGUMON, SP_GREYMON, SP_METALGREYMON, SP_WARGREYMON,
    // 线 1：V 仔兽
    SP_CHIBIMON, SP_VMON, SP_XVMON, SP_PAILDRAMON, SP_IMPERIALDRAMON,
    // 线 2：大耳兽
    SP_GUMMYMON, SP_TERRIERMON, SP_GALGOMON, SP_RAPIDMON, SP_SAINTGALGOMON,
    // 线 3：巴达兽
    SP_PATAMON, SP_ANGEMON, SP_HOLYANGEMON, SP_SERAPHIMON,
    // 线 4：迪路兽
    SP_TAILMON, SP_NEFERTIMON, SP_SILPHYMON, SP_HOLYDRAMON,
    SP_COUNT
};

#define SP_NONE 0xFF

// 物种表访问（实现在 pet.cpp）
uint8_t pet_species_stage(uint8_t sp);
uint8_t pet_species_next(uint8_t sp);
uint8_t pet_species_wins(uint8_t sp);
uint8_t pet_species_char(uint8_t sp);
uint8_t pet_species_line(uint8_t sp);
uint8_t pet_line_count();
uint8_t pet_line_first(uint8_t line);
uint8_t pet_line_len(uint8_t line);
uint8_t pet_devolve_target(uint8_t sp, uint8_t min_stage);

enum ItemFlags : uint32_t {
    ITEM_CARD_COURAGE = 1u << 0,
    ITEM_STRENGTH     = 1u << 3,
    ITEM_GUARD        = 1u << 4,
    ITEM_REVIVE       = 1u << 5,
    ITEM_BATTLE       = 1u << 6,   // 战斗卡：下一战临时进化
};

// 事件类型：网页 / 飞书按这个编号翻译成中文
enum PetEvent : uint8_t {
    EV_HATCH = 1, EV_EVOLVE, EV_FEED, EV_CLEAN, EV_TRAIN, EV_WIN, EV_LOSE,
    EV_POOP, EV_SICK, EV_FOUND, EV_CALL, EV_SLEEP, EV_WAKE, EV_CARD,
    EV_DEATH, EV_IGNORED, EV_HEAL, EV_PET, EV_WILD, EV_DEVOLVE
};

// 需要玩家响应的挂起事件
enum Pending : uint8_t {
    PEND_NONE = 0, PEND_WILD = 1, PEND_CALL = 2
};

struct PetState {
    uint32_t magic;        // 'DIGI'
    uint16_t version;
    uint8_t  stage;
    uint8_t  species;
    uint8_t  alive;
    uint8_t  sleeping;
    uint16_t hunger;       // 0..4 心
    uint16_t mood;         // 0..100
    uint16_t hp;           // 0..100
    uint16_t atk;
    uint16_t def;
    uint16_t weight;       // 克
    uint16_t wins;
    uint16_t losses;
    uint16_t mistakes;
    uint16_t discipline;
    uint32_t items;
    uint8_t  poops;
    uint8_t  sick;
    uint8_t  pending;      // Pending
    uint8_t  pad;

    // 时间基座（都用 epoch 秒，跨重启连续）
    uint32_t birth_epoch;  // 出生时刻
    uint32_t stage_start;  // 本阶段开始
    uint32_t evolve_at;    // 本阶段的时间闸门
    uint32_t last_settle;  // 上次结算
    uint32_t next_event;   // 下一次随机事件
    uint32_t pending_until;

    uint16_t acc_hunger;
    uint16_t acc_mood;
    uint16_t acc_hp;
    uint8_t  crc;
};

// ---------------------------------------------------------------- 战斗 ----
// 力量比命中 + 伤害交换（DM 系列的 P/(P+E) 模型）
#define BATTLE_HP             24
#define BATTLE_MAX_EXCHANGES  40
#define BATTLE_MAX_EX         80

struct BattleExchange {
    uint8_t by_player;     // 1=我方出手
    uint8_t hit;           // 1=命中
    uint8_t dmg;
};

// 最近一场战斗的结果（战斗演出用）
struct BattleOutcome {
    uint8_t  win;          // 1=赢
    uint8_t  card;         // 1=用了战斗卡（临时进化）
    uint8_t  my_species;   // 参战形态（进化后的）
    uint16_t hp_before;
    uint16_t hp_after;
    uint8_t  enemy_hp;     // 敌方剩余 HP（0..BATTLE_HP）
    uint8_t  exchanges;    // 交换次数
};
extern uint8_t         g_evolve_fx;   // 1=刚进化，UI 播特效后清零

extern BattleOutcome   g_battle_out;                 // 打完由 UI 读取
extern BattleExchange  g_battle_ex[BATTLE_MAX_EX];   // 交换明细（可回放）
extern uint8_t         g_battle_ex_n;

const char* pet_stage_name(uint8_t stage);
const char* pet_stage_name_cn(uint8_t stage);
const char* pet_species_name(uint8_t species);
const char* pet_species_name_cn(uint8_t species);
uint8_t     pet_sprite_id(const PetState& p);

uint32_t pet_age_s(const PetState& p, uint32_t now);
uint32_t pet_evolve_in(const PetState& p, uint32_t now);   // 距离时间闸门还有多久
uint16_t pet_wins_needed(uint8_t stage);

void pet_new(PetState& p, uint32_t now);
void pet_tick(PetState& p, uint32_t now);

// NTP 校准时整体平移时间戳，避免把「时钟跳变」误判成「托管了 50 年」
void pet_rebase(PetState& p, uint32_t delta);

// cmd: feed / clean / train / battle / sleep / wake / pet / evolve
bool pet_action(PetState& p, uint32_t now, const char* cmd, char* msg, size_t msglen);
bool pet_apply_card(PetState& p, uint32_t now, const char* id, char* msg, size_t msglen);

// 事件日志（RAM 环形缓冲，最新在前，最多 16 条）
int      pet_log_count();
uint32_t pet_log_time(int idx);
uint8_t  pet_log_kind(int idx);
