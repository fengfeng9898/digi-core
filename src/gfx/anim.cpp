#include "anim.h"

#include "../assets/joker_data.h"   // 敌方角色：小丑王
#include "../assets/vb_data.h"     // Vital Bracelet 精灵（机械暴龙兽/战斗暴龙兽）
#include "../game/pet.h"

static CharId          s_char = CH_NONE;
static const AnimClip* s_clip = nullptr;
static uint8_t         s_frame = 0;
static uint32_t        s_frame_start = 0;
static bool            s_loop = true;
static bool            s_active = false;

static bool            s_battle = false;      // 攻击动画播放中，等结果
static bool            s_has_result = false;
static AnimId          s_result = AN_IDLE;

static const CharAnimSet* set_for(CharId c) {
    switch (c) {
        case CH_VB_BOTA: return &VB_BOTA_SET;
        case CH_VB_KORO: return &VB_KORO_SET;
        case CH_VB_AGU: return &VB_AGU_SET;
        case CH_VB_GREY: return &VB_GREY_SET;
        case CH_VB_METALGREY: return &VB_METALGREY_SET;
        case CH_VB_WARGREY: return &VB_WARGREY_SET;
        case CH_VB_CHIBI: return &VB_CHIBI_SET;
        case CH_VB_V: return &VB_V_SET;
        case CH_VB_XV: return &VB_XV_SET;
        case CH_VB_PAILDRA: return &VB_PAILDRA_SET;
        case CH_VB_IMPERIAL: return &VB_IMPERIAL_SET;
        case CH_VB_PATA: return &VB_PATA_SET;
        case CH_VB_ANGE: return &VB_ANGE_SET;
        case CH_VB_HOLYANGE: return &VB_HOLYANGE_SET;
        case CH_VB_SERAPHI: return &VB_SERAPHI_SET;
        case CH_VB_GUMMY: return &VB_GUMMY_SET;
        case CH_VB_TERRIER: return &VB_TERRIER_SET;
        case CH_VB_GALGO: return &VB_GALGO_SET;
        case CH_VB_RAPID: return &VB_RAPID_SET;
        case CH_VB_SAINTGALGO: return &VB_SAINTGALGO_SET;
        case CH_VB_TAILMON: return &VB_TAILMON_SET;
        case CH_VB_NEFERTI: return &VB_NEFERTI_SET;
        case CH_VB_SILPHY: return &VB_SILPHY_SET;
        case CH_VB_HOLYDRA: return &VB_HOLYDRA_SET;
        case CH_VB_DEVI: return &VB_DEVI_SET;
        case CH_VB_ETE: return &VB_ETE_SET;
        case CH_VB_VAMDE: return &VB_VAMDE_SET;
        case CH_VB_BELPHE_R: return &VB_BELPHE_R_SET;
        case CH_VB_EGG: return &VB_EGG_SET;
        case CH_JOKER: return &JOKER_SET;
        default: return nullptr;
    }
}

static const AnimClip* clip_for(const CharAnimSet* s, AnimId a) {
    if (!s) return nullptr;
    switch (a) {
        case AN_IDLE:   return s->idle;
        case AN_ATTACK: return s->attack;
        case AN_HURT:   return s->hurt;
        case AN_WIN:    return s->win;
        default:        return nullptr;
    }
}

CharId anim_char() { return s_char; }

const SpriteDef* anim_frame() {
    if (!s_active || !s_clip || s_clip->frames == 0) return nullptr;
    return &s_clip->frameDefs[s_frame];
}

void anim_play(CharId c, AnimId a, bool loop) {
    const CharAnimSet* set = set_for(c);
    const AnimClip* clip = clip_for(set, a);
    s_battle = false;
    s_has_result = false;
    if (!set || !clip || clip->frames == 0) {
        s_char = CH_NONE;
        s_clip = nullptr;
        s_active = false;
        return;
    }
    s_char = c;
    s_clip = clip;
    s_frame = 0;
    s_frame_start = millis();
    s_loop = loop;
    s_active = true;
}

void anim_play_battle(CharId c, bool win) {
    if (!set_for(c)) return;
    anim_play(c, AN_ATTACK, false);
    if (!s_active) return;          // 没有攻击动画就不进入序列
    s_battle = true;
    s_result = win ? AN_WIN : AN_HURT;
    s_has_result = true;
}

void anim_update(uint32_t ms) {
    if (!s_active || !s_clip) return;

    const uint16_t dur = s_clip->durations[s_frame];
    if (ms - s_frame_start < dur) return;

    s_frame_start = ms;
    s_frame++;

    if (s_frame < s_clip->frames) return;

    // 本轮播完
    if (s_loop) {
        s_frame = 0;
        return;
    }

    if (s_battle) {
        if (s_has_result) {
            s_battle = false;
            s_has_result = false;
            anim_play(s_char, s_result, false);
            return;
        }
        s_frame = (uint8_t)(s_clip->frames - 1);   // 等结果时定格最后一帧
        return;
    }

    // 非循环动画结束 -> 回待机
    anim_play(s_char, AN_IDLE, true);
}

CharId anim_char_for_species(uint8_t species) {
    switch (species) {
        default:               return (CharId)pet_species_char(species);
    }
}

const AnimClip* anim_get(CharId c, AnimId a) {
    return clip_for(set_for(c), a);
}
