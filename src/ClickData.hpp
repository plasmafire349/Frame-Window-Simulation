#pragma once
#include <Geode/Geode.hpp>
#include <string>
#include <vector>

enum class ClickTier : char {
    None = '-',
    A = 'A',
    B = 'B',
    C = 'C',
    D = 'D',
    E = 'E'
};

struct ClickMarkerInfo {
    int frame = 0;
    int clickIndex = 0;
    float percent = 0.0f;
    ClickTier tier = ClickTier::None;
    bool isHold = true; // true = press/down, false = release/up
    bool isPlayer2 = false;
    cocos2d::CCPoint position = { 0.f, 0.f };
    bool hasTriggered = false;
};

inline cocos2d::ccColor4F getTierColor(ClickTier tier) {
    switch (tier) {
        case ClickTier::A:    return { 1.0f, 0.20f, 0.20f, 1.0f }; // Red (Hardest)
        case ClickTier::B:    return { 1.0f, 0.55f, 0.00f, 1.0f }; // Orange
        case ClickTier::C:    return { 1.0f, 0.85f, 0.00f, 1.0f }; // Yellow
        case ClickTier::D:    return { 0.2f, 0.95f, 0.20f, 1.0f }; // Green
        case ClickTier::E:    return { 0.0f, 0.75f, 1.00f, 1.0f }; // Cyan (Easiest)
        case ClickTier::None: return { 0.5f, 0.50f, 0.50f, 0.6f }; // Subtle Gray (Disabled)
        default:              return { 1.0f, 1.00f, 1.00f, 1.0f };
    }
}

inline std::string getTierSound(ClickTier tier) {
    switch (tier) {
        case ClickTier::A: return "tierA.mp3";
        case ClickTier::B: return "tierB.mp3";
        case ClickTier::C: return "tierC.wav";
        case ClickTier::D: return "tierD.mp3";
        case ClickTier::E: return "tierE.mp3";
        default:           return "";
    }
}
