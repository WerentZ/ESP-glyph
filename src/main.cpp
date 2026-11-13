#include <Arduino.h>

#include "anim.h"
#include "config.h"
#include "gfx.h"
#include "settings.h"
#include "ui.h"
#include "web.h"

namespace {

// English is the base language; Russian is enabled in the settings.
bool ru() { return settings::get().lang != 0; }
const char *L(const char *en, const char *ruText) { return ru() ? ruText : en; }

// ------------------------------------------------------------
//  Splash screen
// ------------------------------------------------------------

void drawSplash(const char *status) {
    gfx::clear();
    int W = gfx::width();
    int H = gfx::height();

    gfx::rect(0, 0, W, H, true);

    uint8_t titleScale = 2;
    if (gfx::textWidth("GLYPH", titleScale) > W - 6) titleScale = 1;
    uint8_t subScale = 2;
    if (gfx::textWidth("MATRIX", subScale) > W - 6) subScale = 1;

    int blockH = 14 + 3 + 14 + 10 + 7;
    int y = (H - blockH) / 2;
    if (y < 4) y = 4;

    gfx::textCenter(W / 2, y, "GLYPH", titleScale, true);
    gfx::textCenter(W / 2, y + 17, "MATRIX", subScale, true);
    gfx::textCenter(W / 2, y + 38, "v" PROJECT_VERSION, 1, true);

    if (status && *status) {
        gfx::textCenter(W / 2, H - 11, status, 1, true);
    }
    gfx::present();
}

// ------------------------------------------------------------
//  Wi-Fi info screen
// ------------------------------------------------------------

void drawBootInfo() {
    gfx::clear();
    int W = gfx::width();
    int H = gfx::height();

    gfx::rect(0, 0, W, 14, true);
    gfx::textCenter(W / 2, 4, L("WI-FI READY", "WI-FI ГОТОВ"), 1, true);

    String ip = web::ip();

    if (gfx::portrait()) {
        struct Pair {
            const char *label;
            const char *value;
        };
        char ipBuf[24];
        snprintf(ipBuf, sizeof(ipBuf), "%s", ip.c_str());
        char hostBuf[24];
        snprintf(hostBuf, sizeof(hostBuf), "%s.local", MDNS_HOST);

        Pair pairs[4] = {
            {L("NETWORK", "СЕТЬ"), AP_SSID},
            {L("PASSWORD", "ПАРОЛЬ"), AP_PASS},
            {L("IP", "АДРЕС"), ipBuf},
            {L("SITE", "САЙТ"), hostBuf},
        };
        for (int i = 0; i < 4; i++) {
            int y = 20 + i * 24;
            gfx::text(5, y, pairs[i].label, 1, true);
            gfx::textClipped(8, y + 9, W - 12, pairs[i].value, 1, true);
        }
        gfx::textCenter(W / 2, H - 13, L("OPEN", "ОТКРОЙТЕ"), 1, true);
    } else {
        char line[40];
        int y = 18;
        snprintf(line, sizeof(line), L("NETWORK: %s", "СЕТЬ: %s"), AP_SSID);
        gfx::text(4, y, line, 1, true);
        snprintf(line, sizeof(line), L("PASSWORD: %s", "ПАРОЛЬ: %s"), AP_PASS);
        gfx::text(4, y + 10, line, 1, true);
        snprintf(line, sizeof(line), L("SITE: %s", "САЙТ: %s"), ip.c_str());
        gfx::text(4, y + 20, line, 1, true);
        snprintf(line, sizeof(line), L("OR: %s.local", "ИЛИ: %s.local"), MDNS_HOST);
        gfx::text(4, y + 30, line, 1, true);
        gfx::textCenter(W / 2, H - 8, L("OPEN BROWSER", "ОТКРОЙТЕ БРАУЗЕР"), 1, true);
    }

    gfx::present();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(150);
    Serial.println();
    Serial.println("=== " PROJECT_NAME " v" PROJECT_VERSION " ===");

    Serial.println("[BOOT] settings");
    settings::begin();
    Serial.printf("[BOOT] settings: fps %u, rotation %u, grid %u\n",
                  settings::get().fps, settings::get().rotation,
                  settings::get().grid);

    Serial.println("[BOOT] display");
    gfx::begin();
    gfx::setRotation(settings::get().rotation);

    Serial.println("[BOOT] splash");
    drawSplash(L("LOADING", "ЗАГРУЗКА"));
    delay(SPLASH_MS);

    Serial.println("[BOOT] filesystem");
    anim::begin();
    anim::ensureDemo();
    Serial.println("[BOOT] filesystem ready");

    Serial.println("[BOOT] wi-fi");
    drawSplash("WI-FI");
    web::begin();

    Serial.println("[BOOT] ui");
    ui::begin();
    drawBootInfo();

    uint32_t started = millis();
    while (millis() - started < BOOT_INFO_MS) {
        web::loop();
        if (ui::dismissBootInfo()) break;
        delay(5);
    }

    gfx::clear();
    gfx::present();

    Serial.printf("[UI] ready, press OK. Site: http://%s/ or http://%s.local/\n",
                  web::ip().c_str(), MDNS_HOST);
}

void loop() { ui::loop(); }
