#pragma once

#include <Arduino.h>

// ============================================================
//  ui — the whole interface: main screen, settings, information,
//  animation list, preview card and fullscreen playback.
//  Also consumes requests coming from the local website.
// ============================================================

namespace ui {

void begin();
void loop();

// Leave the boot info screen early with the OK button.
bool dismissBootInfo();

bool playing();
String playingName();

}  // namespace ui
