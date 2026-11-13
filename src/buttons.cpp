#include "buttons.h"

void Button::begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    stable = false;
    lastRaw = false;
    clickFlag = false;
    longFlag = false;
    repeatFlag = false;
    longFired = false;
    repeatFired = false;
    lastChange = millis();
}

void Button::update(uint32_t now) {
    bool raw = (digitalRead(pin) == LOW);

    if (raw != lastRaw) {
        lastRaw = raw;
        lastChange = now;
    }

    if (now - lastChange >= 25 && raw != stable) {
        stable = raw;
        if (stable) {
            pressAt = now;
            nextRepeat = now + 420;
            longFired = false;
            repeatFired = false;
            repeatFlag = false;
        } else {
            if (!longFired && !repeatFired) clickFlag = true;
        }
    }

    if (stable) {
        if (!longFired && now - pressAt >= LONG_PRESS_MS) {
            longFired = true;
            longFlag = true;
        }
        if (now >= nextRepeat) {
            repeatFlag = true;
            repeatFired = true;
            nextRepeat = now + 85;
        }
    }
}

bool Button::click() {
    if (!clickFlag) return false;
    clickFlag = false;
    return true;
}

bool Button::longPress() {
    if (!longFlag) return false;
    longFlag = false;
    return true;
}

bool Button::repeat() {
    if (!repeatFlag) return false;
    repeatFlag = false;
    return true;
}
