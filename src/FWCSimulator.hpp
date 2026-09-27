#pragma once
#include <Geode/Geode.hpp>
#include "ReplayFormat.hpp"

enum class SimState {
    Idle,
    SeekingAnchor,
    TestingEarly,
    TestingLate,
    Validating,
    Finished
};

struct WindowResult {
    int clickFrame;
    int earliest;
    int latest;
};

class FWCSimulator {
public:
    static FWCSimulator& get() {
        static FWCSimulator instance;
        return instance;
    }

    void startSimulation(const ReplayData& data);
    void stopSimulation();
    void processTick(GJBaseGameLayer* layer);
    void onPlayerDeath();

    bool isSimulating() const { return m_state != SimState::Idle; }
    
private:
    SimState m_state = SimState::Idle;
    ReplayData m_replay;
    int m_currentActionIndex = 0;
    
    int m_currentFrame = 0;
    int m_anchorFrame = 0;
    
    int m_testDelta = 0;
    int m_earliestSurviving = 0;
    int m_latestSurviving = 0;
    
    bool m_diedThisTest = false;

    CheckpointObject* m_anchorCheckpoint = nullptr;
    std::vector<WindowResult> m_results;

    void advanceToNextClick(GJBaseGameLayer* layer);
    void loadAnchor(GJBaseGameLayer* layer);
    void saveAnchor(GJBaseGameLayer* layer);
    
    void exportResults();
};
