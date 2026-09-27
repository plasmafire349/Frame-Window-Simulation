#pragma once
#include <Geode/Geode.hpp>
#include <vector>

struct ReplayAction {
    int frame;
    bool hold;
    int button; // 1 = P1 Jump
};

struct ReplayData {
    std::vector<ReplayAction> actions;
    float fps = 240.f;
};

// Parses a simple JSON replay or GDR2 if possible
ReplayData parseReplayFile(const std::filesystem::path& path);
