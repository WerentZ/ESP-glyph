#pragma once

#include <Arduino.h>

#include "config.h"

namespace settings {

struct Data {
    uint8_t fps = DEFAULT_FPS;
    uint8_t rotation = 0;       // 0, 1, 2, 3 -> 0, 90, 180, 270
    uint8_t background = 0;     // 0 = snow, 1 = stars
    uint8_t grid = DEFAULT_GRID; // 25 or 64 (default for new animations)
    uint8_t stockVersion = 0;    // built-in animations version
};

void begin();
Data &get();
void save();

}  // namespace settings
