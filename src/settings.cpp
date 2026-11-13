#include "settings.h"

#include <Preferences.h>

namespace {

Preferences prefs;
settings::Data cfg;
bool started = false;

uint8_t clampFps(int v) {
    if (v < MIN_FPS) return MIN_FPS;
    if (v > MAX_FPS) return MAX_FPS;
    return static_cast<uint8_t>(v);
}

}  // namespace

namespace settings {

void begin() {
    prefs.begin("glyph", false);
    cfg.fps = clampFps(prefs.getUChar("fps", DEFAULT_FPS));
    cfg.rotation = prefs.getUChar("rot", 0) & 0x03;
    cfg.background = prefs.getUChar("bg", 0) ? 1 : 0;
    uint8_t grid = prefs.getUChar("grid", DEFAULT_GRID);
    cfg.grid = (grid == 64) ? 64 : 25;
    cfg.lang = prefs.getUChar("lang", 0) ? 1 : 0;
    cfg.stockVersion = prefs.getUChar("stk", 0);
    started = true;
}

Data &get() { return cfg; }

void save() {
    if (!started) return;
    prefs.putUChar("fps", cfg.fps);
    prefs.putUChar("rot", cfg.rotation);
    prefs.putUChar("bg", cfg.background);
    prefs.putUChar("grid", cfg.grid);
    prefs.putUChar("lang", cfg.lang);
    prefs.putUChar("stk", cfg.stockVersion);
}

}  // namespace settings
