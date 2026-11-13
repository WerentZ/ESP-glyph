#include "web.h"

#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <vector>

#include "anim.h"
#include "config.h"
#include "settings.h"
#include "web_assets.h"

namespace {

WebServer http(WEB_PORT);

bool playRequested = false;
String playRequestName;
bool stopRequested = false;
bool animChanged = false;

String statusScreen = "main";
String statusPlaying;

// ---------- upload ----------
File uploadFile;
String uploadName;
size_t uploadWritten = 0;
bool uploadFailed = false;

// ------------------------------------------------------------
//  JSON
// ------------------------------------------------------------

String jsonEscape(const String &s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<uint8_t>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void sendCorsHeaders() {
    // Allow the standalone HTML editor (opened from the phone as a file)
    // to talk to the device API.
    http.sendHeader("Access-Control-Allow-Origin", "*");
    http.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    http.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendJson(int code, const String &body) {
    http.sendHeader("Cache-Control", "no-store");
    sendCorsHeaders();
    http.send(code, "application/json; charset=utf-8", body);
}

void sendError(int code, const String &message) {
    sendJson(code, String("{\"ok\":false,\"error\":\"") + jsonEscape(message) + "\"}");
}

String settingsJson() {
    const settings::Data &c = settings::get();
    String j = "{";
    j += "\"ok\":true";
    j += ",\"fps\":" + String(c.fps);
    j += ",\"rotation\":" + String(c.rotation);
    j += ",\"background\":\"" + String(c.background ? "stars" : "snow") + "\"";
    j += ",\"grid\":" + String(c.grid);
    j += ",\"lang\":\"" + String(c.lang ? "ru" : "en") + "\"";
    j += "}";
    return j;
}

String infoJson() {
    const settings::Data &c = settings::get();

    std::vector<AnimInfo> list;
    anim::scan(list);

    String j = "{";
    j += "\"project\":\"" PROJECT_NAME "\"";
    j += ",\"board\":\"" BOARD_NAME "\"";
    j += ",\"version\":\"" PROJECT_VERSION "\"";
    j += ",\"build\":\"" __DATE__ " " __TIME__ "\"";
    j += ",\"grid\":{\"default\":" + String(c.grid) + ",\"max\":64,\"min\":25}";
    j += ",\"fps\":" + String(c.fps);
    j += ",\"rotation\":" + String(c.rotation);
    j += ",\"background\":\"" + String(c.background ? "stars" : "snow") + "\"";
    j += ",\"lang\":\"" + String(c.lang ? "ru" : "en") + "\"";
    j += ",\"ip\":\"" + jsonEscape(web::ip()) + "\"";
    j += ",\"ap\":\"" AP_SSID "\"";
    j += ",\"ap_pass\":\"" AP_PASS "\"";
    j += ",\"host\":\"" MDNS_HOST ".local\"";
    j += ",\"mac\":\"" + WiFi.macAddress() + "\"";
    j += ",\"heap\":" + String(static_cast<unsigned long>(ESP.getFreeHeap()));
    j += ",\"fs_total\":" + String(static_cast<unsigned long>(LittleFS.totalBytes()));
    j += ",\"fs_used\":" + String(static_cast<unsigned long>(LittleFS.usedBytes()));
    j += ",\"anim_count\":" + String(static_cast<int>(list.size()));
    j += ",\"uptime\":" + String(static_cast<unsigned long>(millis() / 1000));
    j += ",\"screen\":\"" + jsonEscape(statusScreen) + "\"";
    j += ",\"playing\":\"" + jsonEscape(statusPlaying) + "\"";
    j += "}";
    return j;
}

String animationsJson() {
    std::vector<AnimInfo> list;
    anim::scan(list);
    uint16_t fps = settings::get().fps;

    String j = "[";
    for (size_t i = 0; i < list.size(); i++) {
        const AnimInfo &a = list[i];
        if (i) j += ",";
        j += "{";
        j += "\"name\":\"" + jsonEscape(a.name) + "\"";
        j += ",\"w\":" + String(a.w);
        j += ",\"h\":" + String(a.h);
        j += ",\"frames\":" + String(a.frames);
        j += ",\"fps\":" + String(a.fps);
        j += ",\"bytes\":" + String(static_cast<unsigned long>(a.bytes));
        j += ",\"duration_ms\":" + String(a.durationMs(fps));
        j += "}";
    }
    j += "]";
    return j;
}

// ------------------------------------------------------------
//  Static pages (embedded into the firmware)
// ------------------------------------------------------------

void sendHtml(const char *page) {
    http.sendHeader("Cache-Control", "no-cache");
    sendCorsHeaders();
    http.send_P(200, "text/html; charset=utf-8", page);
}

void handleIndex() { sendHtml(INDEX_HTML); }

void handleNotFound() {
    if (http.method() == HTTP_GET) {
        String uri = http.uri();
        if (uri == "/" || uri == "/index.html") {
            handleIndex();
            return;
        }
        if (uri == "/favicon.ico") {
            http.send(204, "text/plain", "");
            return;
        }
    }
    sendError(404, "not found");
}

// ------------------------------------------------------------
//  API
// ------------------------------------------------------------

void handleInfo() { sendJson(200, infoJson()); }
void handleSettingsGet() { sendJson(200, settingsJson()); }

void handleSettingsPost() {
    settings::Data &c = settings::get();

    if (http.hasArg("fps")) {
        int v = http.arg("fps").toInt();
        c.fps = static_cast<uint8_t>(constrain(v, MIN_FPS, MAX_FPS));
    }
    if (http.hasArg("rotation")) {
        int v = http.arg("rotation").toInt();
        c.rotation = static_cast<uint8_t>(constrain(v, 0, 3));
    }
    if (http.hasArg("background")) {
        String b = http.arg("background");
        c.background = (b == "stars" || b == "1") ? 1 : 0;
    }
    if (http.hasArg("grid")) {
        c.grid = (http.arg("grid").toInt() == 64) ? 64 : 25;
    }
    if (http.hasArg("lang")) {
        String l = http.arg("lang");
        c.lang = (l == "ru" || l == "1") ? 1 : 0;
    }
    settings::save();
    sendJson(200, settingsJson());
}

void handleAnimations() { sendJson(200, animationsJson()); }

void handleAnimGet() {
    String name = anim::cleanName(http.arg("name"));
    if (!anim::validName(name)) {
        sendError(400, "bad name");
        return;
    }
    File f = LittleFS.open(anim::path(name), "r");
    if (!f) {
        sendError(404, "not found");
        return;
    }
    http.sendHeader("Cache-Control", "no-store");
    http.sendHeader("Content-Disposition", "attachment; filename=\"animation.anim\"");
    sendCorsHeaders();
    http.streamFile(f, "application/octet-stream");
    f.close();
}

void handleDelete() {
    String name = anim::cleanName(http.arg("name"));
    if (!anim::validName(name)) {
        sendError(400, "bad name");
        return;
    }
    if (!anim::remove(name)) {
        sendError(404, "not found");
        return;
    }
    animChanged = true;
    sendJson(200, "{\"ok\":true}");
}

void handlePlay() {
    String name = anim::cleanName(http.arg("name"));
    AnimInfo info;
    if (!anim::validName(name) || !anim::readInfo(name, info)) {
        sendError(404, "not found");
        return;
    }
    playRequestName = name;
    playRequested = true;
    sendJson(200, "{\"ok\":true,\"name\":\"" + jsonEscape(name) + "\"}");
}

void handleStop() {
    stopRequested = true;
    sendJson(200, "{\"ok\":true}");
}

// ------------------------------------------------------------
//  Animation upload (multipart, field "file")
// ------------------------------------------------------------

String uploadTempPath() { return String("/") + uploadName + ".part"; }

void abortUpload() {
    if (uploadFile) uploadFile.close();
    if (uploadName.length()) LittleFS.remove(uploadTempPath());
    uploadFailed = true;
    uploadWritten = 0;
}

void handleUploadChunk() {
    HTTPUpload &up = http.upload();

    if (up.status == UPLOAD_FILE_START) {
        uploadFailed = false;
        uploadWritten = 0;
        uploadName = anim::cleanName(up.filename.length() ? up.filename : http.arg("name"));
        if (!anim::validName(uploadName)) {
            uploadFailed = true;
            return;
        }
        uploadFile = LittleFS.open(uploadTempPath(), "w");
        if (!uploadFile) {
            uploadFailed = true;
            return;
        }
        Serial.printf("[WEB] upload start: %s\n", uploadName.c_str());
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (uploadFailed) return;
        if (uploadWritten + up.currentSize > ANIM_UPLOAD_LIMIT) {
            Serial.println("[WEB] upload too big");
            abortUpload();
            return;
        }
        if (uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
            Serial.println("[WEB] upload write failed");
            abortUpload();
            return;
        }
        uploadWritten += up.currentSize;
    } else if (up.status == UPLOAD_FILE_END) {
        if (uploadFile) uploadFile.close();
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        abortUpload();
    }
}

void handleUploadDone() {
    if (uploadFile) uploadFile.close();

    if (uploadFailed || uploadWritten == 0) {
        if (uploadName.length()) LittleFS.remove(uploadTempPath());
        uploadName = "";
        uploadWritten = 0;
        uploadFailed = false;
        sendError(400, "upload failed");
        return;
    }

    // Validate the temporary file first and only then replace the old version.
    AnimInfo info;
    if (!anim::readInfoAt(uploadTempPath(), info)) {
        LittleFS.remove(uploadTempPath());
        uploadName = "";
        uploadWritten = 0;
        sendError(400, "invalid animation file");
        return;
    }

    String finalPath = anim::path(uploadName);
    LittleFS.remove(finalPath);
    if (!LittleFS.rename(uploadTempPath().c_str(), finalPath.c_str())) {
        LittleFS.remove(uploadTempPath());
        uploadName = "";
        uploadWritten = 0;
        sendError(500, "cannot save file");
        return;
    }
    info.name = uploadName;

    animChanged = true;
    String body = "{\"ok\":true,\"name\":\"" + jsonEscape(info.name) + "\",\"frames\":" +
                  String(info.frames) + ",\"w\":" + String(info.w) + ",\"h\":" + String(info.h) +
                  "}";
    Serial.printf("[WEB] uploaded %s (%u bytes, %u frames)\n", info.name.c_str(),
                  static_cast<unsigned>(info.bytes), info.frames);

    uploadName = "";
    uploadWritten = 0;
    uploadFailed = false;
    sendJson(200, body);
}

}  // namespace

namespace web {

void begin() {
    bool useSta = strlen(STA_SSID) > 0;

    WiFi.persistent(false);
    WiFi.mode(useSta ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, AP_HIDDEN, AP_MAX_CLIENTS);

    if (useSta) {
        WiFi.setSleep(false);
        WiFi.begin(STA_SSID, STA_PASS);
    }

    MDNS.begin(MDNS_HOST);
    MDNS.addService("http", "tcp", WEB_PORT);

    http.on("/", HTTP_GET, handleIndex);
    http.on("/index.html", HTTP_GET, handleIndex);
    http.on("/api/info", HTTP_GET, handleInfo);
    http.on("/api/settings", HTTP_GET, handleSettingsGet);
    http.on("/api/settings", HTTP_POST, handleSettingsPost);
    http.on("/api/animations", HTTP_GET, handleAnimations);
    http.on("/api/anim", HTTP_GET, handleAnimGet);
    http.on("/api/anim", HTTP_DELETE, handleDelete);
    http.on("/api/anim", HTTP_POST, handleUploadDone, handleUploadChunk);
    http.on("/api/play", HTTP_POST, handlePlay);
    http.on("/api/stop", HTTP_POST, handleStop);
    http.onNotFound(handleNotFound);
    http.begin();

    Serial.printf("[WEB] AP '%s' IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    if (useSta) {
        Serial.printf("[WEB] STA '%s' connecting...\n", STA_SSID);
    }
}

void loop() {
    http.handleClient();
}

String ip() {
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress local = WiFi.localIP();
        if (local != IPAddress(0, 0, 0, 0)) return local.toString();
    }
    return WiFi.softAPIP().toString();
}

String apSsid() { return String(AP_SSID); }

bool takePlayRequest(String &name) {
    if (!playRequested) return false;
    name = playRequestName;
    playRequested = false;
    return true;
}

bool takeStopRequest() {
    if (!stopRequested) return false;
    stopRequested = false;
    return true;
}

bool takeAnimDirty() {
    if (!animChanged) return false;
    animChanged = false;
    return true;
}

void setStatus(const String &screen, const String &playing) {
    statusScreen = screen;
    statusPlaying = playing;
}

}  // namespace web
