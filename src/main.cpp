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
        file::FilePickOptions::Filter filter;
        filter.description = "JSON Replay Files";
        filter.files = {"*.json"};

        file::FilePickOptions options;
        options.filters.push_back(filter);

        file::pick(file::PickMode::OpenFile, options).listen([this](file::PickResult const* result) {
            if (!result || !result->isOk()) return;
            auto optPath = result->unwrap();
            if (!optPath.has_value()) return;

            auto path = optPath.value();
            ReplayData data = parseReplayFile(path);
            if (data.actions.empty()) {
                FLAlertLayer::create("Error", "Failed to parse replay or no actions found.", "OK")->show();
                return;
            }

            FWCSimulator::get().startSimulation(data);
            this->onResume(nullptr);
        });
    }
};
