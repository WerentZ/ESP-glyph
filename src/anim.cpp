#include "anim.h"

#include <LittleFS.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "gfx.h"
#include "settings.h"

namespace {

const uint8_t MAGIC[4] = {'G', 'L', 'Y', 'M'};
const uint8_t FORMAT_VERSION = 1;

bool isForbiddenByte(uint8_t c) {
    if (c < 0x20) return true;
    switch (c) {
        case '/':
        case '\\':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            return true;
    }
    return false;
}

}  // namespace

uint32_t AnimInfo::durationMs(uint16_t fpsOverride) const {
    uint16_t f = fpsOverride ? fpsOverride : (fps ? fps : DEFAULT_FPS);
    if (!f) return 0;
    return static_cast<uint32_t>(frames) * 1000UL / f;
}

namespace anim {

bool begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS mount failed");
        return false;
    }
    Serial.printf("[FS] LittleFS: %u / %u bytes\n",
                  static_cast<unsigned>(LittleFS.usedBytes()),
                  static_cast<unsigned>(LittleFS.totalBytes()));

    // Remove unfinished uploads left after a reboot.
    std::vector<String> stale;
    File dir = LittleFS.open("/");
    if (dir && dir.isDirectory()) {
        File f = dir.openNextFile();
        while (f) {
            String n = f.name();
            if (!f.isDirectory() && n.endsWith(".part")) stale.push_back(n);
            f.close();
            f = dir.openNextFile();
        }
        dir.close();
    }
    for (size_t i = 0; i < stale.size(); i++) {
        if (LittleFS.remove(stale[i])) {
            Serial.printf("[FS] removed incomplete upload: %s\n", stale[i].c_str());
        }
    }
    return true;
}

String path(const String &name) { return String("/") + name + ANIM_EXT; }

String cleanName(const String &raw) {
    String n = raw;
    int slash = n.lastIndexOf('/');
    if (slash >= 0) n = n.substring(slash + 1);
    int backslash = n.lastIndexOf('\\');
    if (backslash >= 0) n = n.substring(backslash + 1);
    if (n.endsWith(ANIM_EXT)) n = n.substring(0, n.length() - strlen(ANIM_EXT));
    n.trim();

    String out;
    for (size_t i = 0; i < n.length(); i++) {
        uint8_t c = static_cast<uint8_t>(n[i]);
        if (isForbiddenByte(c)) continue;
        if (out.length() >= 24) break;
        out += static_cast<char>(c);
    }
    out.trim();
    while (out.endsWith(".")) out.remove(out.length() - 1);
    // Do not cut a UTF-8 sequence in the middle.
    if (out.length() >= 24) {
        while (out.length() && (static_cast<uint8_t>(out[out.length() - 1]) & 0x80)) {
            out.remove(out.length() - 1);
        }
    }
    if (out == "." || out == "..") out = "";
    return out;
}

bool validName(const String &name) {
    if (name.length() == 0 || name.length() > 24) return false;
    if (name == "." || name == "..") return false;
    for (size_t i = 0; i < name.length(); i++) {
        if (isForbiddenByte(static_cast<uint8_t>(name[i]))) return false;
    }
    return true;
}

bool readInfoAt(const String &fsPath, AnimInfo &info) {
    File f = LittleFS.open(fsPath, "r");
    if (!f) return false;

    size_t size = f.size();
    uint8_t hdr[ANIM_HEADER_SIZE];
    if (size < ANIM_HEADER_SIZE || f.read(hdr, ANIM_HEADER_SIZE) != ANIM_HEADER_SIZE) {
        f.close();
        return false;
    }
    if (memcmp(hdr, MAGIC, 4) != 0 || hdr[4] != FORMAT_VERSION) {
        f.close();
        return false;
    }

    uint8_t w = hdr[5];
    uint8_t h = hdr[6];
    uint16_t frames = static_cast<uint16_t>(hdr[12] | (hdr[13] << 8));
    if (w == 0 || h == 0 || w > ANIM_MAX_W || h > ANIM_MAX_H) {
        f.close();
        return false;
    }
    if (frames == 0 || frames > ANIM_MAX_FRAMES) {
        f.close();
        return false;
    }
    size_t frameBytes = (static_cast<size_t>(w) * h + 7) / 8;
    size_t expected = ANIM_HEADER_SIZE + frameBytes * frames;
    if (size != expected) {
        f.close();
        return false;
    }

    info.name = "";
    info.w = w;
    info.h = h;
    info.fps = hdr[7];
    info.flags = hdr[8];
    info.frames = frames;
    info.bytes = size;
    f.close();
    return true;
}

bool readInfo(const String &name, AnimInfo &info) {
    if (!validName(name)) return false;
    if (!readInfoAt(path(name), info)) return false;
    info.name = name;
    return true;
}

int scan(std::vector<AnimInfo> &out) {
    out.clear();
    File dir = LittleFS.open("/");
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return 0;
    }

    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String n = f.name();
            if (n.endsWith(ANIM_EXT)) {
                int slash = n.lastIndexOf('/');
                String base = (slash >= 0) ? n.substring(slash + 1) : n;
                base = cleanName(base);
                AnimInfo info;
                if (validName(base) && readInfo(base, info)) {
                    out.push_back(info);
                }
            }
        }
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    std::sort(out.begin(), out.end(), [](const AnimInfo &a, const AnimInfo &b) {
        return a.name < b.name;
    });
    return static_cast<int>(out.size());
}

bool remove(const String &name) {
    if (!validName(name)) return false;
    return LittleFS.remove(path(name));
}

// ------------------------------------------------------------
//  Built-in animations on first boot
// ------------------------------------------------------------

namespace {

typedef void (*FrameGen)(uint16_t index, uint16_t frames, uint8_t *buf,
                         uint8_t w, uint8_t h);

const float TAU = 6.2831853f;

void putPx(uint8_t *buf, uint8_t w, uint8_t h, int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    uint32_t idx = static_cast<uint32_t>(y) * w + x;
    buf[idx >> 3] |= static_cast<uint8_t>(1 << (idx & 7));
}

void genPulse(uint16_t i, uint16_t frames, uint8_t *buf, uint8_t w, uint8_t h) {
    static const uint8_t radii[8] = {4, 6, 8, 10, 11, 10, 8, 6};
    int r = radii[i % 8];
    int cx = w / 2;
    int cy = h / 2;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= r * r) putPx(buf, w, h, x, y);
        }
    }
    (void)frames;
}

void genWave(uint16_t i, uint16_t frames, uint8_t *buf, uint8_t w, uint8_t h) {
    float phase = static_cast<float>(i) / frames * TAU;
    for (int x = 0; x < w; x++) {
        float p = static_cast<float>(x) / w * TAU + phase;
        int y = h / 2 + static_cast<int>(sinf(p) * (h / 3));
        putPx(buf, w, h, x, y);
        putPx(buf, w, h, x, y + 1);
    }
}

void genScan(uint16_t i, uint16_t frames, uint8_t *buf, uint8_t w, uint8_t h) {
    int x = frames > 1 ? static_cast<int>(static_cast<uint32_t>(i) * (w - 1) / (frames - 1))
                       : 0;
    for (int y = 0; y < h; y++) {
        putPx(buf, w, h, x, y);
        putPx(buf, w, h, x - 2, y);
        putPx(buf, w, h, x - 4, y);
    }
}

void genSpiral(uint16_t i, uint16_t frames, uint8_t *buf, uint8_t w, uint8_t h) {
    float base = static_cast<float>(i) / frames * TAU;
    int cx = w / 2;
    int cy = h / 2;
    for (int t = 0; t < 44; t++) {
        float ang = base + t * 0.33f;
        float rr = 1.0f + t * 0.26f;
        int x = cx + static_cast<int>(cosf(ang) * rr);
        int y = cy + static_cast<int>(sinf(ang) * rr);
        putPx(buf, w, h, x, y);
    }
}

bool writeAnim(const char *name, uint8_t w, uint8_t h, uint8_t fps,
               uint16_t frames, FrameGen gen) {
    const size_t frameBytes = (static_cast<size_t>(w) * h + 7) / 8;

    File f = LittleFS.open(path(name), "w");
    if (!f) return false;

    uint8_t hdr[ANIM_HEADER_SIZE] = {'G', 'L', 'Y', 'M', 1, w, h, fps, 1,
                                     0,   0,   0,   static_cast<uint8_t>(frames & 0xFF),
                                     static_cast<uint8_t>(frames >> 8), 0, 0};
    if (f.write(hdr, ANIM_HEADER_SIZE) != ANIM_HEADER_SIZE) {
        f.close();
        LittleFS.remove(path(name));
        return false;
    }

    uint8_t buf[(ANIM_MAX_W * ANIM_MAX_H + 7) / 8];
    for (uint16_t i = 0; i < frames; i++) {
        memset(buf, 0, frameBytes);
        gen(i, frames, buf, w, h);
        if (f.write(buf, frameBytes) != frameBytes) {
            f.close();
            LittleFS.remove(path(name));
            return false;
        }
    }
    f.close();
    Serial.printf("[FS] stock animation %s created\n", name);
    return true;
}

struct StockAnim {
    const char *name;
    uint8_t w;
    uint8_t h;
    uint8_t fps;
    uint16_t frames;
    FrameGen gen;
};

const StockAnim STOCKS[] = {
    {"PULSE", 25, 25, 18, 8, genPulse},
    {"WAVE", 25, 25, 20, 16, genWave},
    {"SCAN", 25, 25, 22, 16, genScan},
    {"SPIRAL", 25, 25, 18, 16, genSpiral},
};

const uint8_t STOCK_VERSION = 1;

}  // namespace

void ensureDemo() {
    if (settings::get().stockVersion >= STOCK_VERSION) return;

    for (const StockAnim &s : STOCKS) {
        if (LittleFS.exists(path(s.name))) continue;
        if (!writeAnim(s.name, s.w, s.h, s.fps, s.frames, s.gen)) return;
    }

    settings::get().stockVersion = STOCK_VERSION;
    settings::save();
}

}  // namespace anim

// ------------------------------------------------------------
//  AnimPlayer
// ------------------------------------------------------------

bool AnimPlayer::open(const String &name) {
    close();
    AnimInfo info;
    if (!anim::readInfo(name, info)) return false;

    file = LittleFS.open(anim::path(name), "r");
    if (!file) return false;

    m_info = info;
    m_index = 0;
    m_last = 0;
    if (!loadFrame()) {
        close();
        return false;
    }
    return true;
}

void AnimPlayer::close() {
    if (file) file.close();
    m_info = AnimInfo();
    m_index = 0;
    m_last = 0;
}

bool AnimPlayer::loadFrame() {
    if (!file) return false;
    size_t offset = ANIM_HEADER_SIZE + static_cast<size_t>(m_index) * m_info.frameBytes();
    if (!file.seek(offset)) return false;
    size_t got = file.read(m_buf, m_info.frameBytes());
    return got == m_info.frameBytes();
}

bool AnimPlayer::nextFrame() {
    if (!file) return false;
    m_index++;
    if (m_index >= m_info.frames) m_index = 0;
    return loadFrame();
}

bool AnimPlayer::advance(uint32_t now, uint16_t fps) {
    if (!file) return false;
    if (fps < 1) fps = DEFAULT_FPS;
    uint32_t interval = 1000UL / fps;
    if (interval == 0) interval = 1;

    if (m_last == 0) {
        m_last = now;
        return false;
    }
    if (now - m_last < interval) return false;

    m_last += interval;
    if (now - m_last > interval * 3) m_last = now;
    return nextFrame();
}

void AnimPlayer::draw() const {
    drawRect(0, 0, gfx::width(), gfx::height());
}

void AnimPlayer::drawRect(int rx, int ry, int rw, int rh) const {
    if (!file || rw <= 0 || rh <= 0) return;
    int scale = min(rw / static_cast<int>(m_info.w), rh / static_cast<int>(m_info.h));
    if (scale < 1) scale = 1;
    int ox = rx + (rw - static_cast<int>(m_info.w) * scale) / 2;
    int oy = ry + (rh - static_cast<int>(m_info.h) * scale) / 2;

    for (uint16_t y = 0; y < m_info.h; y++) {
        for (uint16_t x = 0; x < m_info.w; x++) {
            uint16_t idx = static_cast<uint16_t>(y * m_info.w + x);
            if (m_buf[idx >> 3] & (1 << (idx & 7))) {
                gfx::fillRect(ox + x * scale, oy + y * scale, scale, scale, true);
            }
        }
    }
}
