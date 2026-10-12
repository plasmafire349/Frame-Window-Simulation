#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include "ClickShowcase.hpp"
#include "ShowcasePopup.hpp"

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_hudLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        ClickShowcase::get().reset();

        // Create clean HUD overlay in top-left
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto hud = CCLabelBMFont::create("Clicks: 0", "bigFont.fnt");
        hud->setAnchorPoint({ 0.0f, 1.0f });
        hud->setPosition({ 10.0f, winSize.height - 10.0f });
        hud->setScale(0.4f);
        hud->setID("click-showcase-hud"_spr);

        if (this->m_uiLayer) {
            this->m_uiLayer->addChild(hud);
        } else {
            this->addChild(hud, 999);
        }

        m_fields->m_hudLabel = hud;

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        ClickShowcase::get().onPhysicsStep(this);

        // Update HUD display
        if (m_fields->m_hudLabel && ClickShowcase::get().m_showHUD) {
            auto& sc = ClickShowcase::get();
            size_t totalInReplay = sc.m_clicks.size();

            std::string text;
            if (totalInReplay > 0) {
                int totalActive = sc.getTotalActiveClicks();
                int hitActive = sc.getHitActiveClicks();

                auto formatTier = [&](ClickTier t) -> std::string {
                    int assigned = sc.getTierAssignedCount(t);
                    int hit = sc.getTierHitCount(t);
                    if (assigned > 0) {
                        return fmt::format("{}/{}", hit, assigned);
                    }
                    return "0";
                };

                text = fmt::format(
                    "Clicks: {}/{}\nA:{}  B:{}  C:{}  D:{}  E:{}",
                    hitActive, totalActive,
                    formatTier(ClickTier::A),
                    formatTier(ClickTier::B),
                    formatTier(ClickTier::C),
                    formatTier(ClickTier::D),
                    formatTier(ClickTier::E)
                );
            } else {
                text = "Clicks: (No replay loaded)";
            }

            m_fields->m_hudLabel->setString(text.c_str());
            m_fields->m_hudLabel->setVisible(true);
        } else if (m_fields->m_hudLabel) {
            m_fields->m_hudLabel->setVisible(false);
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        ClickShowcase::get().reset();
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        PlayLayer::destroyPlayer(p0, p1);
        ClickShowcase::get().onPlayerDeath();
    }
};

class $modify(MyPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);

        auto pl = PlayLayer::get();
        if (pl && !pl->m_isPaused && this == pl->m_player1) {
            ClickShowcase::get().stepPhysicsFrame(pl);
        }
    }
};

class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = this->getChildByID("right-button-menu");
        if (!menu) return;

        // Clean small circular button matching pause menu styling
        auto spr = CircleButtonSprite::create(
            CCSprite::createWithSpriteFrameName("GJ_replayBtn_001.png"),
            CircleBaseColor::Green,
            CircleBaseSize::Small
        );
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(MyPauseLayer::onOpenShowcase)
        );
        btn->setID("click-showcase-btn"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onOpenShowcase(CCObject*) {
        auto popup = ShowcasePopup::create();
        if (popup) {
            popup->show();
        }
    }
};
