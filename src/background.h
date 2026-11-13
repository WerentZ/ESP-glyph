#pragma once

#include <Arduino.h>

// Menu background: snow (moving particles) or stars (static,
// with a slight twinkle). Drawn only below the header.
namespace background {

void begin();
void reset();

// Call every frame before drawing the header/content.
// top/bottom — bounds of the area in logical coordinates.
void draw(int top, int bottom, uint32_t now);

}  // namespace background
