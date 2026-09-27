#include "ReplayFormat.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

// ─── GDR2 binary helpers ─────────────────────────────────────────────────────

static uint64_t readVarint(const uint8_t* buf, size_t& pos, size_t len) {
    uint64_t result = 0;
    int shift = 0;
    while (pos < len) {
        uint8_t b = buf[pos++];
        result |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return result;
}

static float readFloatBE(const uint8_t* buf, size_t& pos) {
    uint32_t raw = (static_cast<uint32_t>(buf[pos]) << 24) |
                   (static_cast<uint32_t>(buf[pos + 1]) << 16) |
                   (static_cast<uint32_t>(buf[pos + 2]) << 8) |
                   static_cast<uint32_t>(buf[pos + 3]);
    pos += 4;
    float val;
    std::memcpy(&val, &raw, 4);
    return val;
}

static double readDoubleBE(const uint8_t* buf, size_t& pos) {
    uint64_t raw = 0;
    for (int i = 0; i < 8; i++) {
        raw = (raw << 8) | buf[pos++];
    }
    double val;
    std::memcpy(&val, &raw, 8);
    return val;
}

static std::string readString(const uint8_t* buf, size_t& pos, size_t len) {
    uint64_t strLen = readVarint(buf, pos, len);
    if (pos + strLen > len) { pos = len; return ""; }
    std::string s(reinterpret_cast<const char*>(buf + pos), strLen);
    pos += strLen;
    return s;
}

// ─── GDR2 parser ─────────────────────────────────────────────────────────────
// Format from https://github.com/maxnut/GDReplayFormat

static ReplayData parseGDR2(const uint8_t* buf, size_t len) {
    ReplayData data;
    size_t pos = 0;

    // Magic: "GDR"
    if (len < 3 || buf[0] != 'G' || buf[1] != 'D' || buf[2] != 'R') return data;
    pos = 3;

    // version (varint)
    /*uint64_t version =*/ readVarint(buf, pos, len);

    // inputTag, author, description (strings)
    readString(buf, pos, len); // inputTag
    readString(buf, pos, len); // author
    readString(buf, pos, len); // description

    // duration (float BE)
    /*float duration =*/ readFloatBE(buf, pos);

    // gameVersion (varint), framerate (double BE)
    readVarint(buf, pos, len);
    double framerate = readDoubleBE(buf, pos);
    data.fps = static_cast<float>(framerate > 0.0 ? framerate : 240.0);

    // seed, coins (varints), ldm, platformer (bools)
    readVarint(buf, pos, len); // seed
    readVarint(buf, pos, len); // coins
    bool isPlatformer = false;
    if (pos < len) pos++; // ldm
    if (pos < len) isPlatformer = (buf[pos++] != 0); // platformer

    // botName, botVersion
    readString(buf, pos, len);
    readVarint(buf, pos, len);

    // levelId, levelName
    readVarint(buf, pos, len);
    readString(buf, pos, len);

    // extension block (varint size + skip)
    uint64_t extSize = readVarint(buf, pos, len);
    pos += extSize;

    // deaths: deathCount (varint) + that many delta varints
    uint64_t deathCount = readVarint(buf, pos, len);
    for (uint64_t i = 0; i < deathCount; i++) {
        readVarint(buf, pos, len); // delta
    }

    // input counts
    uint64_t inputCount = readVarint(buf, pos, len);
    uint64_t p1Count    = readVarint(buf, pos, len);

    // decode packed inputs
    // We maintain two running frame positions: one for p1, one for p2
    int64_t p = 0;          // current running frame position
    uint64_t remaining1 = p1Count;  // remaining P1 inputs
    bool inP2 = false;

    std::vector<ReplayAction> actions;
    actions.reserve(static_cast<size_t>(inputCount));

    for (uint64_t i = 0; i < inputCount; i++) {
        if (pos >= len) break;

        uint64_t packed = readVarint(buf, pos, len);

        bool down = !!((packed >> 1) & 1);
        int button, delta;

        if (isPlatformer) {
            button = static_cast<int>((packed >> 2) & 3);
            delta  = static_cast<int>(packed >> 4);
        } else {
            button = 1;
            delta  = static_cast<int>(packed >> 2);
        }

        // Transition from P1 → P2: reset running frame counter
        if (remaining1 == 0 && !inP2) {
            inP2 = true;
            p = 0;
        }

        p += delta;

        ReplayAction action;
        action.frame  = static_cast<int>(p);
        action.hold   = down;
        action.button = (button == 0) ? 1 : button; // map 0 → 1 for non-plat

        // For now we only drive player1 inputs (button acts on p1 channel);
        // player2 inputs are also decoded but sent on the same handleButton path.
        actions.push_back(action);

        if (remaining1 > 0) remaining1--;
    }

    // Sort by frame
    std::sort(actions.begin(), actions.end(), [](const ReplayAction& a, const ReplayAction& b) {
        return a.frame < b.frame;
    });

    data.actions = std::move(actions);
    return data;
}

// ─── GDR v1 (legacy text format) helper ──────────────────────────────────────
// GDR v1 is a simple text format: each line is "frame hold" or "frame hold button"
// Some tools also export as JSON arrays — handled below.

static bool parseGDRTextLine(const std::string& line, ReplayAction& out) {
    if (line.empty() || line[0] == '#') return false;
    int frame = 0, hold = 0, button = 1;
    int parsed = std::sscanf(line.c_str(), "%d %d %d", &frame, &hold, &button);
    if (parsed < 2) return false;
    out.frame  = frame;
    out.hold   = (hold != 0);
    out.button = button;
    return true;
}

// ─── Main entry ──────────────────────────────────────────────────────────────

ReplayData parseReplayFile(const std::filesystem::path& path) {
    ReplayData data;

    // Read raw bytes
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        log::error("Failed to open replay file: {}", path.string());
        return data;
    }
    std::streamsize fsize = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> raw(static_cast<size_t>(fsize));
    if (!f.read(reinterpret_cast<char*>(raw.data()), fsize)) {
        log::error("Failed to read replay file: {}", path.string());
        return data;
    }
    f.close();

    // ── GDR2 binary ──────────────────────────────────────────────────────────
    if (raw.size() >= 3 && raw[0] == 'G' && raw[1] == 'D' && raw[2] == 'R') {
        log::info("Parsing as GDR2 binary: {}", path.string());
        return parseGDR2(raw.data(), raw.size());
    }

    // ── Try JSON (covers both FWC JSON format and any JSON replay) ───────────
    std::string content(raw.begin(), raw.end());
    auto parseRes = matjson::parse(content);
    if (parseRes) {
        matjson::Value json = parseRes.unwrap();

        if (json.contains("fps") && json["fps"].isNumber()) {
            data.fps = static_cast<float>(json["fps"].as<double>().unwrapOr(240.0));
        }

        auto parseActions = [&](const std::vector<matjson::Value>& arr, bool altKeys) {
            for (auto const& obj : arr) {
                ReplayAction action;
                action.frame  = obj["frame"].as<int>().unwrapOr(0);
                action.hold   = altKeys
                    ? obj["down"].as<bool>().unwrapOr(false)
                    : obj["hold"].as<bool>().unwrapOr(false);
                action.button = obj.contains("button")
                    ? obj["button"].as<int>().unwrapOr(1)
                    : 1;
                data.actions.push_back(action);
            }
        };

        if (json.contains("actions") && json["actions"].isArray()) {
            auto res = json["actions"].as<std::vector<matjson::Value>>();
            if (res.isOk()) parseActions(res.unwrap(), false);
        } else if (json.isArray()) {
            auto res = json.as<std::vector<matjson::Value>>();
            if (res.isOk()) parseActions(res.unwrap(), true);
        }

        return data;
    }

    // ── GDR v1 plain-text ────────────────────────────────────────────────────
    log::info("Parsing as GDR v1 text: {}", path.string());
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        // trim CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        ReplayAction action;
        if (parseGDRTextLine(line, action)) {
            data.actions.push_back(action);
        }
    }

    return data;
}
