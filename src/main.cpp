#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include "FWCSimulator.hpp"

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

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

        // Use a small circular button sprite to fit natively in right-button-menu
        // (same size as the % and gear buttons already there)
        auto spr = CircleButtonSprite::createWithSpriteFrameName(
            "GJ_replayBtn_001.png",
            CircleBaseColor::Green,
            CircleBaseSize::Small
        );
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyPauseLayer::onStartSim));
        btn->setID("fwc-sim-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onStartSim(CCObject*) {
        async::spawn(file::pick(file::PickMode::OpenFile, file::FilePickOptions {
            .filters = { file::FilePickOptions::Filter {
                .description = "Replay Files (*.gdr2, *.gdr, *.json)",
                .files = { "*.gdr2", "*.gdr", "*.json" },
            }}
        }), [this](Result<std::optional<std::filesystem::path>> result) {
            if (result.isOk() && result.unwrap().has_value()) {
                auto path = result.unwrap().value();
                ReplayData data = parseReplayFile(path);
                if (data.actions.empty()) {
                    FLAlertLayer::create("Error", "Failed to parse replay or no actions found.", "OK")->show();
                    return;
                }

                FWCSimulator::get().startSimulation(data);
                this->onResume(nullptr);
            }
        });
    }
};
