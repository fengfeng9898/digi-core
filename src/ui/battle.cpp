// 回合制战斗演出：敌方登场 -> 我方攻击 -> 敌方受伤 -> (胜利/反击) -> 结束
// 单缓冲 + 脏矩形：背景只铺一次，角色只恢复自己上一帧画过的像素。
#include "battle.h"
#include "../panel.h"
#include "../gfx/anim.h"
#include "../gfx/sprite.h"
#include "../game/clock.h"
#include "screen.h"
#include <stdio.h>
#include <string.h>

enum : uint8_t { SH_ME = 0, SH_ENEMY = 1 };
enum : uint8_t { ACT_IDLE = 0, ACT_ATTACK, ACT_HURT, ACT_WIN, ACT_HOLD };

struct Phase {
    const char* name;
    uint16_t dur;
    uint8_t shot;
    uint8_t act;
};

static bool     s_active = false;
static uint32_t s_phase_start = 0;
static uint8_t  s_pi = 0, s_n = 0;
static uint8_t  s_drawn_pi = 0xFF;
static Phase    s_ph[8];

static CharId   s_me = CH_AGUMON, s_en = CH_GREYMON;
static const AnimClip *s_me_idle, *s_me_atk, *s_me_hurt, *s_me_win;
static const AnimClip *s_en_idle, *s_en_atk, *s_en_hurt, *s_en_win;
static uint8_t  s_win = 0, s_card = 0;
static uint16_t s_hp_before = 100, s_hp_after = 100;

static uint32_t clip_total(const AnimClip* c) {
    if (!c) return 0;
    uint32_t t = 0;
    for (uint8_t i = 0; i < c->frames; i++) t += c->durations[i] ? c->durations[i] : 100;
    return t;
}

static uint16_t cap_ms(const AnimClip* c, uint16_t cap) {
    uint32_t t = clip_total(c);
    if (!t) return 300;
    return (uint16_t)(t < cap ? t : cap);
}

static const SpriteDef* frame_at(const AnimClip* c, uint32_t t, uint32_t cap) {
    if (!c || !c->frames) return nullptr;
    uint32_t total = clip_total(c);
    uint32_t tt = (cap > 0) ? (uint32_t)((uint64_t)t * total / cap) : 0;
    if (tt >= total) tt = total ? total - 1 : 0;
    uint32_t acc = 0;
    for (uint8_t i = 0; i < c->frames; i++) {
        acc += c->durations[i] ? c->durations[i] : 100;
        if (tt < acc) return &c->frameDefs[i];
    }
    return &c->frameDefs[c->frames - 1];
}

static const SpriteDef* collapse_frame(const AnimClip* c) {
    if (!c || !c->frames) return nullptr;
    return frame_at(c, clip_total(c) * 55 / 100, clip_total(c));
}

static CharId pick_enemy() {
    int r = (int)random(0, 100);
    if (r < 15) return CH_JOKER;
    switch ((int)random(0, 3)) {
        case 0:  return CH_KOROMON;
        case 1:  return CH_AGUMON;
        default: return CH_GREYMON;
    }
}

static void add_phase(const char* n, uint16_t d, uint8_t shot, uint8_t act) {
    if (s_n >= 8) return;
    s_ph[s_n].name = n;
    s_ph[s_n].dur = d ? d : 200;
    s_ph[s_n].shot = shot;
    s_ph[s_n].act = act;
    s_n++;
}

static uint8_t scene_now() {
    uint32_t bj = (clock_now() + 8UL * 3600UL) % 86400UL;
    int hour = (int)(bj / 3600);
    return (hour >= 6 && hour < 18) ? 0 : 1;
}

void battle_start(const BattleOutcome& out, uint32_t now_ms) {
    s_me = anim_char_for_species(out.my_species);
    if (s_me == CH_NONE) s_me = CH_AGUMON;
    s_en = pick_enemy();
    if (s_en == s_me) s_en = CH_JOKER;

    s_me_idle = anim_get(s_me, AN_IDLE);
    s_me_atk  = anim_get(s_me, AN_ATTACK);
    s_me_hurt = anim_get(s_me, AN_HURT);
    s_me_win  = anim_get(s_me, AN_WIN);
    s_en_idle = anim_get(s_en, AN_IDLE);
    s_en_atk  = anim_get(s_en, AN_ATTACK);
    s_en_hurt = anim_get(s_en, AN_HURT);
    s_en_win  = anim_get(s_en, AN_WIN);

    s_win = out.win;
    s_card = out.card;
    s_hp_before = out.hp_before;
    s_hp_after = out.hp_after;

    s_n = 0;
    s_pi = 0;
    s_drawn_pi = 0xFF;
    s_phase_start = now_ms;

    if (s_card) add_phase("CARD!", 520, SH_ME, ACT_IDLE);
    add_phase("", 450, SH_ENEMY, ACT_IDLE);                       // 敌方登场
    add_phase("GO!", cap_ms(s_me_atk, 1400), SH_ME, ACT_ATTACK);
    add_phase("HIT!", cap_ms(s_en_hurt, 1000), SH_ENEMY, ACT_HURT);
    if (s_win) {
        add_phase("KO!", 420, SH_ENEMY, ACT_HOLD);
        add_phase("YOU WIN!", cap_ms(s_me_win, 1500), SH_ME, ACT_WIN);
        add_phase("WINNER!", 500, SH_ME, ACT_WIN);
    } else {
        add_phase("ENEMY!", cap_ms(s_en_atk, 1300), SH_ENEMY, ACT_ATTACK);
        add_phase("OUCH!", cap_ms(s_me_hurt, 1200), SH_ME, ACT_HURT);
        add_phase("DEFEAT..", 550, SH_ME, ACT_HURT);
    }

    // 铺战斗背景（只铺一次）
    screen_scene(scene_now());
    screen_forget_sprite();
    s_active = true;
}

bool battle_active() { return s_active; }

static const char* enemy_name(CharId c) {
    switch (c) {
        case CH_KOROMON: return "KOROMON!";
        case CH_AGUMON:  return "AGUMON!";
        case CH_GREYMON: return "GREYMON!";
        case CH_JOKER:   return "JOKER!";
        default:         return "WILD!";
    }
}

static void hp_bar(int16_t x, int16_t y, int16_t w, uint16_t pct, uint16_t color) {
    g_matrix->fillRect(x, y, w, 5, 0x2104);
    g_matrix->fillRect(x + 1, y + 1, w - 2, 3, 0x0000);
    int fw = (w - 2) * (int)(pct > 100 ? 100 : pct) / 100;
    if (fw > 0) g_matrix->fillRect(x + 1, y + 1, fw, 3, color);
}

void battle_render(uint32_t ms) {
    if (!s_active) return;

    while (s_pi < s_n) {
        if (ms - s_phase_start < s_ph[s_pi].dur) break;
        s_phase_start += s_ph[s_pi].dur;
        s_pi++;
    }
    if (s_pi >= s_n) {
        s_active = false;
        return;
    }

    const Phase& ph = s_ph[s_pi];
    uint32_t t = ms - s_phase_start;
    bool mine = (ph.shot == SH_ME);
    bool hurt_late = (!strcmp(ph.name, "OUCH!") || !strcmp(ph.name, "DEFEAT.."));

    // ---- 血条 + 文字：只在镜头切换时重画 ----
    if (s_pi != s_drawn_pi) {
        s_drawn_pi = s_pi;

        uint16_t my_hp = hurt_late ? s_hp_after : s_hp_before;
        uint16_t en_hp = 100;
        if (!strcmp(ph.name, "HIT!")) en_hp = 40;
        else if (!strcmp(ph.name, "KO!") || !strcmp(ph.name, "YOU WIN!")) en_hp = 0;
        else if (s_win && !strcmp(ph.name, "WINNER!")) en_hp = 0;
        else if (!s_win && (!strcmp(ph.name, "ENEMY!") || hurt_late)) en_hp = 40;

        screen_clear_band(0, 7);
        hp_bar(1, 1, 30, my_hp, 0x07E0);
        hp_bar(33, 1, 30, en_hp, 0xF800);

        screen_clear_band(56, 8);
        const char* msg = ph.name[0] ? ph.name : enemy_name(s_en);
        screen_text_center(msg, 56, 0xFFFF);
    }

    // ---- 角色 ----
    const AnimClip* clip = nullptr;
    switch (ph.act) {
        case ACT_ATTACK: clip = mine ? s_me_atk : s_en_atk; break;
        case ACT_HURT:   clip = mine ? s_me_hurt : s_en_hurt; break;
        case ACT_WIN:    clip = mine ? s_me_win : s_en_win; break;
        default:         clip = mine ? s_me_idle : s_en_idle; break;
    }

    const SpriteDef* f = (ph.act == ACT_HOLD)
        ? collapse_frame(mine ? s_me_hurt : s_en_hurt)
        : frame_at(clip, t, ph.dur);

    if (f) {
        if (ph.act == ACT_HURT && t < 140) {
            screen_sprite_flash(f, 32, 29, 200);
        } else {
            uint8_t dim = (!mine && s_en != CH_JOKER) ? 215 : 255;
            screen_sprite(f, 32, 29, dim);
        }
    }
}
