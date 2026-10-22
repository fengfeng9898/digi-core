#pragma once
#include <stdint.h>
#include "../assets/sprites.h"

// 动画类型
enum AnimId : uint8_t {
    AN_IDLE = 0, AN_ATTACK, AN_HURT, AN_WIN, AN_COUNT
};

// 有 GIF 动画的角色（CH_NONE 表示只能用静态精灵）
enum CharId : uint8_t {
    CH_NONE,
    CH_VB_BOTA,
    CH_VB_KORO,
    CH_VB_AGU,
    CH_VB_GREY,
    CH_VB_METALGREY,
    CH_VB_WARGREY,
    CH_VB_CHIBI,
    CH_VB_V,
    CH_VB_XV,
    CH_VB_PAILDRA,
    CH_VB_IMPERIAL,
    CH_VB_PATA,
    CH_VB_ANGE,
    CH_VB_HOLYANGE,
    CH_VB_SERAPHI,
    CH_VB_GUMMY,
    CH_VB_TERRIER,
    CH_VB_GALGO,
    CH_VB_RAPID,
    CH_VB_SAINTGALGO,
    CH_VB_TAILMON,
    CH_VB_NEFERTI,
    CH_VB_SILPHY,
    CH_VB_HOLYDRA,
    CH_VB_SKULLGREY,
    CH_VB_GABU,
    CH_VB_EGG,
    CH_JOKER,
    CH_COUNT
};

struct AnimClip {
    uint8_t frames;
    const SpriteDef* frameDefs;   // 连续数组，第 i 帧 = frameDefs[i]
    const uint16_t* durations;    // 每帧毫秒
};

struct CharAnimSet {
    const AnimClip* idle;
    const AnimClip* attack;
    const AnimClip* hurt;
    const AnimClip* win;
};

struct CharAnimEntry {
    CharId id;
    CharAnimSet set;
};
