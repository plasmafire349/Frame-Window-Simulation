#include "ReplayReader.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

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

static ReplayFileResult parseGDR2(const uint8_t* buf, size_t len) {
    ReplayFileResult res;
    size_t pos = 0;

    if (len < 3 || buf[0] != 'G' || buf[1] != 'D' || buf[2] != 'R') return res;
    pos = 3;

    readVarint(buf, pos, len); // version
    readString(buf, pos, len); // inputTag
    readString(buf, pos, len); // author
    readString(buf, pos, len); // description
    res.duration = readFloatBE(buf, pos); // duration

    readVarint(buf, pos, len); // gameVersion
    double framerate = readDoubleBE(buf, pos);
    res.fps = static_cast<float>(framerate > 0.0 ? framerate : 240.0);

    readVarint(buf, pos, len); // seed
    readVarint(buf, pos, len); // coins
    bool isPlatformer = false;
    if (pos < len) pos++; // ldm
    if (pos < len) isPlatformer = (buf[pos++] != 0);

    readString(buf, pos, len); // botName
    readVarint(buf, pos, len); // botVersion
    readVarint(buf, pos, len); // levelId
    readString(buf, pos, len); // levelName

    uint64_t extSize = readVarint(buf, pos, len);
    pos += extSize;

    uint64_t deathCount = readVarint(buf, pos, len);
    for (uint64_t i = 0; i < deathCount; i++) {
        readVarint(buf, pos, len);
    }

    uint64_t inputCount = readVarint(buf, pos, len);
    uint64_t p1Count = readVarint(buf, pos, len);

    int64_t runningFrame = 0;
    uint64_t remainingP1 = p1Count;
    bool inP2 = false;

    for (uint64_t i = 0; i < inputCount; i++) {
        if (pos >= len) break;
        uint64_t packed = readVarint(buf, pos, len);

        bool down = !!((packed >> 1) & 1);
        int button = 1;
        int delta = 0;

        if (isPlatformer) {
            button = static_cast<int>((packed >> 2) & 3);
            delta = static_cast<int>(packed >> 4);
        } else {
            button = 1;
            delta = static_cast<int>(packed >> 2);
        }

        if (remainingP1 == 0 && !inP2) {
            inP2 = true;
            runningFrame = 0;
        }

        runningFrame += delta;

        // Keep every action (press and release) so all inputs are tracked
        RawReplayAction act;
        act.frame = static_cast<int>(runningFrame);
        act.hold = down;
        act.button = (button == 0) ? 1 : button;
        act.isPlayer2 = inP2;
        res.actions.push_back(act);

        if (remainingP1 > 0) remainingP1--;
    }

    std::sort(res.actions.begin(), res.actions.end(), [](const RawReplayAction& a, const RawReplayAction& b) {
        return a.frame < b.frame;
    });

    return res;
}

ReplayFileResult parseReplay(const std::filesystem::path& path) {
    ReplayFileResult res;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        log::error("Could not open replay file: {}", path.string());
        return res;
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> raw(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(raw.data()), size)) {
        log::error("Could not read replay bytes: {}", path.string());
        return res;
    }
    f.close();

    // Check GDR2 header
    if (raw.size() >= 3 && raw[0] == 'G' && raw[1] == 'D' && raw[2] == 'R') {
        log::info("Parsing as GDR2 replay");
        return parseGDR2(raw.data(), raw.size());
    }

    // Try parsing as JSON
    std::string text(raw.begin(), raw.end());
    auto parseRes = matjson::parse(text);
    if (parseRes) {
        auto json = parseRes.unwrap();
        if (json.contains("fps") && json["fps"].isNumber()) {
            res.fps = static_cast<float>(json["fps"].as<double>().unwrapOr(240.0));
        }
        if (json.contains("duration") && json["duration"].isNumber()) {
            res.duration = static_cast<float>(json["duration"].as<double>().unwrapOr(0.0));
        }

        auto processItem = [&](const matjson::Value& obj) {
            bool hold = obj.contains("hold") ? obj["hold"].as<bool>().unwrapOr(true)
                      : (obj.contains("down") ? obj["down"].as<bool>().unwrapOr(true) : true);
            RawReplayAction act;
            act.frame = obj["frame"].as<int>().unwrapOr(0);
            act.hold = hold;
            act.button = obj.contains("button") ? obj["button"].as<int>().unwrapOr(1) : 1;
            act.isPlayer2 = obj.contains("p2") ? obj["p2"].as<bool>().unwrapOr(false) : false;
            res.actions.push_back(act);
        };

        if (json.contains("actions") && json["actions"].isArray()) {
            auto arr = json["actions"].as<std::vector<matjson::Value>>();
            if (arr.isOk()) {
                for (const auto& item : arr.unwrap()) processItem(item);
            }
        } else if (json.isArray()) {
            auto arr = json.as<std::vector<matjson::Value>>();
            if (arr.isOk()) {
                for (const auto& item : arr.unwrap()) processItem(item);
            }
        }

        std::sort(res.actions.begin(), res.actions.end(), [](const RawReplayAction& a, const RawReplayAction& b) {
            return a.frame < b.frame;
        });
    }

    return res;
}
