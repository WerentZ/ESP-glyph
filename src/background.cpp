#include "background.h"

#include <math.h>

#include "gfx.h"
#include "settings.h"

namespace {

struct Particle {
    float x;
    float y;
    float vy;
    float phase;
    uint8_t size;
};

const uint8_t SNOW_COUNT = 34;
const uint8_t STAR_COUNT = 44;
const uint8_t MAX_COUNT = 44;

Particle parts[MAX_COUNT];
uint8_t mode = 0;
int lastW = -1;
int lastH = -1;
int lastTop = -1;
uint8_t lastMode = 255;
uint32_t lastNow = 0;

int activeCount() { return (settings::get().background == 0) ? SNOW_COUNT : STAR_COUNT; }

void seed(int top, int bottom) {
    int W = gfx::width();
    int H = gfx::height();
    int areaTop = constrain(top, 0, H - 1);
    int areaBottom = constrain(bottom, areaTop + 1, H);
    mode = settings::get().background;

    for (int i = 0; i < MAX_COUNT; i++) {
        parts[i].x = static_cast<float>(random(0, W));
        parts[i].y = static_cast<float>(random(areaTop, areaBottom));
        parts[i].phase = static_cast<float>(random(0, 628)) / 100.0f;
        if (mode == 0) {
            parts[i].vy = static_cast<float>(random(10, 32));  // pixels per second
            parts[i].size = (i % 5 == 0) ? 2 : 1;
        } else {
            parts[i].vy = 0;
            parts[i].size = 1;
        }
    }
    lastW = W;
    lastH = H;
    lastTop = areaTop;
    lastMode = mode;
}

}  // namespace

namespace background {

void begin() {
    lastW = -1;
    lastH = -1;
    lastTop = -1;
    lastMode = 255;
    lastNow = 0;
}

void reset() {
    lastW = -1;
    lastH = -1;
    lastTop = -1;
    lastMode = 255;
    lastNow = 0;
}

void draw(int top, int bottom, uint32_t now) {
    int W = gfx::width();
    int H = gfx::height();
    int areaTop = constrain(top, 0, H - 1);
    int areaBottom = constrain(bottom, areaTop + 1, H);
    uint8_t wantMode = settings::get().background;

    if (W != lastW || H != lastH || wantMode != lastMode ||
        areaTop != lastTop) {
        seed(areaTop, areaBottom);
    }

    if (lastNow == 0) lastNow = now;
    float dt = (now - lastNow) / 1000.0f;
    lastNow = now;
    if (dt < 0) dt = 0;
    if (dt > 0.15f) dt = 0.15f;

    int count = activeCount();
    for (int i = 0; i < count; i++) {
        Particle &p = parts[i];
        if (wantMode == 0) {
            p.y += p.vy * dt;
            p.x += sinf(now * 0.0016f + p.phase) * 9.0f * dt;
            if (p.x < -2) p.x = W + 1;
            if (p.x > W + 2) p.x = -1;
            if (p.y > areaBottom + 2) {
                p.y = areaTop - 2;
                p.x = static_cast<float>(random(0, W));
                p.phase = static_cast<float>(random(0, 628)) / 100.0f;
                p.vy = static_cast<float>(random(10, 32));
            }
            if (p.y >= areaTop) {
                int px = static_cast<int>(p.x);
                int py = static_cast<int>(p.y);
                gfx::fillRect(px, py, p.size, p.size, true);
            }
        } else {
            bool visible = ((now / 420 + i * 3 + static_cast<int>(p.phase * 10)) % 11) != 0;
            if (visible) {
                int px = static_cast<int>(p.x);
                int py = static_cast<int>(p.y);
                if (py >= areaTop) gfx::pixel(px, py, true);
            }
        }
    }
}

}  // namespace background
