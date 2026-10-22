#include "save.h"
#include "../config.h"
#include <Preferences.h>
#include <stddef.h>
#include <string.h>

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

bool save_load(PetState& p) {
    Preferences prefs;
    if (!prefs.begin(SAVE_NS, true)) {
        pet_new(p);
        return false;
    }
    size_t got = prefs.getBytes(SAVE_KEY, &p, sizeof(PetState));
    prefs.end();

    if (got != sizeof(PetState)) {
        pet_new(p);
        return false;
    }
    const uint8_t want = crc8(reinterpret_cast<const uint8_t*>(&p), offsetof(PetState, crc));
    if (p.magic != 0x44494749u || p.version != 1 || p.crc != want) {
        pet_new(p);
        return false;
    }
    return true;
}

void save_store(PetState& p) {
    pet_seal(p);
    Preferences prefs;
    if (!prefs.begin(SAVE_NS, false)) return;
    prefs.putBytes(SAVE_KEY, &p, sizeof(PetState));
    prefs.end();
}

void save_wipe() {
    Preferences prefs;
    if (prefs.begin(SAVE_NS, false)) {
        prefs.clear();
        prefs.end();
    }
}

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
