#include "ui.h"

#include <LittleFS.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "anim.h"
#include "background.h"
#include "buttons.h"
#include "config.h"
#include "gfx.h"
#include "icons.h"
#include "settings.h"
#include "web.h"

namespace {

// ------------------------------------------------------------
//  State
// ------------------------------------------------------------

enum Screen : uint8_t {
    SCREEN_MAIN,
    SCREEN_SETTINGS,
    SCREEN_INFO,
    SCREEN_PLAYING,
};

const int CONTENT_TOP = 20;
const uint8_t SETTINGS_COUNT = 6;

Screen screen = SCREEN_MAIN;
int mainSel = 0;              // 0 — settings, 1 — info, then animations
uint8_t settingsSel = 0;
bool settingsEditing = false;
int infoScroll = 0;
uint32_t lastFpsRepeat = 0;

std::vector<AnimInfo> anims;
AnimPlayer player;
bool paused = false;
bool infoOverlay = false;

Button btnUp;
Button btnDown;
Button btnOk;

uint32_t lastDraw = 0;
uint32_t settingsChangedAt = 0;
bool settingsDirty = false;

// ------------------------------------------------------------
//  Small helpers
// ------------------------------------------------------------

// English is the base language; Russian is enabled in the settings.
bool ru() { return settings::get().lang != 0; }
const char *L(const char *en, const char *ruText) { return ru() ? ruText : en; }

int rowHeight() { return gfx::portrait() ? 18 : 8; }

const char *screenName() {
    switch (screen) {
        case SCREEN_SETTINGS:
            return "settings";
        case SCREEN_INFO:
            return "info";
        case SCREEN_PLAYING:
            return "playing";
        default:
            return "main";
    }
}

void syncStatus() {
    String playing;
    if (screen == SCREEN_PLAYING && player.isOpen()) playing = player.info().name;
    web::setStatus(String(screenName()), playing);
}

void goScreen(Screen s) {
    screen = s;
    syncStatus();
}

void saveSettingsSoon() {
    settingsDirty = true;
    settingsChangedAt = millis();
}

void flushSettings() {
    if (!settingsDirty) return;
    settings::save();
    settingsDirty = false;
}

int mainEntries() { return 2 + static_cast<int>(anims.size()); }

void refreshAnims() {
    anim::scan(anims);
    int entries = mainEntries();
    if (mainSel >= entries) mainSel = entries - 1;
    if (mainSel < 0) mainSel = 0;
}

void fmtDuration(uint32_t ms, char *out, size_t n) {
    if (ms < 1000) {
        snprintf(out, n, "%lu %s", static_cast<unsigned long>(ms), L("MS", "МС"));
    } else {
        snprintf(out, n, "%lu.%02lu %s", static_cast<unsigned long>(ms / 1000),
                 static_cast<unsigned long>((ms % 1000) / 10), L("S", "С"));
    }
}

void centerClipped(int y, const char *s, int maxW) {
    int tw = gfx::textWidth(s);
    if (tw > maxW) tw = maxW;
    int x = (gfx::width() - tw) / 2;
    gfx::textClipped(x, y, maxW, s, 1, true);
}

// ------------------------------------------------------------
//  Header: arrows on the edges, frame with an icon and a title
// ------------------------------------------------------------

void drawHeader(const uint8_t *icon, const char *title) {
    int W = gfx::width();
    int tw = gfx::textWidth(title, 1);
    int maxFrame = W - 14;  // leave room for the arrows
    int fw = tw + 16;
    if (fw > maxFrame) fw = maxFrame;
    int fx = (W - fw) / 2;

    gfx::rect(fx, 1, fw, 16, true);
    gfx::icon8(fx + 3, 5, icon, true);
    gfx::textClipped(fx + 13, 6, fw - 16, title, 1, true);

    gfx::arrowLeft(2, 5, 7);
    gfx::arrowRight(W - 8, 5, 7);
}

void drawScrollbar(int total, int visible, int scroll) {
    if (total <= visible || visible <= 0) return;
    int W = gfx::width();
    int H = gfx::height();
    int area = H - CONTENT_TOP;
    int thumb = area * visible / total;
    if (thumb < 8) thumb = 8;
    int y = CONTENT_TOP;
    if (total > visible) {
        y += (area - thumb) * scroll / (total - visible);
    }
    gfx::vline(W - 1, y, y + thumb - 1, true);
}

// ------------------------------------------------------------
//  Main screen: one item in a frame, up/down switches items
// ------------------------------------------------------------

const uint8_t *mainIcon() {
    if (mainSel == 0) return icons::GEAR;
    if (mainSel == 1) return icons::PENCIL;
    return icons::PICTURE;
}

const char *mainTitle() {
    if (mainSel == 0) return gfx::portrait() ? L("SET", "НАСТР") : L("SETTINGS", "НАСТРОЙКИ");
    if (mainSel == 1) return L("INFO", "ИНФО");
    return gfx::portrait() ? L("ANIM", "АНИМ") : L("ANIMATION", "АНИМАЦИЯ");
}

// Subtitle under the item: for an animation it is the name without extension.
const char *mainSubtitle() {
    if (mainSel < 2) return nullptr;
    int a = mainSel - 2;
    if (a >= 0 && a < static_cast<int>(anims.size())) return anims[a].name.c_str();
    return nullptr;
}

void drawMain(uint32_t now) {
    drawHeader(mainIcon(), mainTitle());
    background::draw(CONTENT_TOP, gfx::height(), now);

    const char *sub = mainSubtitle();
    if (sub && sub[0]) {
        int W = gfx::width();
        int areaH = gfx::height() - CONTENT_TOP;
        int maxBox = W - 4;

        int scale = 2;
        int tw = gfx::textWidth(sub, scale);
        int pad = 14;
        if (tw + pad * 2 > maxBox) {
            scale = 1;
            tw = gfx::textWidth(sub, scale);
            pad = 12;
        }
        if (tw + pad * 2 > maxBox) {
            pad = (maxBox - tw) / 2;
            if (pad < 2) pad = 2;
        }

        int boxW = tw + pad * 2;
        if (boxW > maxBox) boxW = maxBox;
        int th = 7 * scale;
        int boxH = th + 8;
        int x = (W - boxW) / 2;
        if (x < 1) x = 1;
        int y = CONTENT_TOP + areaH / 2 - boxH / 2;

        gfx::fillRect(x, y, boxW, boxH, false);
        gfx::rect(x, y, boxW, boxH, true);
        gfx::textClipped(x + pad, y + (boxH - th) / 2, boxW - pad * 2, sub, scale, true);
    }
}

// ------------------------------------------------------------
//  Settings
// ------------------------------------------------------------

void drawSettingsRow(int idx, int y, int rh) {
    const settings::Data &c = settings::get();
    const char *label = "";
    char value[20] = {0};

    switch (idx) {
        case 0:
            label = "FPS";
            snprintf(value, sizeof(value), "%u", c.fps);
            break;
        case 1:
            label = L("ROTATION", "ПОВОРОТ");
            snprintf(value, sizeof(value), "%u°", static_cast<unsigned>(c.rotation) * 90);
            break;
        case 2:
            label = L("BACKGROUND", "ФОН");
            snprintf(value, sizeof(value), "%s", c.background ? L("STARS", "ЗВЁЗДЫ")
                                                            : L("SNOW", "СНЕГ"));
            break;
        case 3:
            label = L("GRID", "СЕТКА");
            snprintf(value, sizeof(value), "%u", c.grid);
            break;
        case 4:
            label = L("LANGUAGE", "ЯЗЫК");
            snprintf(value, sizeof(value), "%s", c.lang ? "RU" : "EN");
            break;
        default:
            label = L("BACK", "НАЗАД");
            break;
    }

    bool selected = (idx == settingsSel);
    bool editing = selected && settingsEditing;
    if (selected) gfx::fillRect(0, y, gfx::width(), rh, true);
    bool ink = !selected;

    // While editing, the value is shown in angle brackets.
    char marked[24] = {0};
    const char *shown = value;
    if (editing && value[0]) {
        snprintf(marked, sizeof(marked), "<%s>", value);
        shown = marked;
    }

    if (gfx::portrait()) {
        gfx::text(4, y + 2, label, 1, ink);
        if (shown[0]) gfx::text(8, y + 9, shown, 1, ink);
    } else {
        gfx::text(3, y, label, 1, ink);
        if (shown[0]) gfx::textRight(gfx::width() - 3, y, shown, 1, ink);
    }
}

void drawSettings() {
    int rh = rowHeight();
    int total = SETTINGS_COUNT * rh;
    int freeSpace = gfx::height() - CONTENT_TOP - total;
    int top = CONTENT_TOP + (freeSpace > 0 ? freeSpace / 2 : 0);

    for (int i = 0; i < SETTINGS_COUNT; i++) {
        drawSettingsRow(i, top + i * rh, rh);
    }
}

// ------------------------------------------------------------
//  Information
// ------------------------------------------------------------

const int INFO_COUNT = 12;

void infoRow(int idx, const char *&label, char *value, size_t n) {
    const settings::Data &c = settings::get();
    switch (idx) {
        case 0:
            label = L("PROJECT", "ПРОЕКТ");
            snprintf(value, n, "%s", PROJECT_NAME);
            break;
        case 1:
            label = L("BOARD", "ПЛАТА");
            snprintf(value, n, "%s", BOARD_SHORT);
            break;
        case 2:
            label = L("VERSION", "ВЕРСИЯ");
            snprintf(value, n, "%s", PROJECT_VERSION);
            break;
        case 3:
            label = L("GRID", "СЕТКА");
            snprintf(value, n, "%ux%u", c.grid, c.grid);
            break;
        case 4:
            label = "FPS";
            snprintf(value, n, "%u", c.fps);
            break;
        case 5:
            label = L("BACKGROUND", "ФОН");
            snprintf(value, n, "%s", c.background ? L("STARS", "ЗВЁЗДЫ")
                                                 : L("SNOW", "СНЕГ"));
            break;
        case 6:
            label = "WI-FI";
            snprintf(value, n, "%s", AP_SSID);
            break;
        case 7:
            label = L("PASSWORD", "ПАРОЛЬ");
            snprintf(value, n, "%s", AP_PASS);
            break;
        case 8: {
            label = L("IP", "АДРЕС");
            String ip = web::ip();
            snprintf(value, n, "%s", ip.c_str());
            break;
        }
        case 9:
            label = L("SITE", "САЙТ");
            snprintf(value, n, "%s.local", MDNS_HOST);
            break;
        case 10:
            label = L("MEMORY", "ПАМЯТЬ");
            snprintf(value, n, "%lu/%lu%s",
                     static_cast<unsigned long>(LittleFS.usedBytes() / 1024),
                     static_cast<unsigned long>(LittleFS.totalBytes() / 1024),
                     L("KB", "КБ"));
            break;
        default:
            label = L("ANIMS", "АНИМ");
            snprintf(value, n, "%d", static_cast<int>(anims.size()));
            break;
    }
}

int infoVisibleRows() {
    int v = (gfx::height() - CONTENT_TOP) / rowHeight();
    return v > 0 ? v : 1;
}

void drawInfo() {
    int rh = rowHeight();
    int visible = infoVisibleRows();
    int maxScroll = INFO_COUNT - visible;
    if (maxScroll < 0) maxScroll = 0;
    if (infoScroll > maxScroll) infoScroll = maxScroll;
    if (infoScroll < 0) infoScroll = 0;

    for (int i = 0; i < visible; i++) {
        int idx = infoScroll + i;
        if (idx >= INFO_COUNT) break;
        const char *label = "";
        char value[32] = {0};
        infoRow(idx, label, value, sizeof(value));
        int y = CONTENT_TOP + i * rh;

        if (gfx::portrait()) {
            gfx::text(5, y + 2, label, 1, true);
            gfx::textClipped(5, y + 9, gfx::width() - 10, value, 1, true);
        } else {
            gfx::text(3, y, label, 1, true);
            gfx::textRight(gfx::width() - 4, y, value, 1, true);
        }
    }

    drawScrollbar(INFO_COUNT, visible, infoScroll);
}

// ------------------------------------------------------------
//  Playback
// ------------------------------------------------------------

void drawPauseBadge() {
    int W = gfx::width();
    int x = W - 16;
    int y = 2;
    gfx::fillRect(x, y, 14, 12, false);
    gfx::rect(x, y, 14, 12, true);
    gfx::fillRect(x + 3, y + 3, 3, 6, true);
    gfx::fillRect(x + 8, y + 3, 3, 6, true);
}

void drawPlayingInfo() {
    if (!player.isOpen()) return;
    const AnimInfo &a = player.info();
    const settings::Data &c = settings::get();
    int W = gfx::width();

    // Animation on top in portrait and on the left in landscape, info beside it.
    player.drawRect(0, 0, 64, 64);

    bool portrait = gfx::portrait();
    char buf[32];
    char dur[24];
    fmtDuration(a.durationMs(c.fps), dur, sizeof(dur));

    if (portrait) {
        centerClipped(68, a.name.c_str(), W);
        snprintf(buf, sizeof(buf), "FPS %u", c.fps);
        centerClipped(78, buf, W);
        snprintf(buf, sizeof(buf), L("FRAMES %u", "КАДРОВ %u"), a.frames);
        centerClipped(88, buf, W);
        centerClipped(98, L("DURATION", "ДЛИТ."), W);
        centerClipped(108, dur, W);
        centerClipped(119, L("OK=STOP", "OK=СТОП"), W);
    } else {
        int tx = 66;
        gfx::textClipped(tx, 3, W - tx - 2, a.name.c_str(), 1, true);
        snprintf(buf, sizeof(buf), "FPS %u", c.fps);
        gfx::textClipped(tx, 13, W - tx - 2, buf, 1, true);
        snprintf(buf, sizeof(buf), L("FRAMES %u", "КАДРОВ %u"), a.frames);
        gfx::textClipped(tx, 23, W - tx - 2, buf, 1, true);
        gfx::textClipped(tx, 33, W - tx - 2, L("DURATION", "ДЛИТ."), 1, true);
        gfx::textClipped(tx, 43, W - tx - 2, dur, 1, true);
        gfx::textClipped(tx, 54, W - tx - 2, L("OK=STOP", "OK=СТОП"), 1, true);
    }
}

void drawPlaying() {
    if (!player.isOpen()) {
        gfx::textCenter(gfx::width() / 2, gfx::height() / 2 - 3, L("NO FILE", "НЕТ ФАЙЛА"), 1, true);
        return;
    }
    if (infoOverlay) {
        drawPlayingInfo();
    } else {
        player.draw();
        if (paused) drawPauseBadge();
    }
}

// ------------------------------------------------------------
//  Transitions
// ------------------------------------------------------------

void leaveToMain() {
    settingsEditing = false;
    flushSettings();
    goScreen(SCREEN_MAIN);
}

void playAnim(int index) {
    if (index < 0 || index >= static_cast<int>(anims.size())) return;
    if (!player.open(anims[index].name)) {
        refreshAnims();
        return;
    }
    player.resetTiming();
    paused = false;
    infoOverlay = false;
    goScreen(SCREEN_PLAYING);
}

void activateMainItem() {
    if (mainSel == 0) {
        settingsSel = 0;
        settingsEditing = false;
        goScreen(SCREEN_SETTINGS);
    } else if (mainSel == 1) {
        infoScroll = 0;
        goScreen(SCREEN_INFO);
    } else {
        playAnim(mainSel - 2);
    }
}

void stopToMain() {
    paused = false;
    infoOverlay = false;
    player.close();
    refreshAnims();
    goScreen(SCREEN_MAIN);
}

void applyFps(int delta) {
    settings::Data &c = settings::get();
    int v = static_cast<int>(c.fps) + delta;
    if (v < MIN_FPS) v = MAX_FPS;
    if (v > MAX_FPS) v = MIN_FPS;
    c.fps = static_cast<uint8_t>(v);
    saveSettingsSoon();
}

void applyRotation(int delta) {
    settings::Data &c = settings::get();
    c.rotation = static_cast<uint8_t>((c.rotation + (delta > 0 ? 1 : 3)) & 0x03);
    gfx::setRotation(c.rotation);
    background::reset();
    saveSettingsSoon();
}

// Changes the selected setting by one step up (delta > 0) or down.
void changeSetting(int delta) {
    settings::Data &c = settings::get();
    switch (settingsSel) {
        case 0:
            applyFps(delta);
            break;
        case 1:
            applyRotation(delta);
            break;
        case 2:
            c.background = c.background ? 0 : 1;
            background::reset();
            saveSettingsSoon();
            break;
        case 3:
            c.grid = (c.grid == 64) ? 25 : 64;
            saveSettingsSoon();
            break;
        case 4:
            c.lang = c.lang ? 0 : 1;
            saveSettingsSoon();
            break;
        default:
            break;
    }
}

void activateSetting() {
    switch (settingsSel) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            settingsEditing = true;
            break;
        default:
            leaveToMain();
            break;
    }
}

// ------------------------------------------------------------
//  Buttons
// ------------------------------------------------------------

void moveMainSel(int delta) {
    int entries = mainEntries();
    if (entries <= 0) return;
    mainSel = (mainSel + delta + entries) % entries;
}

// In portrait rotations (90/270) the physical "down" button becomes
// the logical "up". While the rotation row is being edited the layout
// stays as at 0° so the buttons do not jump under the finger.
bool buttonsSwapped() {
    if (screen == SCREEN_SETTINGS && settingsEditing && settingsSel == 1) return false;
    uint8_t r = gfx::rotation();
    // 90 and 180 swap the buttons, 270 does not (verified on the device).
    return r == 1 || r == 2;
}

void handleButtons() {
    bool rawUpClick = btnUp.click();
    bool rawUpRepeat = btnUp.repeat();
    bool rawDownClick = btnDown.click();
    bool rawDownRepeat = btnDown.repeat();
    bool okClick = btnOk.click();
    bool okLong = btnOk.longPress();

    bool swap = buttonsSwapped();
    bool upClick = swap ? rawDownClick : rawUpClick;
    bool upRepeat = swap ? rawDownRepeat : rawUpRepeat;
    bool downClick = swap ? rawUpClick : rawDownClick;
    bool downRepeat = swap ? rawUpRepeat : rawDownRepeat;

    switch (screen) {
        case SCREEN_MAIN:
            if (okClick || okLong) {
                activateMainItem();
            } else if (upClick || upRepeat) {
                moveMainSel(-1);
            } else if (downClick || downRepeat) {
                moveMainSel(1);
            }
            break;

        case SCREEN_SETTINGS:
            if (settingsEditing) {
                uint32_t now = millis();
                bool fpsRow = (settingsSel == 0);
                if (okClick || okLong) {
                    settingsEditing = false;
                    flushSettings();
                } else if (upClick) {
                    changeSetting(1);
                    if (fpsRow) lastFpsRepeat = now;
                } else if (upRepeat && fpsRow && now - lastFpsRepeat >= 180) {
                    changeSetting(1);
                    lastFpsRepeat = now;
                } else if (downClick) {
                    changeSetting(-1);
                    if (fpsRow) lastFpsRepeat = now;
                } else if (downRepeat && fpsRow && now - lastFpsRepeat >= 180) {
                    changeSetting(-1);
                    lastFpsRepeat = now;
                }
                break;
            }
            if (okLong) {
                leaveToMain();
            } else if (okClick) {
                activateSetting();
            } else if (upClick || upRepeat) {
                settingsSel = static_cast<uint8_t>((settingsSel + SETTINGS_COUNT - 1) % SETTINGS_COUNT);
            } else if (downClick || downRepeat) {
                settingsSel = static_cast<uint8_t>((settingsSel + 1) % SETTINGS_COUNT);
            }
            break;

        case SCREEN_INFO: {
            int maxScroll = INFO_COUNT - infoVisibleRows();
            if (maxScroll < 0) maxScroll = 0;
            if (okClick || okLong) {
                goScreen(SCREEN_MAIN);
            } else if (upClick || upRepeat) {
                if (infoScroll > 0) infoScroll--;
            } else if (downClick || downRepeat) {
                if (infoScroll < maxScroll) infoScroll++;
            }
            break;
        }

        case SCREEN_PLAYING:
            if (okClick || okLong) {
                infoOverlay = false;
                stopToMain();
            } else if (upClick) {
                infoOverlay = !infoOverlay;
            } else if (downClick) {
                infoOverlay = false;
                paused = !paused;
                if (!paused) player.resetTiming();
            }
            break;
    }
}

// ------------------------------------------------------------
//  Requests from the website
// ------------------------------------------------------------

void handleWeb() {
    String name;
    if (web::takePlayRequest(name)) {
        if (player.open(name)) {
            player.resetTiming();
            paused = false;
            infoOverlay = false;
            goScreen(SCREEN_PLAYING);
        } else {
            refreshAnims();
        }
    }

    if (web::takeStopRequest()) {
        if (screen == SCREEN_PLAYING) stopToMain();
    }

    if (web::takeAnimDirty()) {
        refreshAnims();
    }
}

// The rotation may have changed from the website — apply it to the screen.
void syncRotation() {
    uint8_t want = settings::get().rotation;
    if (want != gfx::rotation()) {
        gfx::setRotation(want);
        background::reset();
    }
}

// ------------------------------------------------------------
//  Frame drawing
// ------------------------------------------------------------

void draw(uint32_t now) {
    gfx::clear();

    switch (screen) {
        case SCREEN_MAIN:
            drawMain(now);
            break;
        case SCREEN_SETTINGS:
            drawHeader(icons::GEAR,
                       gfx::portrait() ? L("SET", "НАСТР") : L("SETTINGS", "НАСТРОЙКИ"));
            drawSettings();
            break;
        case SCREEN_INFO:
            drawHeader(icons::PENCIL, L("INFO", "ИНФО"));
            drawInfo();
            break;
        case SCREEN_PLAYING:
            if (!paused) player.advance(now, settings::get().fps);
            drawPlaying();
            break;
    }

    gfx::present();
}

}  // namespace

namespace ui {

void begin() {
    gfx::setRotation(settings::get().rotation);
    background::begin();
    btnUp.begin(BTN_UP_PIN);
    btnDown.begin(BTN_DOWN_PIN);
    btnOk.begin(BTN_OK_PIN);
    refreshAnims();
    screen = SCREEN_MAIN;
    mainSel = 0;
    paused = false;
    infoOverlay = false;
    lastDraw = 0;
    syncStatus();
}

void loop() {
    uint32_t now = millis();

    web::loop();

    btnUp.update(now);
    btnDown.update(now);
    btnOk.update(now);

    handleButtons();
    handleWeb();
    syncRotation();
    gfx::tick();

    if (settingsDirty && now - settingsChangedAt >= 500) {
        flushSettings();
    }

    if (now - lastDraw >= 1000UL / UI_FPS) {
        lastDraw = now;
        draw(now);
    }
}

bool dismissBootInfo() {
    uint32_t now = millis();
    btnOk.update(now);
    return btnOk.click() || btnOk.longPress();
}

bool playing() { return screen == SCREEN_PLAYING && player.isOpen(); }

String playingName() {
    if (!playing()) return String();
    return player.info().name;
}

}  // namespace ui
