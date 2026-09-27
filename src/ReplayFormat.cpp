#include "ReplayFormat.hpp"
#include <fstream>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

ReplayData parseReplayFile(const std::filesystem::path& path) {
    ReplayData data;
    auto result = file::readString(path);
    if (!result) {
        log::error("Failed to read replay file: {}", path.string());
        return data;
    }

    std::string content = result.unwrap();

    // Try parsing as JSON
    try {
        auto json = matjson::parse(content);
        if (json.contains("fps")) {
            data.fps = json["fps"].as_double();
        }
        if (json.contains("actions")) {
            for (auto& actionObj : json["actions"]) {
                ReplayAction action;
                action.frame = actionObj["frame"].as_int();
                action.hold = actionObj["hold"].as_bool();
                action.button = actionObj.contains("button") ? actionObj["button"].as_int() : 1;
                data.actions.push_back(action);
            }
        } else if (json.isArray()) {
            // simple array
            for (auto& actionObj : json) {
                ReplayAction action;
                action.frame = actionObj["frame"].as_int();
                action.hold = actionObj["down"].as_bool();
                action.button = 1;
                data.actions.push_back(action);
            }
        }
    } catch(const std::exception& e) {
        log::error("Failed to parse JSON replay: {}", e.what());
    }

    return data;
}
