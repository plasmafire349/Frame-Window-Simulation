#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/utils/file.hpp>
#include "FWCSimulator.hpp"

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        // ensure simulator is idle on start
        FWCSimulator::get().stopSimulation();

        return true;
    }

    void update(float dt) {
        // If simulating, we take control of update stepping.
        // Actually, we can just let it run standard update, but we tell the simulator to process the tick.
        PlayLayer::update(dt);
        FWCSimulator::get().processTick(this);
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        PlayLayer::destroyPlayer(p0, p1);
        FWCSimulator::get().onPlayerDeath();
    }
};

class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = this->getChildByID("right-button-menu");
        if (!menu) return;

        auto spr = ButtonSprite::create("FWC Sim");
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyPauseLayer::onStartSim));
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onStartSim(CCObject*) {
        utils::file::pickFile(
            utils::file::PickMode::OpenFile,
            {"*.json"},
            [this](std::filesystem::path path) {
                ReplayData data = parseReplayFile(path);
                if (data.actions.empty()) {
                    FLAlertLayer::create("Error", "Failed to parse replay or no actions found.", "OK")->show();
                    return;
                }
                
                FWCSimulator::get().startSimulation(data);
                
                // Resume game
                this->onResume(nullptr);
            },
            []() {
                // cancelled
            }
        );
    }
};
