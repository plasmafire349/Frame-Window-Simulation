#include "FWCSimulator.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

void FWCSimulator::startSimulation(const ReplayData& data) {
    m_replay = data;
    m_state = SimState::SeekingAnchor;
    m_currentActionIndex = 0;
    m_currentFrame = 0;
    m_results.clear();
    m_anchorCheckpoint = nullptr;
    
    // sort actions by frame
    std::sort(m_replay.actions.begin(), m_replay.actions.end(), [](const ReplayAction& a, const ReplayAction& b) {
        return a.frame < b.frame;
    });

    log::info("Started simulation with {} actions", m_replay.actions.size());
}

void FWCSimulator::stopSimulation() {
    m_state = SimState::Idle;
    if (m_anchorCheckpoint) {
        m_anchorCheckpoint->release();
        m_anchorCheckpoint = nullptr;
    }
}

void FWCSimulator::onPlayerDeath() {
    if (m_state == SimState::Idle) return;
    m_diedThisTest = true;
}

void FWCSimulator::saveAnchor(GJBaseGameLayer* layer) {
    if (m_anchorCheckpoint) {
        m_anchorCheckpoint->release();
    }
    // createCheckpoint returns a retained CheckpointObject usually, or autoreleased?
    // In GD it's autoreleased, we should retain it.
    PlayLayer* pLayer = typeinfo_cast<PlayLayer*>(layer);
    if (pLayer) {
        m_anchorCheckpoint = pLayer->createCheckpoint();
        m_anchorCheckpoint->retain();
        log::info("Saved anchor at frame {}", m_currentFrame);
    }
}

void FWCSimulator::loadAnchor(GJBaseGameLayer* layer) {
    PlayLayer* pLayer = typeinfo_cast<PlayLayer*>(layer);
    if (pLayer && m_anchorCheckpoint) {
        pLayer->loadFromCheckpoint(m_anchorCheckpoint);
        m_currentFrame = m_anchorFrame;
        m_diedThisTest = false;
    }
}

void FWCSimulator::advanceToNextClick(GJBaseGameLayer* layer) {
    if (m_currentActionIndex >= m_replay.actions.size()) {
        m_state = SimState::Finished;
        exportResults();
        stopSimulation();
        return;
    }

    ReplayAction action = m_replay.actions[m_currentActionIndex];
    
    // Anchor should be 15 frames before the click
    m_anchorFrame = std::max(0, action.frame - 15);
    m_state = SimState::SeekingAnchor;
}

void FWCSimulator::processTick(GJBaseGameLayer* layer) {
    if (m_state == SimState::Idle) {
        m_currentFrame++;
        return;
    }

    if (m_state == SimState::SeekingAnchor) {
        if (m_currentFrame == m_anchorFrame) {
            saveAnchor(layer);
            m_state = SimState::TestingEarly;
            m_testDelta = 0;
            m_earliestSurviving = m_replay.actions[m_currentActionIndex].frame;
            loadAnchor(layer);
            return;
        }
        m_currentFrame++;
        return;
    }

    ReplayAction action = m_replay.actions[m_currentActionIndex];

    if (m_state == SimState::TestingEarly) {
        // We are currently in a test run for m_testDelta
        int shiftedClickFrame = action.frame + m_testDelta;
        
        // If we reach 30 frames past the original click without dying, we consider it survived!
        if (m_currentFrame > action.frame + 30) {
            m_earliestSurviving = shiftedClickFrame;
            m_testDelta--;
            if (m_testDelta < -15) { // Give up testing early if more than 15 frames
                m_state = SimState::TestingLate;
                m_testDelta = 1;
                m_latestSurviving = action.frame;
                loadAnchor(layer);
                return;
            }
            loadAnchor(layer);
            return;
        }

        if (m_diedThisTest) {
            // Found the failure point
            m_state = SimState::TestingLate;
            m_testDelta = 1;
            m_latestSurviving = action.frame;
            loadAnchor(layer);
            return;
        }

        // Apply input if it's the shifted frame
        if (m_currentFrame == shiftedClickFrame) {
            layer->pushButton(1, true); // P1
            if (!action.hold) {
                layer->pushButton(1, false);
            }
        }

        m_currentFrame++;
        return;
    }
    
    if (m_state == SimState::TestingLate) {
        int shiftedClickFrame = action.frame + m_testDelta;
        
        if (m_currentFrame > action.frame + 30) {
            m_latestSurviving = shiftedClickFrame;
            m_testDelta++;
            if (m_testDelta > 15) { 
                m_state = SimState::Validating;
                loadAnchor(layer);
                return;
            }
            loadAnchor(layer);
            return;
        }

        if (m_diedThisTest) {
            m_state = SimState::Validating;
            loadAnchor(layer);
            return;
        }

        if (m_currentFrame == shiftedClickFrame) {
            layer->pushButton(1, true);
            if (!action.hold) {
                layer->pushButton(1, false);
            }
        }

        m_currentFrame++;
        return;
    }

    if (m_state == SimState::Validating) {
        // Record and move to next
        WindowResult res;
        res.clickFrame = action.frame;
        res.earliest = m_earliestSurviving;
        res.latest = m_latestSurviving;
        m_results.push_back(res);
        
        log::info("Click {}: earliest={}, latest={}, window={}", action.frame, m_earliestSurviving, m_latestSurviving, m_latestSurviving - m_earliestSurviving + 1);
        
        m_currentActionIndex++;
        advanceToNextClick(layer);
    }
}

void FWCSimulator::exportResults() {
    log::info("Exporting FWC results!");
    
    matjson::Value arr = matjson::Array();
    for (const auto& res : m_results) {
        matjson::Value obj = matjson::Object();
        obj["time"] = res.clickFrame / m_replay.fps;
        obj["window"] = res.latest - res.earliest + 1;
        arr.push_back(obj);
    }

    std::filesystem::path outPath = geode::dirs::getModsDir() / "antigravity.fwc_simulator" / "output.json";
    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream out(outPath);
    out << arr.dump(matjson::NO_INDENTATION);
    out.close();

    geode::Notification::create("Simulation Complete! Saved to output.json", geode::NotificationIcon::Success)->show();
}
