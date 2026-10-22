#include "net.h"
#include "../config.h"
#include "../ui/ui.h"
#include "webui.h"
#include "../game/save.h"
#include "../game/clock.h"
#include "../game/pet.h"
#include "../gfx/anim.h"
#include "../ui/battle.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

static PetState*  s_pet     = nullptr;
static WebServer  s_server(80);
static bool       s_ap_mode = false;

// ---------------------------------------------------------------- 工具 ----
static String ip_str() {
    return s_ap_mode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

static void build_state(String& out) {
    const PetState& p = *s_pet;
    const uint32_t now = clock_now();
    char head[640];
    snprintf(head, sizeof(head),
             "{\"stage\":%u,\"stageName\":\"%s\",\"stageCn\":\"%s\","
             "\"species\":\"%s\",\"speciesCn\":\"%s\",\"hunger\":%u,\"mood\":%u,"
             "\"hp\":%u,\"atk\":%u,\"def\":%u,\"weight\":%u,\"age\":%lu,"
             "\"wins\":%u,\"losses\":%u,\"mistakes\":%u,\"poops\":%u,"
             "\"sick\":%u,\"pending\":%u,\"pendingIn\":%lu,\"sleeping\":%u,\"alive\":%u,"
             "\"items\":%lu,\"bright\":%u,\"evolveIn\":%lu,\"winsNeed\":%u,"
             "\"mode\":\"%s\",\"ip\":\"%s\",\"synced\":%u,\"warp\":%ld,\"ev\":[",
             (unsigned)p.stage, pet_stage_name(p.stage), pet_stage_name_cn(p.stage),
             pet_species_name(p.species), pet_species_name_cn(p.species),
             (unsigned)p.hunger, (unsigned)p.mood, (unsigned)p.hp, (unsigned)p.atk,
             (unsigned)p.def, (unsigned)p.weight, (unsigned long)pet_age_s(p, now),
             (unsigned)p.wins, (unsigned)p.losses, (unsigned)p.mistakes, (unsigned)p.poops,
             (unsigned)p.sick, (unsigned)p.pending,
             (unsigned long)(p.pending_until > now ? p.pending_until - now : 0),
             (unsigned)p.sleeping, (unsigned)p.alive, (unsigned long)p.items,
             (unsigned)ui_get_brightness(), (unsigned long)pet_evolve_in(p, now),
             (unsigned)pet_wins_needed(p.stage),
             s_ap_mode ? "AP" : "LAN", ip_str().c_str(),
             clock_synced() ? 1 : 0, (long)clock_warp_offset());

    out = head;
    const int n = pet_log_count();
    char item[40];
    for (int i = 0; i < n; i++) {
        snprintf(item, sizeof(item), "%s[%lu,%u]",
                 i ? "," : "", (unsigned long)pet_log_time(i), (unsigned)pet_log_kind(i));
        out += item;
    }
    out += "]}";
}

static String reply_json(bool ok, const char* msg) {
    String st;
    build_state(st);
    String out = "{\"ok\":";
    out += ok ? '1' : '0';
    out += ",\"msg\":\"";
    out += msg ? msg : "";
    out += "\",\"state\":";
    out += st;
    out += "}";
    return out;
}

// ---------------------------------------------------------------- 路由 ----
static void handle_root() {
    if (s_ap_mode) s_server.send_P(200, "text/html", SETUP_HTML);
    else            s_server.send_P(200, "text/html", INDEX_HTML);
}

static void handle_state() {
    String st;
    build_state(st);
    s_server.send(200, "application/json", st);
}

static void handle_act() {
    String cmd = s_server.arg("c");
    char msg[24] = {0};
    bool changed = pet_action(*s_pet, clock_now(), cmd.c_str(), msg, sizeof(msg));

    // 战斗：交给回合制演出（敌方登场 -> 攻击 -> 受伤 -> 结果）
    if (cmd == "battle" && changed) {
        battle_start(g_battle_out, millis());
    }

    if (changed) save_store(*s_pet);
    if (msg[0]) ui_toast(msg);
    s_server.send(200, "application/json", reply_json(changed, msg));
}

static void handle_card() {
    String id = s_server.arg("id");
    char msg[24] = {0};
    bool changed = pet_apply_card(*s_pet, clock_now(), id.c_str(), msg, sizeof(msg));
    if (changed) save_store(*s_pet);
    if (msg[0]) ui_toast(msg);
    s_server.send(200, "application/json", reply_json(changed, msg));
}

static void handle_bright() {
    int v = s_server.arg("v").toInt();
    if (v < 5) v = 5;
    if (v > 200) v = 200;
    ui_set_brightness((uint8_t)v);
    s_server.send(200, "application/json", "{\"ok\":1}");
}

// 调试：快进时间，验证 15~30 天节奏时不用真等
static void handle_warp() {
    int h = s_server.arg("h").toInt();
    if (h == 0) h = 24;
    if (h < -720 || h > 720) h = 24;
    clock_warp_hours(h);
    // 同步平移结算基准，避免被判定成“托管挨饿”
    s_pet->last_settle = (uint32_t)((int32_t)s_pet->last_settle + h * 3600);
    s_server.send(200, "application/json", reply_json(true, "TIME WARP"));
}

static void handle_wifi_save() {
    String ssid = s_server.arg("ssid");
    String pass = s_server.arg("pass");
    if (ssid.length() == 0) {
        s_server.send(400, "text/plain", "ssid required");
        return;
    }
    save_wifi_store(ssid, pass);
    s_server.send(200, "text/html",
                  "<meta charset=utf-8><body style='background:#0b0f14;color:#e6edf3;"
                  "font-family:sans-serif;padding:30px'><h3>已保存，正在重启…</h3></body>");
    delay(600);
    ESP.restart();
}

static void handle_wifi_clear() {
    save_wifi_wipe();
    s_server.send(200, "text/html",
                  "<meta charset=utf-8><body style='background:#0b0f14;color:#e6edf3;"
                  "font-family:sans-serif;padding:30px'><h3>WiFi 配置已清除，重启后进入配网模式</h3></body>");
    delay(600);
    ESP.restart();
}

static void handle_notfound() {
    if (s_ap_mode) s_server.send_P(200, "text/html", SETUP_HTML);
    else           s_server.send(404, "text/plain", "not found");
}

static void show_ip_banner() {
    String ip = ip_str();
    int dot = ip.lastIndexOf('.');
    if (dot > 0) {
        String a = ip.substring(0, dot);
        String b = ip.substring(dot + 1);
        ui_boot(a.c_str(), ("." + b).c_str());
    } else {
        ui_boot("WIFI OK", ip.c_str());
    }
}

// ---------------------------------------------------------------- 启动 ----
void net_begin(PetState* pet) {
    s_pet = pet;

    String ssid, pass;
    bool have_creds = save_wifi_load(ssid, pass);

    // 没存过 WiFi 时用编译期默认值（config.h 里配的家里 WiFi）
    if (!have_creds && strlen(WIFI_STA_SSID_DEFAULT) > 0) {
        ssid = WIFI_STA_SSID_DEFAULT;
        pass = WIFI_STA_PASS_DEFAULT;
        have_creds = true;
        Serial.printf("[net] using built-in WiFi: %s\n", ssid.c_str());
    }

    if (have_creds) {
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.begin(ssid.c_str(), pass.c_str());

        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_STA_TIMEOUT_MS) {
            delay(150);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        s_ap_mode = false;
        // NTP：15~30 天的周期必须走真实时间，跨重启连续
        configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org", "time.windows.com");
        MDNS.begin("digicore");
        MDNS.addService("http", "tcp", 80);
    } else {
        s_ap_mode = true;
        WiFi.mode(WIFI_AP);
        String mac = WiFi.macAddress();
        String suffix = mac.substring(12);
        suffix.replace(":", "");
        String ap_ssid = String(WIFI_AP_PREFIX) + suffix;
        WiFi.softAP(ap_ssid.c_str(), WIFI_AP_PASS);
    }

    s_server.on("/", handle_root);
    s_server.on("/api/state", handle_state);
    s_server.on("/api/act", handle_act);
    s_server.on("/api/card", handle_card);
    s_server.on("/api/bright", handle_bright);
    s_server.on("/api/warp", handle_warp);
    s_server.on("/api/wifi", HTTP_POST, handle_wifi_save);
    s_server.on("/api/wifi/clear", HTTP_POST, handle_wifi_clear);
    s_server.onNotFound(handle_notfound);
    s_server.begin();

    show_ip_banner();
    Serial.printf("[net] mode=%s ip=%s\n", s_ap_mode ? "AP" : "LAN", ip_str().c_str());
}

void net_loop() {
    s_server.handleClient();

    if (!s_ap_mode && WiFi.status() != WL_CONNECTED) {
        static uint32_t last_try = 0;
        if (millis() - last_try > 15000) {
            last_try = millis();
            WiFi.reconnect();
        }
    }
}

bool net_ap_mode() { return s_ap_mode; }
String net_ip() { return ip_str(); }
