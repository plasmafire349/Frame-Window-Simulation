#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include "ClickShowcase.hpp"
#include "ShowcasePopup.hpp"
#include <algorithm>
#include <vector>
#include <map>
#include <cmath>

using namespace geode::prelude;

namespace eclipse::__internal__ {
    constexpr size_t API_VERSION = 1;

    struct VTable {
        bool(*Config_getBool)(std::string_view, bool const&) = nullptr;
        int(*Config_getInt)(std::string_view, int const&) = nullptr;
        double(*Config_getDouble)(std::string_view, double const&) = nullptr;
        std::string(*Config_getString)(std::string_view, std::string const&) = nullptr;
        std::string_view(*Config_getStringView)(std::string_view, std::string_view const&) = nullptr;
        bool(*Config_getBoolInternal)(std::string_view, bool const&) = nullptr;
        int(*Config_getIntInternal)(std::string_view, int const&) = nullptr;
        double(*Config_getDoubleInternal)(std::string_view, double const&) = nullptr;
        std::string(*Config_getStringInternal)(std::string_view, std::string const&) = nullptr;
        std::string_view(*Config_getStringViewInternal)(std::string_view, std::string_view const&) = nullptr;
    };

    struct FetchVTableEvent : geode::Event<FetchVTableEvent, bool(VTable&, size_t)> {
        using Event::Event;
    };

    inline VTable const& getVTable() {
        static VTable vtable{};
        static bool initialized = false;
        if (!initialized) {
            initialized = FetchVTableEvent().send(vtable, API_VERSION);
        }
        return vtable;
    }
}

inline bool isBotRunning() {
    if (ClickShowcase::get().m_clicks.empty()) {
        return false;
    }

    if (!ClickShowcase::get().m_botOnly) {
        return true;
    }

    auto& vtable = eclipse::__internal__::getVTable();
    if (vtable.Config_getInt) {
        int state = vtable.Config_getInt("bot.state", 0);
        return (state == 2); // 2 = Playback
    }

    return true;
}

struct ActiveMarker {
    Ref<CCNode> node;
    CCPoint worldPos;
    int clickIndex = 0;
};

inline int getTierId(ClickTier tier) {
    switch (tier) {
        case ClickTier::A: return 1;
        case ClickTier::B: return 2;
        case ClickTier::C: return 3;
        case ClickTier::D: return 4;
        case ClickTier::E: return 5;
        default: return -1;
    }
}

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        int m_currentFrame = 0;
        int m_lastFrame = -1;
        std::map<void*, int> m_checkpointMap;
        std::vector<int> m_checkpointFrameStack;
        int m_pendingRestoreFrame = -1;
        bool m_restoreFrameArmed = false;
        bool m_checkpointRemovalActive = false;
        int m_checkpointRemovalRestoreFrame = -1;
        std::map<int, int> m_hudCounts; // 0=CLICKS, 1=A, 2=B, 3=C, 4=D, 5=E
        Ref<CCNode> m_hudNode = nullptr;
        std::vector<ActiveMarker> m_activeMarkers;
        std::map<int, Ref<CCLabelBMFont>> m_countLabels;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        m_fields->m_currentFrame = 0;
        m_fields->m_lastFrame = -1;
        m_fields->m_checkpointMap.clear();
        m_fields->m_checkpointFrameStack.clear();
        m_fields->m_pendingRestoreFrame = -1;
        m_fields->m_restoreFrameArmed = false;
        m_fields->m_checkpointRemovalActive = false;
        m_fields->m_checkpointRemovalRestoreFrame = -1;
        m_fields->m_hudCounts.clear();
        m_fields->m_activeMarkers.clear();
        m_fields->m_countLabels.clear();

        ClickShowcase::get().reset();

        this->rebuildHUD();
        this->schedule(schedule_selector(MyPlayLayer::onMyTick));

        return true;
    }

    void rebuildHUD() {
        if (m_fields->m_hudNode) {
            m_fields->m_hudNode->removeFromParent();
            m_fields->m_hudNode = nullptr;
        }
        m_fields->m_countLabels.clear();

        if (!ClickShowcase::get().m_showHUD || !isBotRunning()) return;

        m_fields->m_hudNode = CCNode::create();
        m_fields->m_hudNode->setPosition({ 5.f, CCDirector::get()->getWinSize().height - 5.f });
        m_fields->m_hudNode->setID("frame-window-hud"_spr);

        if (this->m_uiLayer) this->m_uiLayer->addChild(m_fields->m_hudNode, 9999);
        else this->addChild(m_fields->m_hudNode, 9999);

        struct HudItem {
            int id;
            std::string name;
            ccColor4F color;
        };
        // Presets: CLICKS (top), E, D, C, B, A (bottom) matching Frame Window Counter
        std::vector<HudItem> items = {
            { 0, "CLICKS", { 1.0f, 1.0f, 1.0f, 1.0f } },
            { 5, "E",      { 0.0f, 0.75f, 1.00f, 1.0f } },
            { 4, "D",      { 0.2f, 0.95f, 0.20f, 1.0f } },
            { 3, "C",      { 1.0f, 0.85f, 0.00f, 1.0f } },
            { 2, "B",      { 1.0f, 0.55f, 0.00f, 1.0f } },
            { 1, "A",      { 1.0f, 0.20f, 0.20f, 1.0f } }
        };

        float maxTextWidth = 25.f;
        std::vector<std::pair<int, std::pair<CCLabelBMFont*, CCLabelBMFont*>>> createdLabels;

        for (auto& item : items) {
            int count = m_fields->m_hudCounts.contains(item.id) ? m_fields->m_hudCounts[item.id] : 0;
            auto textLabel = CCLabelBMFont::create((item.name + ":").c_str(), "bigFont.fnt");
            textLabel->setAnchorPoint({ 0.f, 1.f });
            textLabel->setScale(0.5f);
            textLabel->setColor({
                static_cast<GLubyte>(item.color.r * 255),
                static_cast<GLubyte>(item.color.g * 255),
                static_cast<GLubyte>(item.color.b * 255)
            });
            textLabel->setOpacity(static_cast<GLubyte>(item.color.a * 255));

            float currentWidth = textLabel->getScaledContentSize().width;
            if (currentWidth > maxTextWidth) maxTextWidth = currentWidth;

            auto countLabel = CCLabelBMFont::create(std::to_string(count).c_str(), "bigFont.fnt");
            countLabel->setAnchorPoint({ 0.f, 1.f });
            countLabel->setScale(0.5f);
            countLabel->setColor({
                static_cast<GLubyte>(item.color.r * 255),
                static_cast<GLubyte>(item.color.g * 255),
                static_cast<GLubyte>(item.color.b * 255)
            });
            countLabel->setOpacity(static_cast<GLubyte>(item.color.a * 255));

            createdLabels.push_back({ item.id, { textLabel, countLabel } });
        }

        float currentY = -10.f;
        for (auto& [id, pair] : createdLabels) {
            pair.first->setPosition({ 0.f, currentY });
            m_fields->m_hudNode->addChild(pair.first);

            pair.second->setPosition({ maxTextWidth + 4.f, currentY });
            m_fields->m_hudNode->addChild(pair.second);

            m_fields->m_countLabels[id] = pair.second;
            currentY -= 18.f;
        }
    }

    void updateHUDCounts() {
        if (!m_fields->m_hudNode) {
            this->rebuildHUD();
            return;
        }

        for (auto& [id, countLbl] : m_fields->m_countLabels) {
            if (countLbl) {
                int count = m_fields->m_hudCounts.contains(id) ? m_fields->m_hudCounts[id] : 0;
                countLbl->setString(std::to_string(count).c_str());
            }
        }
    }

    void playHUDCountAnimation(int presetId) {
        auto it = m_fields->m_countLabels.find(presetId);
        if (it == m_fields->m_countLabels.end() || !it->second) return;

        auto countLbl = it->second;

        ccColor3B origColor = { 255, 255, 255 };
        switch (presetId) {
            case 1: origColor = { 255, 51, 51 }; break;   // A
            case 2: origColor = { 255, 140, 0 }; break;   // B
            case 3: origColor = { 255, 217, 0 }; break;   // C
            case 4: origColor = { 51, 242, 51 }; break;   // D
            case 5: origColor = { 0, 191, 255 }; break;   // E
            default: origColor = { 255, 255, 255 }; break;// CLICKS
        }

        constexpr int TAG_SCALE = 1001;
        constexpr int TAG_TINT = 1002;

        countLbl->stopActionByTag(TAG_SCALE);
        countLbl->stopActionByTag(TAG_TINT);

        constexpr float TIME_UP = 0.06f;
        constexpr float TIME_DOWN = 0.20f;
        constexpr float BASE_SCALE = 0.50f;
        constexpr float PEAK_SCALE = 0.60f;

        auto scaleUp = CCEaseSineOut::create(CCScaleTo::create(TIME_UP, PEAK_SCALE));
        auto scaleDown = CCEaseSineOut::create(CCScaleTo::create(TIME_DOWN, BASE_SCALE));
        auto scaleSeq = CCSequence::create(scaleUp, scaleDown, nullptr);
        scaleSeq->setTag(TAG_SCALE);
        countLbl->runAction(scaleSeq);

        auto tintToWhite = CCEaseSineOut::create(CCTintTo::create(TIME_UP, 255, 255, 255));
        auto tintToOrig = CCEaseSineOut::create(CCTintTo::create(TIME_DOWN, origColor.r, origColor.g, origColor.b));
        auto tintSeq = CCSequence::create(tintToWhite, tintToOrig, nullptr);
        tintSeq->setTag(TAG_TINT);
        countLbl->runAction(tintSeq);
    }

    void removeMarkerByClickIndex(int clickIndex) {
        for (auto it = m_fields->m_activeMarkers.begin(); it != m_fields->m_activeMarkers.end(); ) {
            if (it->clickIndex == clickIndex) {
                if (it->node) it->node->removeFromParent();
                it = m_fields->m_activeMarkers.erase(it);
            } else {
                ++it;
            }
        }
    }

    void spawnFrameWindowMarker(CCPoint pos, ccColor4F color, int clickIndex = 0, bool isRelease = false) {
        if (clickIndex > 0) {
            this->removeMarkerByClickIndex(clickIndex);
        }

        auto markerNode = CCNode::create();
        markerNode->setID("frame-window-marker"_spr);

        auto circle = CCDrawNode::create();
        CCPoint verts[64];
        float radius = 13.f;
        for (int i = 0; i < 64; i++) {
            float angle = i * (3.14159f * 2.f) / 64.f;
            verts[i] = CCPoint{ radius * cosf(angle), radius * sinf(angle) };
        }

        float a = color.a;
        float r = color.r * a;
        float g = color.g * a;
        float b = color.b * a;

        circle->drawPolygon(verts, 64, { 0.f, 0.f, 0.f, 0.f }, 4.f, { 0.f, 0.f, 0.f, color.a });
        circle->drawPolygon(verts, 64, { 0.f, 0.f, 0.f, 0.f }, 2.f, { r, g, b, a });

        // If release action: add center dot for clear visual distinction matching FWC
        if (isRelease) {
            CCPoint innerVerts[32];
            float innerRadius = 4.0f;
            for (int i = 0; i < 32; i++) {
                float angle = i * (3.14159f * 2.0f) / 32.0f;
                innerVerts[i] = CCPoint{ innerRadius * cosf(angle), innerRadius * sinf(angle) };
            }
            circle->drawPolygon(innerVerts, 32, { r, g, b, a }, 1.0f, { r, g, b, a });
        }

        markerNode->addChild(circle, 0);

        if (this->m_uiLayer) {
            this->m_uiLayer->addChild(markerNode);
        } else {
            this->addChild(markerNode, 9999);
        }

        if (this->m_objectLayer) {
            markerNode->setPosition(this->m_objectLayer->convertToWorldSpace(pos));
            markerNode->setScale(std::abs(this->m_objectLayer->getScaleY()));
        } else {
            markerNode->setPosition(pos);
        }

        m_fields->m_activeMarkers.push_back({ markerNode, pos, clickIndex });
    }

    void updateAndCleanMarkers() {
        if (!this->m_objectLayer) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        float layerScale = std::abs(this->m_objectLayer->getScaleY());

        constexpr float margin = 300.0f;
        float minX = -margin;
        float maxX = winSize.width + margin;
        float minY = -margin;
        float maxY = winSize.height + margin;

        // Update positions of existing markers and cull offscreen
        for (auto it = m_fields->m_activeMarkers.begin(); it != m_fields->m_activeMarkers.end(); ) {
            auto& marker = *it;
            if (!marker.node || !marker.node->getParent()) {
                it = m_fields->m_activeMarkers.erase(it);
                continue;
            }

            CCPoint screenPos = this->m_objectLayer->convertToWorldSpace(marker.worldPos);
            marker.node->setPosition(screenPos);
            marker.node->setScale(layerScale);

            if (screenPos.x < minX || screenPos.x > maxX || screenPos.y < minY || screenPos.y > maxY) {
                marker.node->removeFromParent();
                it = m_fields->m_activeMarkers.erase(it);
            } else {
                ++it;
            }
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        this->updateAndCleanMarkers();
    }

    void onMyTick(float dt) {
        auto gm = GameManager::sharedState();
        if (gm->getPlayLayer() && !gm->getPlayLayer()->m_isPaused) {
            if (this->m_player1 && !this->m_player1->m_isDead) {
                bool botActive = isBotRunning();
                if (m_fields->m_hudNode) {
                    m_fields->m_hudNode->setVisible(botActive && ClickShowcase::get().m_showHUD);
                } else if (botActive && ClickShowcase::get().m_showHUD) {
                    this->rebuildHUD();
                }

                if (!botActive) {
                    for (auto& marker : m_fields->m_activeMarkers) {
                        if (marker.node) marker.node->setVisible(false);
                    }
                    return;
                }

                for (auto& marker : m_fields->m_activeMarkers) {
                    if (marker.node) marker.node->setVisible(true);
                }

                if (m_fields->m_checkpointRemovalActive) {
                    armCheckpointRestore(m_fields->m_checkpointRemovalRestoreFrame);
                    applyRestoredFrame();
                    m_fields->m_checkpointRemovalActive = false;
                    m_fields->m_checkpointRemovalRestoreFrame = -1;
                } else if (m_fields->m_restoreFrameArmed) {
                    applyRestoredFrame();
                }

                this->updateAndCleanMarkers();
            }
        }
    }

    void onPhysicsStep() {
        if (!this->m_player1 || this->m_player1->m_isDead) return;
        if (!isBotRunning()) return;

        if (m_fields->m_restoreFrameArmed) {
            applyRestoredFrame();
        }

        m_fields->m_currentFrame++;
        int currentFrame = m_fields->m_currentFrame;

        // Auto-capture exact positions and percents for clicks matching currentFrame
        for (auto& click : ClickShowcase::get().m_clicks) {
            if (click.frame == currentFrame) {
                CCPoint playerPos = (click.isPlayer2 && this->m_player2) ? this->m_player2->getPosition() : this->m_player1->getPosition();
                click.position = playerPos;
            }
        }

        // Reset if rewinding or restarting
        if (currentFrame < m_fields->m_lastFrame) {
            m_fields->m_lastFrame = currentFrame - 1;
            m_fields->m_hudCounts.clear();
            for (auto& marker : m_fields->m_activeMarkers) {
                if (marker.node) marker.node->removeFromParent();
            }
            m_fields->m_activeMarkers.clear();

            for (auto& click : ClickShowcase::get().m_clicks) {
                if (click.tier != ClickTier::None && click.frame <= m_fields->m_lastFrame) {
                    click.hasTriggered = true;
                    int tid = getTierId(click.tier);
                    m_fields->m_hudCounts[tid]++;
                    m_fields->m_hudCounts[0]++;
                } else {
                    click.hasTriggered = false;
                }
            }
            this->updateHUDCounts();
        }

        if (currentFrame > m_fields->m_lastFrame) {
            bool skipAudio = (currentFrame - m_fields->m_lastFrame > 240);
            bool needsHudUpdate = false;
            std::vector<int> updatedTiers;

            auto& clicks = ClickShowcase::get().m_clicks;
            for (auto& click : clicks) {
                if (!click.hasTriggered && click.frame <= currentFrame && click.frame > m_fields->m_lastFrame) {
                    click.hasTriggered = true;

                    if (click.tier != ClickTier::None) {
                        int tid = getTierId(click.tier);
                        m_fields->m_hudCounts[tid]++;
                        m_fields->m_hudCounts[0]++; // CLICKS
                        needsHudUpdate = true;
                        updatedTiers.push_back(tid);
                        updatedTiers.push_back(0);

                        // Capture player position at this exact trigger frame
                        CCPoint pPos = (click.isPlayer2 && this->m_player2) ? this->m_player2->getPosition() : this->m_player1->getPosition();
                        click.position = pPos;

                        // Spawn marker at player position: circle on that position, nothing else
                        if (ClickShowcase::get().m_showMarkers) {
                            this->spawnFrameWindowMarker(click.position, getTierColor(click.tier), click.clickIndex, !click.isHold);
                        }

                        // Play sound
                        if (!skipAudio && ClickShowcase::get().m_playAudio) {
                            std::string sfx = getTierSound(click.tier);
                            if (!sfx.empty()) {
                                auto resPath = geode::Mod::get()->getResourcesDir() / sfx;
                                if (std::filesystem::exists(resPath)) {
                                    FMODAudioEngine::sharedEngine()->playEffect(resPath.string().c_str(), 1.0f, 0.0f, 1.0f);
                                } else {
                                    FMODAudioEngine::sharedEngine()->playEffect(sfx.c_str(), 1.0f, 0.0f, 1.0f);
                                }
                            }
                        }
                    }
                }
            }

            m_fields->m_lastFrame = currentFrame;

            if (needsHudUpdate) {
                this->updateHUDCounts();
                if (!skipAudio) {
                    std::sort(updatedTiers.begin(), updatedTiers.end());
                    updatedTiers.erase(std::unique(updatedTiers.begin(), updatedTiers.end()), updatedTiers.end());
                    for (int id : updatedTiers) {
                        this->playHUDCountAnimation(id);
                    }
                }
            }
        }
    }

    void rememberCheckpoint(CheckpointObject* cp) {
        if (!cp) return;
        auto key = static_cast<void*>(cp);
        if (m_fields->m_checkpointMap.contains(key)) return;
        const int frame = m_fields->m_currentFrame;
        m_fields->m_checkpointMap[key] = frame;
        m_fields->m_checkpointFrameStack.push_back(frame);
    }

    CheckpointObject* createCheckpoint() {
        auto cp = PlayLayer::createCheckpoint();
        if (cp) rememberCheckpoint(cp);
        return cp;
    }

    void removeCheckpoint(bool p0) {
        if (this->m_isPracticeMode && !m_fields->m_checkpointFrameStack.empty()) {
            const int beforeStackSize = static_cast<int>(m_fields->m_checkpointFrameStack.size());
            if (!p0 || beforeStackSize == 1) {
                m_fields->m_checkpointFrameStack.pop_back();
                if (!m_fields->m_checkpointFrameStack.empty()) {
                    m_fields->m_checkpointRemovalActive = true;
                    m_fields->m_checkpointRemovalRestoreFrame = m_fields->m_checkpointFrameStack.back();
                } else {
                    m_fields->m_checkpointRemovalActive = true;
                    m_fields->m_checkpointRemovalRestoreFrame = 0;
                }
            }
        }
        PlayLayer::removeCheckpoint(p0);
    }

    void armCheckpointRestore(int restoreFrame) {
        if (restoreFrame >= 0) {
            m_fields->m_pendingRestoreFrame = restoreFrame;
            m_fields->m_restoreFrameArmed = true;
        }
    }

    void applyRestoredFrame() {
        if (!m_fields->m_restoreFrameArmed) return;
        int restoreFrame = m_fields->m_pendingRestoreFrame;
        m_fields->m_pendingRestoreFrame = -1;
        m_fields->m_restoreFrameArmed = false;

        m_fields->m_currentFrame = restoreFrame;
        m_fields->m_lastFrame = restoreFrame - 1;

        m_fields->m_hudCounts.clear();
        for (auto& marker : m_fields->m_activeMarkers) {
            if (marker.node) marker.node->removeFromParent();
        }
        m_fields->m_activeMarkers.clear();

        for (auto& click : ClickShowcase::get().m_clicks) {
            if (click.tier != ClickTier::None && click.frame <= restoreFrame) {
                click.hasTriggered = true;
                int tid = getTierId(click.tier);
                m_fields->m_hudCounts[tid]++;
                m_fields->m_hudCounts[0]++;
            } else {
                click.hasTriggered = false;
            }
        }
        this->updateHUDCounts();
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        int restoreFrame = -1;
        if (this->m_isPracticeMode && !m_fields->m_checkpointFrameStack.empty()) {
            restoreFrame = m_fields->m_checkpointFrameStack.back();
        }

        if (restoreFrame >= 0) {
            armCheckpointRestore(restoreFrame);
        } else {
            m_fields->m_currentFrame = 0;
            m_fields->m_lastFrame = -1;
            m_fields->m_hudCounts.clear();
            for (auto& marker : m_fields->m_activeMarkers) {
                if (marker.node) marker.node->removeFromParent();
            }
            m_fields->m_activeMarkers.clear();
            ClickShowcase::get().reset();
            this->updateHUDCounts();
        }
    }

    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        PlayLayer::destroyPlayer(p0, p1);
        int restoreFrame = -1;
        if (this->m_isPracticeMode && !m_fields->m_checkpointFrameStack.empty()) {
            restoreFrame = m_fields->m_checkpointFrameStack.back();
        }
        if (restoreFrame >= 0) {
            armCheckpointRestore(restoreFrame);
        }
    }

    void onQuit() {
        this->unschedule(schedule_selector(MyPlayLayer::onMyTick));
        PlayLayer::onQuit();
    }
};

class $modify(MyPlayerObject, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);

        auto pl = PlayLayer::get();
        if (pl && !pl->m_isPaused && this == pl->m_player1) {
            static_cast<MyPlayLayer*>(pl)->onPhysicsStep();
        }
    }
};

class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto rightMenu = this->getChildByID("right-button-menu");
        if (rightMenu) {
            auto btn = CCMenuItemSpriteExtra::create(
                CCSprite::createWithSpriteFrameName("GJ_timeIcon_001.png"),
                this,
                menu_selector(MyPauseLayer::onOpenShowcase)
            );
            btn->setScale(0.8f);
            btn->setID("click-showcase-btn"_spr);
            rightMenu->addChild(btn);
            rightMenu->updateLayout();
        }
    }

    void onOpenShowcase(CCObject*) {
        auto popup = ShowcasePopup::create();
        if (popup) {
            popup->show();
        }
    }
};

int getCurrentPhysicsFrame() {
    if (auto pl = PlayLayer::get()) {
        return static_cast<MyPlayLayer*>(pl)->m_fields->m_currentFrame;
    }
    return 0;
}

void triggerPlayLayerHUDUpdate() {
    if (auto pl = PlayLayer::get()) {
        static_cast<MyPlayLayer*>(pl)->rebuildHUD();
        static_cast<MyPlayLayer*>(pl)->updateAndCleanMarkers();
    }
}

void spawnMarkerInPlayLayer(CCPoint pos, ccColor4F color, int clickIndex, bool isRelease) {
    if (auto pl = PlayLayer::get()) {
        static_cast<MyPlayLayer*>(pl)->spawnFrameWindowMarker(pos, color, clickIndex, isRelease);
    }
}

void removeMarkerInPlayLayer(int clickIndex) {
    if (auto pl = PlayLayer::get()) {
        static_cast<MyPlayLayer*>(pl)->removeMarkerByClickIndex(clickIndex);
    }
}
