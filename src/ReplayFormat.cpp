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

    auto parseRes = matjson::parse(content);
    if (!parseRes) {
        log::error("Failed to parse JSON replay: {}", parseRes.unwrapErr());
        return data;
    }

    matjson::Value json = parseRes.unwrap();

    if (json.contains("fps") && json["fps"].isNumber()) {
        data.fps = static_cast<float>(json["fps"].as<double>().unwrapOr(240.0));
    }

    if (json.contains("actions") && json["actions"].isArray()) {
        for (auto const& actionObj : json["actions"].as<std::vector<matjson::Value>>().unwrapOr({})) {
            ReplayAction action;
            action.frame = actionObj["frame"].as<int>().unwrapOr(0);
            action.hold = actionObj["hold"].as<bool>().unwrapOr(false);
            action.button = actionObj.contains("button") ? actionObj["button"].as<int>().unwrapOr(1) : 1;
            data.actions.push_back(action);
        }
    } else if (json.isArray()) {
        for (auto const& actionObj : json.as<std::vector<matjson::Value>>().unwrapOr({})) {
            ReplayAction action;
            action.frame = actionObj["frame"].as<int>().unwrapOr(0);
            action.hold = actionObj["down"].as<bool>().unwrapOr(false);
            action.button = 1;
            data.actions.push_back(action);
        }
    }

    return data;
}
