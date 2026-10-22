#include "net.h"
#include "../config.h"
#include "../ui/ui.h"
#include "webui.h"
#include "../game/save.h"
#include "../game/clock.h"
#include "../game/pet.h"
#include "../gfx/anim.h"
#include "../ui/battle.h"
#include "../panel.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>

static void play_action_fx(const char* cmd);   // 动作特效映射（定义在后）

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
             "\"mode\":\"%s\",\"ip\":\"%s\",\"synced\":%u,\"warp\":%ld,"
             "\"hour\":%u,\"min\":%u,\"scene\":%u,\"ev\":[",
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
             clock_synced() ? 1 : 0, (long)clock_warp_offset(),
             (unsigned)(((now + 8UL * 3600UL) % 86400UL) / 3600),
             (unsigned)(((now + 8UL * 3600UL) % 3600UL) / 60),
             (unsigned)((((now + 8UL * 3600UL) % 86400UL) / 3600 >= 6 &&
                         ((now + 8UL * 3600UL) % 86400UL) / 3600 < 18) ? 0 : 1));

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

    if (changed) {
        play_action_fx(cmd.c_str());      // 喂食/清洁/训练/安抚/睡觉 -> 面板动作动画
        save_store(*s_pet);
    }
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

// 画面方向：/api/rot?d=cw|ccw
static void handle_rot() {
    String d = s_server.arg("d");
    if (d == "ccw") g_rot_dir = 1;
    else if (d == "cw") g_rot_dir = 0;
    ui_invalidate();
    char body[48];
    snprintf(body, sizeof(body), "{\"ok\":1,\"dir\":%u}", (unsigned)g_rot_dir);
    s_server.send(200, "application/json", body);
}

// 动作特效映射（UI 播 0.7 秒动画）
static void play_action_fx(const char* cmd) {
    if      (!strcmp(cmd, "feed"))  g_action_fx = 1;
    else if (!strcmp(cmd, "clean")) g_action_fx = 2;
    else if (!strcmp(cmd, "train")) g_action_fx = 3;
    else if (!strcmp(cmd, "pet"))   g_action_fx = 4;
    else if (!strcmp(cmd, "sleep")) g_action_fx = 5;
    else if (!strcmp(cmd, "wake"))  g_action_fx = 6;
}

// GET /api/line?n=0..4（255=随机）：选定进化线
static void handle_line() {
    int n = s_server.arg("n").toInt();
    if (n < 0 || n > 4) n = 255;
    s_pet->line_pick = (uint8_t)n;
    save_store(*s_pet);
    ui_invalidate();
    char body[320];
    int len = snprintf(body, sizeof(body), "{\"ok\":1,\"pick\":%d,\"lines\":[", n);
    for (uint8_t l = 0; l < pet_line_count() && len < (int)sizeof(body) - 40; l++) {
        uint8_t sp = pet_line_first(l);
        len += snprintf(body + len, sizeof(body) - len, "%s\"%s\"", l ? "," : "", pet_species_name_cn(sp));
    }
    snprintf(body + len, sizeof(body) - len, "]}");
    s_server.send(200, "application/json", body);
}
static void handle_bright() {
    int v = s_server.arg("v").toInt();
    if (v < 5) v = 5;
    if (v > 200) v = 200;
    ui_set_brightness((uint8_t)v);
    s_server.send(200, "application/json", "{\"ok\":1}");
}

// ---------------------------------------------------------------- OTA ----
// POST /api/ota?k=<OTA_KEY>   multipart 上传 firmware.bin
static bool     s_ota_denied = false;
static bool     s_ota_armed = false;
static uint32_t s_ota_arm_until = 0;

// 第一步：GET /api/ota/arm?k=1234 授权 60 秒
// （不能在 multipart 上传回调里调 server.arg()，会 abort）
// 调试：GET /api/reset?k=1234 重置成一颗新蛋（清掉测试脏数据）
static void handle_reset() {
    if (s_server.arg("k") != OTA_KEY) {
        s_server.send(403, "application/json", "{\"ok\":0,\"err\":\"key\"}");
        return;
    }
    pet_new(*s_pet, clock_now());
    save_store(*s_pet);
    ui_invalidate();
    char body[96];
    snprintf(body, sizeof(body), "{\"ok\":1,\"stage\":%u,\"species\":\"%s\"}",
             (unsigned)s_pet->stage, pet_species_name(s_pet->species));
    Serial.printf("[reset] new pet\n");
    s_server.send(200, "application/json", body);
}
static void handle_ota_arm() {
    if (s_server.arg("k") != OTA_KEY) {
        s_server.send(403, "application/json", "{\"ok\":0,\"err\":\"key\"}");
        return;
    }
    s_ota_armed = true;
    s_ota_arm_until = millis() + 60000u;
    Serial.println("[ota] armed");
    s_server.send(200, "application/json", "{\"ok\":1,\"armed\":60000}");
}

static void handle_ota_done() {
    const bool ok = !s_ota_denied && !Update.hasError();
    char body[72];
    snprintf(body, sizeof(body), "{\"ok\":%d,\"err\":%u,\"denied\":%d}",
             ok ? 1 : 0, (unsigned)Update.getError(), s_ota_denied ? 1 : 0);
    s_server.send(200, "application/json", body);
    if (ok) {
        Serial.println("[ota] success, rebooting");
        delay(400);
        ESP.restart();
    }
    s_ota_denied = false;
    s_ota_armed = false;
}

static void handle_ota_upload() {
    HTTPUpload& up = s_server.upload();

    if (up.status == UPLOAD_FILE_START) {
        s_ota_denied = !s_ota_armed || millis() > s_ota_arm_until;
        if (s_ota_denied) {
            Serial.println("[ota] denied: bad key");
            return;
        }
        Serial.printf("[ota] start %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.printf("[ota] begin failed %u\n", (unsigned)Update.getError());
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (s_ota_denied) return;
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            Serial.printf("[ota] write failed %u\n", (unsigned)Update.getError());
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (s_ota_denied) return;
        if (Update.end(true)) {
            Serial.printf("[ota] done %u bytes\n", (unsigned)up.totalSize);
        } else {
            Serial.printf("[ota] end failed %u\n", (unsigned)Update.getError());
        }
    }
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
    s_server.on("/api/rot", handle_rot);
    s_server.on("/api/reset", HTTP_GET, handle_reset);
    s_server.on("/api/line", HTTP_GET, handle_line);
    s_server.on("/api/ota/arm", HTTP_GET, handle_ota_arm);
    s_server.on("/api/ota", HTTP_POST, handle_ota_done, handle_ota_upload);
    s_server.on("/api/wifi", HTTP_POST, handle_wifi_save);
    s_server.on("/api/wifi/clear", HTTP_POST, handle_wifi_clear);
    s_server.onNotFound(handle_notfound);
    s_server.begin();
    s_server.enableCORS(true);   // 让 APK 内的本地页面(file://)能调 /api/*

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
