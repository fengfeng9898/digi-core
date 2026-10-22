#include "clock.h"
#include <time.h>

// 时间基座：
//   没联网时 -> 开机秒数（重启归零，但节奏一致）
//   NTP 同步后 -> 真实 epoch
// 关键：宠物用 birth / evolve_at 这类绝对时间戳，所以 NTP 跳变会被识别为
// 「时钟校准」，而不是「玩家托管了 3 天」。
static int32_t  s_warp = 0;
static bool     s_synced = false;
static uint32_t s_last_now = 0;
static uint32_t s_jump = 0;

void clock_begin() {
    s_last_now = (uint32_t)millis() / 1000u;
}

bool clock_synced() {
    time_t t = time(nullptr);
    if (t > 1600000000) s_synced = true;
    return s_synced;
}

uint32_t clock_now() {
    time_t t = time(nullptr);
    uint32_t now;
    if (t > 1600000000) {
        now = (uint32_t)t;
    } else {
        now = (uint32_t)millis() / 1000u;
    }
    now += (uint32_t)s_warp;

    // 只有 NTP 校准才会产生「一年以上」的跳变；快进最大 30 天，不会误判
    if (s_last_now != 0 && now > s_last_now + 365 * 86400u) {
        s_jump += now - s_last_now;
    }
    s_last_now = now;
    return now;
}

uint32_t clock_take_jump() {
    uint32_t j = s_jump;
    s_jump = 0;
    return j;
}

void clock_warp_hours(int hours) {
    s_warp += hours * 3600;
}

int32_t clock_warp_offset() { return s_warp; }
