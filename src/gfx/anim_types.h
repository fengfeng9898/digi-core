#pragma once
#include <stdint.h>
#include "../assets/sprites.h"

// 动画类型
enum AnimId : uint8_t {
    AN_IDLE = 0, AN_ATTACK, AN_HURT, AN_WIN, AN_COUNT
};

// 有 GIF 动画的角色（CH_NONE 表示只能用静态精灵）
enum CharId : uint8_t {
    CH_NONE = 0,
    CH_VB_EGG, CH_VB_BOTA, CH_VB_KORO, CH_VB_AGU, CH_VB_GREY,
    CH_VB_METALGREY, CH_VB_WARGREY, CH_VB_SKULLGREY, CH_VB_GABU,
    CH_JOKER, CH_COUNT
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
