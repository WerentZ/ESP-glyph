#pragma once

#include <Arduino.h>

namespace web {

void begin();
void loop();

String ip();
String apSsid();

// Requests from the website, consumed by the UI.
bool takePlayRequest(String &name);
bool takeStopRequest();
bool takeAnimDirty();

// Current state for /api/info.
void setStatus(const String &screen, const String &playing);

}  // namespace web
