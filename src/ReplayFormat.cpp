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

    // Try parsing as JSON using matjson modern API
    auto parseRes = matjson::parse(content);
    if (!parseRes) {
        log::error("Failed to parse JSON replay: {}", parseRes.unwrapErr());
        return data;
    }

    matjson::Value json = parseRes.unwrap();

    if (json.contains("fps") && json["fps"].is_number()) {
        data.fps = static_cast<float>(json["fps"].as_double().unwrapOr(240.0));
    }

    if (json.contains("actions") && json["actions"].is_array()) {
        for (auto const& actionObj : json["actions"].as_array().unwrap()) {
            ReplayAction action;
            action.frame = actionObj["frame"].as_int().unwrapOr(0);
            action.hold = actionObj["hold"].as_bool().unwrapOr(false);
            action.button = actionObj.contains("button") ? actionObj["button"].as_int().unwrapOr(1) : 1;
            data.actions.push_back(action);
        }
    } else if (json.is_array()) {
        for (auto const& actionObj : json.as_array().unwrap()) {
            ReplayAction action;
            action.frame = actionObj["frame"].as_int().unwrapOr(0);
            action.hold = actionObj["down"].as_bool().unwrapOr(false);
            action.button = 1;
            data.actions.push_back(action);
        }
    }

    return data;
}
