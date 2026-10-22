#pragma once
#include <stdint.h>
#include "../assets/sprites.h"

// 动画类型
enum AnimId : uint8_t {
    AN_IDLE = 0, AN_ATTACK, AN_HURT, AN_WIN, AN_COUNT
};

// 有 GIF 动画的角色（CH_NONE 表示只能用静态精灵）
enum CharId : uint8_t {
    CH_NONE = 0, CH_KOROMON, CH_AGUMON, CH_GREYMON, CH_METALGREYMON, CH_COUNT
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
