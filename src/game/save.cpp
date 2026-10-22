#include "save.h"
#include "../config.h"
#include <Preferences.h>
#include <stddef.h>
#include <string.h>

// ---------------------------------------------------------------- 校验 ----
static uint8_t crc8(const uint8_t* d, size_t n) {
    uint8_t c = 0x00;
    while (n--) {
        c ^= *d++;
        for (int i = 0; i < 8; i++) {
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
        }
    }
    return c;
}

static void pet_seal(PetState& p) {
    p.crc = crc8(reinterpret_cast<const uint8_t*>(&p), offsetof(PetState, crc));
}

// ------------------------------------------------------- A/B 双槽存档 ----
// 两个槽各存一份「序号 + 宠物」，写入时总覆盖序号较小的那个。
// 掉电最坏只丢一次存档，另一个槽永远是完整的上一份。
#define SLOT_A     "petA"
#define SLOT_B     "petB"
#define REC_MAGIC  0x44475331u      // 'DGS1'

struct SaveRec {
    uint32_t magic;
    uint32_t seq;
    PetState pet;
};

static bool rec_ok(const SaveRec& r) {
    if (r.magic != REC_MAGIC) return false;
    if (r.pet.magic != 0x44494749u || r.pet.version != 1) return false;
    const uint8_t want = crc8(reinterpret_cast<const uint8_t*>(&r.pet), offsetof(PetState, crc));
    return r.pet.crc == want;
}

static size_t read_slot(Preferences& prefs, const char* key, SaveRec& r) {
    memset(&r, 0, sizeof(r));
    return prefs.getBytes(key, &r, sizeof(r));
}

bool save_load(PetState& p, uint32_t now) {
    Preferences prefs;
    if (!prefs.begin(SAVE_NS, true)) {
        pet_new(p, now);
        return false;
    }
    SaveRec a, b;
    size_t na = read_slot(prefs, SLOT_A, a);
    size_t nb = read_slot(prefs, SLOT_B, b);
    prefs.end();

    const bool oka = (na == sizeof(SaveRec)) && rec_ok(a);
    const bool okb = (nb == sizeof(SaveRec)) && rec_ok(b);

    if (oka && (!okb || a.seq >= b.seq)) {
        p = a.pet;
        return true;
    }
    if (okb) {
        p = b.pet;
        return true;
    }
    pet_new(p, now);
    return false;
}

void save_store(PetState& p) {
    pet_seal(p);

    Preferences prefs;
    if (!prefs.begin(SAVE_NS, false)) return;

    SaveRec a, b;
    size_t na = read_slot(prefs, SLOT_A, a);
    size_t nb = read_slot(prefs, SLOT_B, b);
    const bool oka = (na == sizeof(SaveRec)) && rec_ok(a);
    const bool okb = (nb == sizeof(SaveRec)) && rec_ok(b);

    uint32_t next = 1;
    if (oka) next = a.seq + 1;
    if (okb && b.seq + 1 > next) next = b.seq + 1;

    SaveRec rec;
    rec.magic = REC_MAGIC;
    rec.seq = next;
    rec.pet = p;

    // 覆盖更旧的那个槽
    const bool to_a = !oka || (okb && a.seq <= b.seq);
    prefs.putBytes(to_a ? SLOT_A : SLOT_B, &rec, sizeof(rec));
    prefs.end();
}

void save_wipe() {
    Preferences prefs;
    if (prefs.begin(SAVE_NS, false)) {
        prefs.clear();
        prefs.end();
    }
}

// ---------------------------------------------------------------- WiFi ----
bool save_wifi_load(String& ssid, String& pass) {
    Preferences prefs;
    if (!prefs.begin(SAVE_NS, true)) return false;
    ssid = prefs.getString(SAVE_WIFI_SSID, "");
    pass = prefs.getString(SAVE_WIFI_PASS, "");
    prefs.end();
    return ssid.length() > 0;
}

void save_wifi_store(const String& ssid, const String& pass) {
    Preferences prefs;
    if (!prefs.begin(SAVE_NS, false)) return;
    prefs.putString(SAVE_WIFI_SSID, ssid);
    prefs.putString(SAVE_WIFI_PASS, pass);
    prefs.end();
}

void save_wifi_wipe() {
    Preferences prefs;
    if (!prefs.begin(SAVE_NS, false)) return;
    prefs.remove(SAVE_WIFI_SSID);
    prefs.remove(SAVE_WIFI_PASS);
    prefs.end();
}
