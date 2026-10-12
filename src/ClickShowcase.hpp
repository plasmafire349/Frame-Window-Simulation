#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <map>
#include <filesystem>
#include "ClickData.hpp"

class ClickShowcase {
public:
    static ClickShowcase& get() {
        static ClickShowcase instance;
        return instance;
    }

    bool m_showMarkers = true;
    bool m_playAudio = true;
    bool m_showHUD = true;

    std::vector<ClickMarkerInfo> m_clicks;
    float m_fps = 240.0f;
    int m_lastTriggeredFrame = -1;

    // HUD tracking
    int m_currentClicksHit = 0;
    std::map<ClickTier, int> m_tierCounts;

    void loadReplayFile(const std::filesystem::path& path, float levelLength);
    void exportClicksJson(const std::filesystem::path& path);
    void reset();

    void onPhysicsStep(PlayLayer* layer);
    void onPlayerDeath();

    cocos2d::CCNode* createMarker(const ClickMarkerInfo& info);
};
