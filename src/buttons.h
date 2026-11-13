#pragma once

#include <Arduino.h>

#include "config.h"

// Button with debouncing, short click, long press and auto-repeat.
class Button {
public:
    void begin(uint8_t pin);
    void update(uint32_t now);

    bool click();       // short press (reported on release)
    bool longPress();   // held for LONG_PRESS_MS
    bool repeat();      // auto-repeat while held
    bool isDown() const { return stable; }

private:
    uint8_t pin = 255;
    bool stable = false;      // true = pressed
    bool lastRaw = false;
    bool longFired = false;
    bool repeatFired = false;
    uint32_t lastChange = 0;
    uint32_t pressAt = 0;
    uint32_t nextRepeat = 0;
    bool clickFlag = false;
    bool longFlag = false;
    bool repeatFlag = false;
};
