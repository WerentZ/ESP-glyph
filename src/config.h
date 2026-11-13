#pragma once

// ============================================================
//  Glyph Matrix — configuration
// ============================================================

#define PROJECT_NAME "Glyph Matrix"
#define PROJECT_VERSION "1.0.0"
#define BOARD_NAME "ESP32 DevKit V1"
#define BOARD_SHORT "ESP32 DevKit"

// ---------- OLED (SSD1306 0.96" 128x64, I2C) ----------
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C
#define OLED_I2C_CLOCK 400000
#define GLYPH_CONTRAST 207

// ---------- Buttons ----------
// Buttons short the pin to GND; the internal pull-up is used.
#define BTN_UP_PIN 19
#define BTN_DOWN_PIN 23
#define BTN_OK_PIN 18

// ---------- Wi-Fi ----------
// Access point the phone connects to.
#define AP_SSID "GlyphMatrix"
#define AP_PASS "glyph1234"
#define AP_CHANNEL 6
#define AP_HIDDEN false
#define AP_MAX_CLIENTS 4

// Optional connection to a home network.
// Leave empty to run as an access point only.
#define STA_SSID ""
#define STA_PASS ""

#define MDNS_HOST "glyph"
#define WEB_PORT 80

// ---------- Animations ----------
#define ANIM_EXT ".anim"
#define ANIM_HEADER_SIZE 16
#define ANIM_MAX_W 64
#define ANIM_MAX_H 64
#define ANIM_MAX_FRAMES 512
#define ANIM_MAX_FILE_BYTES (256 * 1024)
#define ANIM_UPLOAD_LIMIT (ANIM_MAX_FILE_BYTES + 1024)

#define DEFAULT_GRID 25
#define DEFAULT_FPS 24
#define MIN_FPS 1
#define MAX_FPS 30

// UI refresh rate. At 400 kHz I2C a full 128x64 frame takes about
// 23 ms to reach the display, so 30 FPS is the practical limit.
#define UI_FPS 30

// ---------- Misc ----------
#define SPLASH_MS 1400
#define BOOT_INFO_MS 2200
#define LONG_PRESS_MS 650
