#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <filesystem>
#include "ClickData.hpp"

struct RawReplayAction {
    int frame = 0;
    bool hold = false;
    int button = 1;
    bool isPlayer2 = false;
};

struct ReplayFileResult {
    std::vector<RawReplayAction> actions;
    float fps = 240.0f;
};

ReplayFileResult parseReplay(const std::filesystem::path& path);
