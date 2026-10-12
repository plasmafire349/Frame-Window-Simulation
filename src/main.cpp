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
            size_t total = ClickShowcase::get().m_clicks.size();
            int current = ClickShowcase::get().m_currentClicksHit;
            auto& counts = ClickShowcase::get().m_tierCounts;

            std::string text;
            if (total > 0) {
                text = fmt::format(
                    "Clicks: {}/{}\nA:{}  B:{}  C:{}  D:{}  E:{}",
                    current, total,
                    counts[ClickTier::A], counts[ClickTier::B], counts[ClickTier::C],
                    counts[ClickTier::D], counts[ClickTier::E]
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

    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        PlayLayer::destroyPlayer(p0, p1);
        ClickShowcase::get().onPlayerDeath();
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
