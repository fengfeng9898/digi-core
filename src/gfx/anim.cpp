#include "anim.h"
#include "../assets/anim_data.h"
#include "../assets/joker_data.h"   // 敌方角色：小丑王
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
    for (size_t i = 0; i < sizeof(CHAR_ANIMS) / sizeof(CHAR_ANIMS[0]); i++) {
        if (CHAR_ANIMS[i].id == c) return &CHAR_ANIMS[i].set;
    }
    if (c == CH_JOKER) return &JOKER_SET;
    return nullptr;
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
        case SP_KOROMON:       return CH_KOROMON;
        case SP_AGUMON:        return CH_AGUMON;
        case SP_GREYMON:       return CH_GREYMON;
        case SP_METALGREYMON:  return CH_METALGREYMON;   // 暂无素材 -> 静态兜底
        case SP_WARGREYMON:    return CH_NONE;
        default:               return CH_NONE;
    }
}

const AnimClip* anim_get(CharId c, AnimId a) {
    return clip_for(set_for(c), a);
}
