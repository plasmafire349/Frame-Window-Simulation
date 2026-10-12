#pragma once
#include <Geode/Geode.hpp>
#include <vector>
#include <map>
#include <tuple>
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
    bool m_botOnly = true;

    std::vector<ClickMarkerInfo> m_clicks;
    float m_fps = 240.0f;
    float m_duration = 0.0f;
    int m_currentFrame = 0;
    int m_lastTriggeredFrame = -1;

    // Manual tier assignments: clickIndex -> tier
    std::map<int, ClickTier> m_manualClickTiers;
    std::vector<std::tuple<float, float, ClickTier>> m_percentRules;

    // HUD tracking
    int m_currentClicksHit = 0;
    int m_livePressCount = 0;
    int m_liveRelCount = 0;
    int m_lastActionIndex = 0;
    std::map<ClickTier, int> m_tierCounts;

    // Live Recording & Preview
    bool m_isRecording = false;
    bool m_isPreviewing = false;
    std::vector<ClickMarkerInfo> m_recordedClicks;

    std::filesystem::path getRecordingsDir();
    std::filesystem::path getLatestRecordingFile(const std::string& levelName);
    std::string saveCurrentRecording(const std::string& levelName);

    void loadReplayFile(const std::filesystem::path& path, float levelLength);
    void exportClicksJson(const std::filesystem::path& path);
    void loadClicksJson(const std::filesystem::path& path);
    void reset();

    bool assignByClick(int clickIndex, ClickTier tier);
    ClickTier cycleClickTier(int clickIndex);
    bool assignByPercent(float percent, ClickTier tier, float tolerance = 0.75f);
    int assignByPercentRange(float minPercent, float maxPercent, ClickTier tier);
    void applyAssignments();

    int getTotalActiveClicks() const;
    int getHitActiveClicks() const;
    int getTierAssignedCount(ClickTier tier) const;
    int getTierHitCount(ClickTier tier) const;

    void stepPhysicsFrame(PlayLayer* layer);
    void onPhysicsStep(PlayLayer* layer);
    void onPlayerDeath();

    cocos2d::CCNode* createMarker(const ClickMarkerInfo& info);
};
