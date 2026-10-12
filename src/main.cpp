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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace geode::prelude;

#include <config.hpp>

inline bool isBotRunning() {
    if (ClickShowcase::get().m_isPreviewing) {
        return true;
    }

    if (!ClickShowcase::get().m_botOnly) {
        return true;
    }

    if (Loader::get()->isModLoaded("eclipse.eclipse-menu")) {
        int state = eclipse::config::get<int>("bot.state", 0);
        return (state == 2); // 2 = Playback
    }

    return !ClickShowcase::get().m_clicks.empty();
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

        // Hotkey edge-detection states for keys 1-5, 0
        bool m_key1Prev = false;
        bool m_key2Prev = false;
        bool m_key3Prev = false;
        bool m_key4Prev = false;
        bool m_key5Prev = false;
        bool m_key0Prev = false;

        // Recording indicator and hotkey
        Ref<CCNode> m_recIndicator = nullptr;
        bool m_recComboPrev = false;
        bool m_keyPPrev = false;
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
        m_fields->m_key1Prev = false;
        m_fields->m_key2Prev = false;
        m_fields->m_key3Prev = false;
        m_fields->m_key4Prev = false;
        m_fields->m_key5Prev = false;
        m_fields->m_key0Prev = false;
        m_fields->m_recComboPrev = false;
        m_fields->m_keyPPrev = false;
        m_fields->m_recIndicator = nullptr;

        ClickShowcase::get().reset();

        this->rebuildHUD();
        this->syncAllAssignedMarkers();
        this->schedule(schedule_selector(MyPlayLayer::onMyTick));

        return true;
    }

    void rebuildHUD() {
        if (m_fields->m_hudNode) {
            m_fields->m_hudNode->removeFromParent();
            m_fields->m_hudNode = nullptr;
        }
        m_fields->m_countLabels.clear();

        if (!ClickShowcase::get().m_showHUD) return;

        m_fields->m_hudNode = CCNode::create();
        m_fields->m_hudNode->setPosition({ 8.f, 125.f });
        m_fields->m_hudNode->setID("frame-window-hud"_spr);

        if (this->m_uiLayer) this->m_uiLayer->addChild(m_fields->m_hudNode, 9999);
        else this->addChild(m_fields->m_hudNode, 9999);

        struct HudItem {
            int id;
            std::string name;
            ccColor4F color;
        };
        // Presets: CLICKS (top), A, B, C, D, E (bottom-left)
        std::vector<HudItem> items = {
            { 0, "CLICKS", { 1.0f, 1.0f, 1.0f, 1.0f } },
            { 1, "A",      { 1.0f, 0.20f, 0.20f, 1.0f } },
            { 2, "B",      { 1.0f, 0.55f, 0.00f, 1.0f } },
            { 3, "C",      { 1.0f, 0.85f, 0.00f, 1.0f } },
            { 4, "D",      { 0.2f, 0.95f, 0.20f, 1.0f } },
            { 5, "E",      { 0.0f, 0.75f, 1.00f, 1.0f } }
        };

        float maxTextWidth = 25.f;
        std::vector<std::pair<int, std::pair<CCLabelBMFont*, CCLabelBMFont*>>> createdLabels;

        for (auto& item : items) {
            int count = m_fields->m_hudCounts.contains(item.id) ? m_fields->m_hudCounts[item.id] : 0;
            auto textLabel = CCLabelBMFont::create((item.name + ":").c_str(), "bigFont.fnt");
            textLabel->setAnchorPoint({ 0.f, 1.f });
            textLabel->setScale(0.45f);
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
            countLabel->setScale(0.45f);
            countLabel->setColor({
                static_cast<GLubyte>(item.color.r * 255),
                static_cast<GLubyte>(item.color.g * 255),
                static_cast<GLubyte>(item.color.b * 255)
            });
            countLabel->setOpacity(static_cast<GLubyte>(item.color.a * 255));

            createdLabels.push_back({ item.id, { textLabel, countLabel } });
        }

        float currentY = 0.f;
        for (auto& [id, pair] : createdLabels) {
            pair.first->setPosition({ 0.f, currentY });
            m_fields->m_hudNode->addChild(pair.first);

            pair.second->setPosition({ maxTextWidth + 4.f, currentY });
            m_fields->m_hudNode->addChild(pair.second);

            m_fields->m_countLabels[id] = pair.second;
            currentY -= 16.f;
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
        constexpr float BASE_SCALE = 0.45f;
        constexpr float PEAK_SCALE = 0.58f;

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

        if (this->m_objectLayer) {
            markerNode->setPosition(pos);
            this->m_objectLayer->addChild(markerNode, 100);
        } else if (this->m_uiLayer) {
            markerNode->setPosition(pos);
            this->m_uiLayer->addChild(markerNode);
        } else {
            markerNode->setPosition(pos);
            this->addChild(markerNode, 9999);
        }

        m_fields->m_activeMarkers.push_back({ markerNode, pos, clickIndex });
    }

    void syncAllAssignedMarkers() {
        if (!this->m_objectLayer) return;
        if (!ClickShowcase::get().m_showMarkers) return;

        for (const auto& click : ClickShowcase::get().m_clicks) {
            if (click.tier != ClickTier::None) {
                if (click.position.x != 0.f || click.position.y != 0.f) {
                    bool alreadyExists = false;
                    for (auto& am : m_fields->m_activeMarkers) {
                        if (am.clickIndex == click.clickIndex && am.node && am.node->getParent()) {
                            alreadyExists = true;
                            break;
                        }
                    }
                    if (!alreadyExists) {
                        this->spawnFrameWindowMarker(click.position, getTierColor(click.tier), click.clickIndex, !click.isHold);
                    }
                }
            } else {
                this->removeMarkerByClickIndex(click.clickIndex);
            }
        }
    }

    void updateAndCleanMarkers() {
        for (auto it = m_fields->m_activeMarkers.begin(); it != m_fields->m_activeMarkers.end(); ) {
            if (!it->node || !it->node->getParent()) {
                it = m_fields->m_activeMarkers.erase(it);
            } else {
                ++it;
            }
        }
    }

    void handleHotkeyAssign(ClickTier targetTier) {
        if (!this->m_player1 || this->m_player1->m_isDead || this->m_isPaused) return;

        CCPoint playerPos = this->m_player1->getPosition();
        int currFrame = m_fields->m_currentFrame;
        float currPercent = this->getCurrentPercent();

        auto& clicks = ClickShowcase::get().m_clicks;
        ClickMarkerInfo* targetClick = nullptr;

        // Search window: look for the click closest to currentFrame within reaction time (60 frames)
        int bestDiff = 999999;
        for (auto& c : clicks) {
            int diff = std::abs(c.frame - currFrame);
            if (diff < bestDiff && diff <= 60) {
                bestDiff = diff;
                targetClick = &c;
            }
        }

        // Fallback: check last action index if triggered within 90 frames
        if (!targetClick && ClickShowcase::get().m_lastActionIndex > 0) {
            for (auto& c : clicks) {
                if (c.clickIndex == ClickShowcase::get().m_lastActionIndex) {
                    if (std::abs(c.frame - currFrame) <= 90) {
                        targetClick = &c;
                    }
                    break;
                }
            }
        }

        if (targetClick) {
            targetClick->tier = targetTier;
            targetClick->position = playerPos;
            ClickShowcase::get().m_manualClickTiers[targetClick->clickIndex] = targetTier;

            if (targetTier != ClickTier::None) {
                this->spawnFrameWindowMarker(playerPos, getTierColor(targetTier), targetClick->clickIndex, !targetClick->isHold);
            } else {
                this->removeMarkerByClickIndex(targetClick->clickIndex);
            }
        } else {
            // No existing replay click found nearby: create a new click marker at this exact position and frame!
            ClickMarkerInfo newClick;
            newClick.frame = currFrame;
            newClick.percent = currPercent;
            newClick.position = playerPos;
            newClick.tier = targetTier;
            newClick.isHold = true;
            newClick.isPlayer2 = false;
            newClick.clickIndex = static_cast<int>(clicks.size()) + 1;
            clicks.push_back(newClick);

            std::sort(clicks.begin(), clicks.end(), [](const ClickMarkerInfo& a, const ClickMarkerInfo& b) {
                return a.frame < b.frame;
            });

            for (size_t i = 0; i < clicks.size(); i++) {
                clicks[i].clickIndex = static_cast<int>(i + 1);
                if (clicks[i].frame == currFrame) {
                    targetClick = &clicks[i];
                }
            }

            if (targetClick && targetTier != ClickTier::None) {
                ClickShowcase::get().m_manualClickTiers[targetClick->clickIndex] = targetTier;
                this->spawnFrameWindowMarker(playerPos, getTierColor(targetTier), targetClick->clickIndex, false);
            }
        }

        if (targetTier != ClickTier::None) {
            int tid = getTierId(targetTier);
            m_fields->m_hudCounts[tid]++;
            m_fields->m_hudCounts[0]++;
            this->updateHUDCounts();
            this->playHUDCountAnimation(tid);
            this->playHUDCountAnimation(0);

            std::string sfx = getTierSound(targetTier);
            if (!sfx.empty()) {
                auto resPath = geode::Mod::get()->getResourcesDir() / sfx;
                if (std::filesystem::exists(resPath)) {
                    FMODAudioEngine::sharedEngine()->playEffect(resPath.string().c_str(), 1.0f, 0.0f, 1.0f);
                } else {
                    FMODAudioEngine::sharedEngine()->playEffect(sfx.c_str(), 1.0f, 0.0f, 1.0f);
                }
            }

            char tChar = static_cast<char>(targetTier);
            int cIdx = targetClick ? targetClick->clickIndex : static_cast<int>(clicks.size());
            Notification::create(
                fmt::format("Click #{} assigned to Tier {}", cIdx, tChar),
                NotificationIcon::Success
            )->show();
        } else {
            this->updateHUDCounts();
            Notification::create("Marker cleared", NotificationIcon::Info)->show();
        }

        // Auto-save immediately to fwc_recordings/
        std::string lvl = (this->m_level && !this->m_level->m_levelName.empty()) ? this->m_level->m_levelName : "level";
        auto dir = ClickShowcase::get().getRecordingsDir();
        ClickShowcase::get().exportClicksJson(dir / fmt::format("showcase_{}.json", lvl));
    }

    void onRecordPlayerInput(bool isHold, bool isPlayer2) {
        if (!ClickShowcase::get().m_isRecording) return;
        if (!this->m_player1 || this->m_player1->m_isDead || this->m_isPaused) return;

        CCPoint playerPos = (isPlayer2 && this->m_player2) ? this->m_player2->getPosition() : this->m_player1->getPosition();
        int currFrame = m_fields->m_currentFrame;
        float currPercent = this->getCurrentPercent();

        ClickMarkerInfo info;
        info.clickIndex = static_cast<int>(ClickShowcase::get().m_recordedClicks.size()) + 1;
        info.frame = currFrame;
        info.percent = currPercent;
        info.position = playerPos;
        info.tier = ClickTier::None;
        info.isHold = isHold;
        info.isPlayer2 = isPlayer2;
        info.hasTriggered = false;

        ClickShowcase::get().m_recordedClicks.push_back(info);
    }

    void updateRecIndicator(bool visible) {
        if (!visible) {
            if (m_fields->m_recIndicator) {
                m_fields->m_recIndicator->setVisible(false);
            }
            return;
        }

        if (!m_fields->m_recIndicator) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            auto node = CCNode::create();
            node->setID("rec-indicator"_spr);
            node->setPosition({ winSize.width - 55.f, winSize.height - 18.f });

            auto dot = CCDrawNode::create();
            CCPoint verts[32];
            for (int i = 0; i < 32; i++) {
                float a = i * (3.14159f * 2.f) / 32.f;
                verts[i] = CCPoint{ 5.f * cosf(a), 5.f * sinf(a) };
            }
            dot->drawPolygon(verts, 32, { 1.0f, 0.15f, 0.15f, 1.0f }, 0.f, { 0, 0, 0, 0 });
            dot->setPosition({ 0.f, 0.f });
            node->addChild(dot);

            auto fadeOut = CCFadeTo::create(0.45f, 70);
            auto fadeIn = CCFadeTo::create(0.45f, 255);
            auto pulse = CCRepeatForever::create(CCSequence::create(fadeOut, fadeIn, nullptr));
            dot->runAction(pulse);

            auto lbl = CCLabelBMFont::create("REC", "bigFont.fnt");
            lbl->setScale(0.36f);
            lbl->setColor({ 255, 60, 60 });
            lbl->setAnchorPoint({ 0.f, 0.5f });
            lbl->setPosition({ 9.f, 0.f });
            node->addChild(lbl);

            if (this->m_uiLayer) this->m_uiLayer->addChild(node, 9999);
            else this->addChild(node, 9999);

            m_fields->m_recIndicator = node;
        }

        m_fields->m_recIndicator->setVisible(true);
    }

    void toggleRecording() {
        bool newState = !ClickShowcase::get().m_isRecording;
        ClickShowcase::get().m_isRecording = newState;

        if (newState) {
            ClickShowcase::get().m_recordedClicks.clear();
            this->updateRecIndicator(true);
            Notification::create("Recording Started (Ctrl+Shift+J to stop)", NotificationIcon::Info)->show();
        } else {
            this->updateRecIndicator(false);

            std::string levelName = "level";
            if (this->m_level) {
                levelName = this->m_level->m_levelName;
            }
            std::string savedPath = ClickShowcase::get().saveCurrentRecording(levelName);
            size_t count = ClickShowcase::get().m_clicks.size();

            if (!savedPath.empty()) {
                Notification::create(
                    fmt::format("Recording saved! ({} inputs in fwc_recordings/)", count),
                    NotificationIcon::Success
                )->show();
            } else {
                Notification::create("Recording stopped (0 inputs recorded)", NotificationIcon::Warning)->show();
            }

            this->rebuildHUD();
            this->syncAllAssignedMarkers();
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
                if (m_fields->m_hudNode) {
                    m_fields->m_hudNode->setVisible(ClickShowcase::get().m_showHUD);
                } else if (ClickShowcase::get().m_showHUD) {
                    this->rebuildHUD();
                }

                this->updateRecIndicator(ClickShowcase::get().m_isRecording);

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

                // ── Keyboard Hotkeys: 1->A, 2->B, 3->C, 4->D, 5->E, 0->None, Ctrl+Shift+J->Record ──
                #ifdef _WIN32
                HWND fgWnd = GetForegroundWindow();
                DWORD procId = 0;
                GetWindowThreadProcessId(fgWnd, &procId);
                if (procId == GetCurrentProcessId()) {
                    if (!this->m_isPaused) {
                        bool isCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                        bool isShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                        bool isJ = (GetAsyncKeyState('J') & 0x8000) != 0;
                        bool recCombo = isCtrl && isShift && isJ;

                        if (recCombo && !m_fields->m_recComboPrev) {
                            this->toggleRecording();
                        }
                        m_fields->m_recComboPrev = recCombo;

                        bool isP = (GetAsyncKeyState('P') & 0x8000) != 0;
                        bool previewCombo = isCtrl && isP && !isShift;
                        if (previewCombo && !m_fields->m_keyPPrev) {
                            bool nextPreview = !ClickShowcase::get().m_isPreviewing;
                            ClickShowcase::get().m_isPreviewing = nextPreview;
                            Notification::create(
                                nextPreview ? "Preview Mode ON" : "Preview Mode OFF",
                                NotificationIcon::Info
                            )->show();
                        }
                        m_fields->m_keyPPrev = previewCombo;

                        if (!isCtrl && !isShift) {
                            bool k1 = (GetAsyncKeyState('1') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0;
                            bool k2 = (GetAsyncKeyState('2') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD2) & 0x8000) != 0;
                            bool k3 = (GetAsyncKeyState('3') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) != 0;
                            bool k4 = (GetAsyncKeyState('4') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD4) & 0x8000) != 0;
                            bool k5 = (GetAsyncKeyState('5') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD5) & 0x8000) != 0;
                            bool k0 = (GetAsyncKeyState('0') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD0) & 0x8000) != 0 || (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;

                            if (k1 && !m_fields->m_key1Prev) this->handleHotkeyAssign(ClickTier::A);
                            if (k2 && !m_fields->m_key2Prev) this->handleHotkeyAssign(ClickTier::B);
                            if (k3 && !m_fields->m_key3Prev) this->handleHotkeyAssign(ClickTier::C);
                            if (k4 && !m_fields->m_key4Prev) this->handleHotkeyAssign(ClickTier::D);
                            if (k5 && !m_fields->m_key5Prev) this->handleHotkeyAssign(ClickTier::E);
                            if (k0 && !m_fields->m_key0Prev) this->handleHotkeyAssign(ClickTier::None);

                            m_fields->m_key1Prev = k1;
                            m_fields->m_key2Prev = k2;
                            m_fields->m_key3Prev = k3;
                            m_fields->m_key4Prev = k4;
                            m_fields->m_key5Prev = k5;
                            m_fields->m_key0Prev = k0;
                        }
                    }
                }
                #endif
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

        // If preview mode is active, simulate player inputs hands-free
        if (ClickShowcase::get().m_isPreviewing) {
            for (const auto& click : ClickShowcase::get().m_clicks) {
                if (click.frame == currentFrame) {
                    auto player = (click.isPlayer2 && this->m_player2) ? this->m_player2 : this->m_player1;
                    if (player) {
                        if (click.isHold) {
                            player->pushButton(PlayerButton::Jump);
                        } else {
                            player->releaseButton(PlayerButton::Jump);
                        }
                    }
                }
            }
        }

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

                    // Update live click / release tracking
                    if (click.isHold) {
                        ClickShowcase::get().m_livePressCount++;
                    } else {
                        ClickShowcase::get().m_liveRelCount++;
                    }
                    ClickShowcase::get().m_lastActionIndex = click.clickIndex;

                    // Capture player position at this exact trigger frame
                    CCPoint pPos = (click.isPlayer2 && this->m_player2) ? this->m_player2->getPosition() : this->m_player1->getPosition();
                    click.position = pPos;

                    if (click.tier != ClickTier::None) {
                        int tid = getTierId(click.tier);
                        m_fields->m_hudCounts[tid]++;
                        m_fields->m_hudCounts[0]++; // CLICKS
                        needsHudUpdate = true;
                        updatedTiers.push_back(tid);
                        updatedTiers.push_back(0);

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
        this->syncAllAssignedMarkers();
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
            ClickShowcase::get().reset();
            this->updateHUDCounts();
            this->syncAllAssignedMarkers();
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
        ClickShowcase::get().m_isPreviewing = false;
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

    void pushButton(PlayerButton p0) {
        PlayerObject::pushButton(p0);
        auto pl = PlayLayer::get();
        if (pl && !pl->m_isPaused && ClickShowcase::get().m_isRecording) {
            bool isP2 = (this == pl->m_player2);
            static_cast<MyPlayLayer*>(pl)->onRecordPlayerInput(true, isP2);
        }
    }

    void releaseButton(PlayerButton p0) {
        PlayerObject::releaseButton(p0);
        auto pl = PlayLayer::get();
        if (pl && !pl->m_isPaused && ClickShowcase::get().m_isRecording) {
            bool isP2 = (this == pl->m_player2);
            static_cast<MyPlayLayer*>(pl)->onRecordPlayerInput(false, isP2);
        }
    }
};

class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        ClickShowcase::get().m_isPreviewing = false;

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
        auto myPl = static_cast<MyPlayLayer*>(pl);
        myPl->rebuildHUD();
        myPl->syncAllAssignedMarkers();
    }
}

void togglePlayLayerRecording() {
    if (auto pl = PlayLayer::get()) {
        static_cast<MyPlayLayer*>(pl)->toggleRecording();
    } else {
        bool newState = !ClickShowcase::get().m_isRecording;
        ClickShowcase::get().m_isRecording = newState;
        if (!newState) {
            ClickShowcase::get().saveCurrentRecording("recording");
        }
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
