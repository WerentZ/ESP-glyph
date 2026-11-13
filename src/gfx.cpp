#include "gfx.h"

#include <GyverOLED.h>
#include <Wire.h>

#include "config.h"
#include "font5x7.h"

namespace {

GyverOLED<SSD1306_128x64, OLED_BUFFER> *oled = nullptr;
uint8_t currentRotation = 0;
bool displayPresent = false;
uint8_t currentAddr = 0;
uint32_t lastProbe = 0;

struct DisplayConfig {
    uint8_t sda;
    uint8_t scl;
    uint8_t addr;
    uint32_t speed;
};

DisplayConfig activeCfg = {OLED_SDA, OLED_SCL, OLED_ADDRESS, OLED_I2C_CLOCK};

bool i2cPresent(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

// Try both SDA/SCL orders, both common addresses and two bus speeds.
bool detectDisplay(DisplayConfig &out) {
    static const uint8_t pinPairs[2][2] = {
        {OLED_SDA, OLED_SCL},
        {OLED_SCL, OLED_SDA},
    };
    static const uint8_t addresses[2] = {
        OLED_ADDRESS,
        static_cast<uint8_t>(OLED_ADDRESS == 0x3C ? 0x3D : 0x3C),
    };
    static const uint32_t speeds[2] = {OLED_I2C_CLOCK, 100000};

    for (uint8_t si = 0; si < 2; si++) {
        for (uint8_t pi = 0; pi < 2; pi++) {
            Wire.begin(pinPairs[pi][0], pinPairs[pi][1]);
            Wire.setTimeOut(20);
            Wire.setClock(speeds[si]);
            delay(2);
            for (uint8_t ai = 0; ai < 2; ai++) {
                if (i2cPresent(addresses[ai])) {
                    out.sda = pinPairs[pi][0];
                    out.scl = pinPairs[pi][1];
                    out.addr = addresses[ai];
                    out.speed = speeds[si];
                    return true;
                }
            }
            Wire.end();
        }
    }
    return false;
}

void initDisplay(const DisplayConfig &cfg) {
    if (!oled || cfg.addr != currentAddr) {
        delete oled;
        oled = new GyverOLED<SSD1306_128x64, OLED_BUFFER>(cfg.addr);
        currentAddr = cfg.addr;
    }
    oled->init(cfg.sda, cfg.scl);
    Wire.setClock(cfg.speed);
    Wire.setTimeOut(20);
    oled->setContrast(GLYPH_CONTRAST);
    activeCfg = cfg;
    displayPresent = true;
}

// ------------------------------------------------------------
//  UTF-8 and font
// ------------------------------------------------------------

const uint8_t *glyphFor(uint32_t cp) {
    if (cp >= 0x20 && cp <= 0x7E) return font5x7::ASCII[cp - 0x20];
    if (cp >= 0x410 && cp <= 0x42F) return font5x7::CYR[cp - 0x410];
    if (cp >= 0x430 && cp <= 0x44F) return font5x7::CYR[cp - 0x430];  // lowercase -> uppercase
    if (cp == 0x401 || cp == 0x451) return font5x7::CYR[32];          // Yo
    if (cp == 0xB0) return font5x7::DEGREE;
    if (cp == 0x2022) return font5x7::BULLET;
    return font5x7::ASCII[0x3F - 0x20];  // '?'
}

uint32_t nextCodepoint(const char *&p) {
    uint8_t c = static_cast<uint8_t>(*p++);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
        if (!*p) return 0xFFFD;
        uint32_t cp = ((c & 0x1F) << 6) | (static_cast<uint8_t>(*p++) & 0x3F);
        return cp;
    }
    if ((c & 0xF0) == 0xE0) {
        if (!p[0] || !p[1]) return 0xFFFD;
        uint32_t cp = ((c & 0x0F) << 12) |
                      ((static_cast<uint8_t>(p[0]) & 0x3F) << 6) |
                      (static_cast<uint8_t>(p[1]) & 0x3F);
        p += 2;
        return cp;
    }
    if ((c & 0xF8) == 0xF0) {
        if (!p[0] || !p[1] || !p[2]) return 0xFFFD;
        uint32_t cp = ((c & 0x07) << 18) |
                      ((static_cast<uint8_t>(p[0]) & 0x3F) << 12) |
                      ((static_cast<uint8_t>(p[1]) & 0x3F) << 6) |
                      (static_cast<uint8_t>(p[2]) & 0x3F);
        p += 3;
        return cp;
    }
    return 0xFFFD;
}

void drawGlyph5x7(int x, int y, const uint8_t *g, uint8_t scale, bool on) {
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (uint8_t row = 0; row < 7; row++) {
            if ((bits >> row) & 1) {
                if (scale == 1) {
                    gfx::pixel(x + col, y + row, on);
                } else {
                    gfx::fillRect(x + col * scale, y + row * scale, scale, scale, on);
                }
            }
        }
    }
}

}  // namespace

namespace gfx {

void begin() {
    DisplayConfig cfg;
    if (detectDisplay(cfg)) {
        Wire.end();
        initDisplay(cfg);
        Serial.printf("[OLED] display at 0x%02X (SDA %d, SCL %d, %lu kHz)\n", cfg.addr,
                      cfg.sda, cfg.scl, static_cast<unsigned long>(cfg.speed / 1000));
        if (cfg.sda != OLED_SDA || cfg.scl != OLED_SCL) {
            Serial.println("[OLED] SDA/SCL swapped — using detected pins");
        }
        oled->clear();
        oled->update();
    } else {
        Serial.printf("[OLED] not found (tried SDA %d/%d, 0x%02X/0x%02X, 400/100 kHz)\n",
                      OLED_SDA, OLED_SCL, OLED_ADDRESS,
                      static_cast<uint8_t>(OLED_ADDRESS == 0x3C ? 0x3D : 0x3C));

        // Diagnostics: pulled-up lines should read as 1.
        pinMode(OLED_SDA, INPUT_PULLUP);
        pinMode(OLED_SCL, INPUT_PULLUP);
        delay(2);
        Serial.printf("[OLED] idle lines: SDA=%d SCL=%d (1 = OK, 0 = short/low)\n",
                      digitalRead(OLED_SDA), digitalRead(OLED_SCL));

        Wire.begin(OLED_SDA, OLED_SCL);
        Wire.setTimeOut(20);
        oled = new GyverOLED<SSD1306_128x64, OLED_BUFFER>(OLED_ADDRESS);
        currentAddr = OLED_ADDRESS;
        activeCfg = {OLED_SDA, OLED_SCL, OLED_ADDRESS, OLED_I2C_CLOCK};
        oled->clear();
    }
    lastProbe = millis();
}

// Periodically checks the display: if it disappears, reinitialise it
// when it comes back. Also survives a flaky connection.
void tick() {
    uint32_t now = millis();
    if (now - lastProbe < 2000) return;
    lastProbe = now;

    if (displayPresent) {
        if (!i2cPresent(activeCfg.addr)) {
            displayPresent = false;
            Wire.end();
            Serial.println("[OLED] display lost, retrying...");
        }
        return;
    }

    DisplayConfig cfg;
    if (!detectDisplay(cfg)) return;

    Wire.end();
    initDisplay(cfg);
    Serial.printf("[OLED] display reconnected at 0x%02X (SDA %d, SCL %d, %lu kHz)\n",
                  cfg.addr, cfg.sda, cfg.scl,
                  static_cast<unsigned long>(cfg.speed / 1000));
    oled->clear();
    oled->update();
}

void setRotation(uint8_t r) { currentRotation = r & 0x03; }
uint8_t rotation() { return currentRotation; }

int width() { return (currentRotation & 1) ? 64 : 128; }
int height() { return (currentRotation & 1) ? 128 : 64; }
bool portrait() { return height() > width(); }

void clear() {
    if (oled) oled->clear();
}

void present() {
    if (displayPresent && oled) oled->update();
}

void pixel(int x, int y, bool on) {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;

    int px, py;
    switch (currentRotation) {
        case 1:
            px = 127 - y;
            py = x;
            break;
        case 2:
            px = 127 - x;
            py = 63 - y;
            break;
        case 3:
            px = y;
            py = 63 - x;
            break;
        default:
            px = x;
            py = y;
            break;
    }
    if (oled) oled->dot(px, py, on ? 1 : 0);
}

void hline(int x0, int x1, int y, bool on) {
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int x = x0; x <= x1; x++) pixel(x, y, on);
}

void vline(int x, int y0, int y1, bool on) {
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (int y = y0; y <= y1; y++) pixel(x, y, on);
}

void fillRect(int x, int y, int w, int h, bool on) {
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            pixel(x + xx, y + yy, on);
        }
    }
}

void rect(int x, int y, int w, int h, bool on) {
    if (w <= 0 || h <= 0) return;
    hline(x, x + w - 1, y, on);
    hline(x, x + w - 1, y + h - 1, on);
    vline(x, y, y + h - 1, on);
    vline(x + w - 1, y, y + h - 1, on);
}

void line(int x0, int y0, int x1, int y1, bool on) {
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pixel(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

int text(int x, int y, const char *s, uint8_t scale, bool on) {
    if (scale < 1) scale = 1;
    const char *p = s;
    int cx = x;
    while (*p) {
        uint32_t cp = nextCodepoint(p);
        drawGlyph5x7(cx, y, glyphFor(cp), scale, on);
        cx += 6 * scale;
    }
    return cx - x;
}

int textWidth(const char *s, uint8_t scale) {
    if (scale < 1) scale = 1;
    const char *p = s;
    int w = 0;
    while (*p) {
        nextCodepoint(p);
        w += 6 * scale;
    }
    return w;
}

void textCenter(int cx, int y, const char *s, uint8_t scale, bool on) {
    int w = textWidth(s, scale);
    text(cx - w / 2, y, s, scale, on);
}

void textRight(int rx, int y, const char *s, uint8_t scale, bool on) {
    int w = textWidth(s, scale);
    text(rx - w, y, s, scale, on);
}

int textClipped(int x, int y, int maxW, const char *s, uint8_t scale, bool on) {
    if (scale < 1) scale = 1;
    const char *p = s;
    int cx = x;
    while (*p) {
        const char *save = p;
        uint32_t cp = nextCodepoint(p);
        if (cx - x + 6 * scale > maxW) {
            p = save;
            break;
        }
        drawGlyph5x7(cx, y, glyphFor(cp), scale, on);
        cx += 6 * scale;
    }
    return cx - x;
}

void icon8(int x, int y, const uint8_t *bmp, bool on) {
    for (int row = 0; row < 8; row++) {
        uint8_t bits = bmp[row];
        for (int col = 0; col < 8; col++) {
            if ((bits >> col) & 1) pixel(x + col, y + row, on);
        }
    }
}

void arrowLeft(int x, int y, int h) {
    if (h < 3) h = 3;
    if ((h & 1) == 0) h++;
    int half = h / 2;
    for (int r = 0; r < h; r++) {
        int d = abs(r - half);
        // Tip on the left: the middle row is drawn from x to the base,
        // the outer rows draw only the vertical base on the right.
        int start = x + d;
        hline(start, x + half, y + r, true);
    }
}

void arrowRight(int x, int y, int h) {
    if (h < 3) h = 3;
    if ((h & 1) == 0) h++;
    int half = h / 2;
    for (int r = 0; r < h; r++) {
        int d = abs(r - half);
        int end = x + (half - d);
        hline(x, end, y + r, true);
    }
}

}  // namespace gfx
