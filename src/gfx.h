#pragma once

#include <Arduino.h>

// ============================================================
//  gfx — a thin wrapper over GyverOLED with software rotation.
//
//  Logical coordinates are always 128x64 (landscape) or 64x128
//  (portrait at rotation 90/270). Internally the coordinates
//  are transformed into physical 128x64.
// ============================================================

namespace gfx {

void begin();
// Check/restore the display link (call in loop).
void tick();

void setRotation(uint8_t r);   // 0..3 -> 0, 90, 180, 270
uint8_t rotation();
int width();
int height();
bool portrait();

void clear();
void present();

void pixel(int x, int y, bool on = true);
void hline(int x0, int x1, int y, bool on = true);
void vline(int x, int y0, int y1, bool on = true);
void fillRect(int x, int y, int w, int h, bool on = true);
void rect(int x, int y, int w, int h, bool on = true);
void line(int x0, int y0, int x1, int y1, bool on = true);

// Returns the width of the drawn text.
int text(int x, int y, const char* s, uint8_t scale = 1, bool on = true);
int textWidth(const char* s, uint8_t scale = 1);
void textCenter(int cx, int y, const char* s, uint8_t scale = 1, bool on = true);
void textRight(int rx, int y, const char* s, uint8_t scale = 1, bool on = true);
// Clips the string to maxW (no ellipsis), returns the width.
int textClipped(int x, int y, int maxW, const char* s, uint8_t scale = 1, bool on = true);

void icon8(int x, int y, const uint8_t* bmp, bool on = true);

void arrowLeft(int x, int y, int h = 7);
void arrowRight(int x, int y, int h = 7);

}  // namespace gfx
