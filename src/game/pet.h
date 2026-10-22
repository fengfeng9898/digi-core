#pragma once
#include <Arduino.h>

// 六级进化链：蛋 → 幼年期 → 成长期 → 成熟期 → 完全体 → 究极体
enum PetStage : uint8_t {
    ST_EGG = 0, ST_BABY, ST_CHILD, ST_TEEN, ST_ADULT, ST_PERFECT, ST_DEAD
};

enum PetSpecies : uint8_t {
    SP_EGG = 0, SP_SPROUT, SP_LEAFLING, SP_BLOOMON, SP_GUARDIAN,
    SP_OMEGA_FIRE, SP_OMEGA_WATER, SP_OMEGA_NATURE
};

// 网页升级卡写入的标记位
enum ItemFlags : uint32_t {
    ITEM_CARD_FIRE   = 1u << 0,
    ITEM_CARD_WATER  = 1u << 1,
    ITEM_CARD_NATURE = 1u << 2,
    ITEM_STRENGTH    = 1u << 3,
    ITEM_GUARD       = 1u << 4,
    ITEM_REVIVE      = 1u << 5,
};

// ------------------------------------------------------------------ 状态 ----
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
    uint32_t age_s;
    uint32_t stage_age_s;
    uint32_t play_s;
    uint16_t wins;
    uint16_t losses;
    uint16_t mistakes;
    uint16_t discipline;
    uint32_t items;
    uint8_t  poops;
    // 内部累加器（也一起存档，避免重启后节奏突变）
    uint16_t acc_hunger;
    uint16_t acc_mood;
    uint16_t acc_poop;
    uint8_t  crc;
};

const char* pet_stage_name(uint8_t stage);      // 设备屏幕用（ASCII）
const char* pet_stage_name_cn(uint8_t stage);   // 网页/飞书用（中文）
const char* pet_species_name(uint8_t species);
const char* pet_species_name_cn(uint8_t species);
uint8_t     pet_sprite_id(const PetState& p);

void pet_new(PetState& p);
void pet_tick(PetState& p, uint32_t dt_s);

// cmd: feed / clean / train / battle / sleep / wake / evolve
// 返回 true 表示状态有变化（需要存档 + 刷新界面）
bool pet_action(PetState& p, const char* cmd, char* msg, size_t msglen);

// 卡片: fire / water / nature / power / guard / heal / revive
bool pet_apply_card(PetState& p, const char* id, char* msg, size_t msglen);
