#pragma once

#include <Arduino.h>
#include <FS.h>

#include <vector>

#include "config.h"

// ============================================================
//  Animation file format (.anim)
//
//  Offset  Size   Field
//  0       4      "GLYM"
//  4       1      version (1)
//  5       1      grid width  (1..64)
//  6       1      grid height (1..64)
//  7       1      FPS (0 = default)
//  8       1      flags (bit 0 = loop)
//  9       3      reserved
//  12      2      frame count (uint16 LE)
//  14      2      reserved
//  16      ...    frames, bitmap row by row:
//                 bit = y * width + x, LSB of a byte = first pixel
//
//  One frame takes ceil(width * height / 8) bytes.
// ============================================================

struct AnimInfo {
    String name;  // without extension
    uint16_t frames = 0;
    uint8_t w = 0;
    uint8_t h = 0;
    uint8_t fps = 0;
    uint8_t flags = 0;
    size_t bytes = 0;

    uint16_t frameBytes() const { return static_cast<uint16_t>((static_cast<uint16_t>(w) * h + 7) / 8); }
    uint32_t durationMs(uint16_t fpsOverride) const;
};

namespace anim {

bool begin();
String path(const String &name);
String cleanName(const String &raw);
bool validName(const String &name);
int scan(std::vector<AnimInfo> &out);
bool readInfo(const String &name, AnimInfo &info);
// Validates a file by its full path in LittleFS (info.name stays empty).
bool readInfoAt(const String &fsPath, AnimInfo &info);
bool remove(const String &name);
void ensureDemo();

}  // namespace anim

// ============================================================
//  Player: keeps the file open and one frame in a buffer.
// ============================================================

class AnimPlayer {
public:
    bool open(const String &name);
    void close();
    bool isOpen() const { return static_cast<bool>(file); }

    const AnimInfo &info() const { return m_info; }
    uint16_t frameIndex() const { return m_index; }
    uint16_t frameCount() const { return m_info.frames; }
    uint32_t durationMs(uint16_t fps) const { return m_info.durationMs(fps); }

    // Loads the next frame (looping). true — the frame changed.
    bool nextFrame();

    // Call on every tick. true — time to redraw (the frame changed).
    bool advance(uint32_t now, uint16_t fps);

    void resetTiming() { m_last = 0; }

    void draw() const;
    // Draws the frame fitted into a rectangle (keeping the aspect ratio).
    void drawRect(int x, int y, int w, int h) const;

private:
    bool loadFrame();

    File file;
    AnimInfo m_info;
    uint8_t m_buf[ANIM_MAX_W * ANIM_MAX_H / 8];
    uint16_t m_index = 0;
    uint32_t m_last = 0;
};
