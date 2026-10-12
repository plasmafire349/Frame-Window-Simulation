#include "ClickShowcase.hpp"
#include "ReplayReader.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>

using namespace geode::prelude;

void ClickShowcase::reset() {
    m_lastTriggeredFrame = -1;
    m_currentClicksHit = 0;
    m_tierCounts.clear();
    for (auto& click : m_clicks) {
        click.hasTriggered = false;
    }
}

void ClickShowcase::applyAssignments() {
    // 1. Apply range rules first
    for (auto& click : m_clicks) {
        for (const auto& [minP, maxP, t] : m_percentRules) {
            if (click.percent >= minP && click.percent <= maxP) {
                click.tier = t;
            }
        }
    }

    // 2. Apply exact click overrides
    for (auto& click : m_clicks) {
        if (m_manualClickTiers.contains(click.clickIndex)) {
            click.tier = m_manualClickTiers[click.clickIndex];
        }
    }
}

bool ClickShowcase::assignByClick(int clickIndex, ClickTier tier) {
    bool found = false;
    for (auto& click : m_clicks) {
        if (click.clickIndex == clickIndex) {
            click.tier = tier;
            found = true;
            break;
        }
    }
    m_manualClickTiers[clickIndex] = tier;
    return found;
}

bool ClickShowcase::assignByPercent(float percent, ClickTier tier, float tolerance) {
    if (m_clicks.empty()) return false;

    // Find the closest click to the requested percent
    ClickMarkerInfo* closest = nullptr;
    float minDiff = 999999.f;

    for (auto& click : m_clicks) {
        float diff = std::abs(click.percent - percent);
        if (diff < minDiff) {
            minDiff = diff;
            closest = &click;
        }
    }

    if (closest && minDiff <= tolerance) {
        closest->tier = tier;
        m_manualClickTiers[closest->clickIndex] = tier;
        return true;
    } else if (closest) {
        // Fallback: assign closest anyway
        closest->tier = tier;
        m_manualClickTiers[closest->clickIndex] = tier;
        return true;
    }

    return false;
}

int ClickShowcase::assignByPercentRange(float minPercent, float maxPercent, ClickTier tier) {
    int count = 0;
    for (auto& click : m_clicks) {
        if (click.percent >= minPercent && click.percent <= maxPercent) {
            click.tier = tier;
            m_manualClickTiers[click.clickIndex] = tier;
            count++;
        }
    }
    m_percentRules.push_back({ minPercent, maxPercent, tier });
    return count;
}

void ClickShowcase::loadReplayFile(const std::filesystem::path& path, float levelLength) {
    m_clicks.clear();
    reset();

    ReplayFileResult res = parseReplay(path);
    if (res.actions.empty()) {
        log::error("No replay actions found in {}", path.string());
        return;
    }

    m_fps = res.fps;

    std::vector<RawReplayAction> validActions;
    for (const auto& act : res.actions) {
        if (act.frame > 5) {
            validActions.push_back(act);
        }
    }

    if (validActions.empty()) return;

    int totalFrames = validActions.back().frame;
    if (totalFrames <= 0) totalFrames = 1;

    int index = 1;
    for (size_t i = 0; i < validActions.size(); i++) {
        const auto& act = validActions[i];
        ClickMarkerInfo info;
        info.frame = act.frame;
        info.clickIndex = index++;
        info.isPlayer2 = act.isPlayer2;
        info.percent = std::clamp((static_cast<float>(act.frame) / static_cast<float>(totalFrames)) * 100.0f, 0.0f, 100.0f);

        // Baseline tier by delta
        int prevFrame = (i > 0) ? validActions[i - 1].frame : 0;
        int delta = act.frame - prevFrame;

        if (delta < 15) {
            info.tier = ClickTier::A;
        } else if (delta < 30) {
            info.tier = ClickTier::B;
        } else if (delta < 60) {
            info.tier = ClickTier::C;
        } else if (delta < 100) {
            info.tier = ClickTier::D;
        } else {
            info.tier = ClickTier::E;
        }

        m_clicks.push_back(info);
    }

    // Apply any manual click and percent rules
    applyAssignments();

    log::info("Loaded {} clicks for Click Showcase", m_clicks.size());
}

cocos2d::CCNode* ClickShowcase::createMarker(const ClickMarkerInfo& info) {
    auto markerNode = CCNode::create();
    markerNode->setID("click-showcase-marker"_spr);

    auto circle = CCDrawNode::create();
    CCPoint verts[64];
    float radius = 13.0f;
    for (int i = 0; i < 64; i++) {
        float angle = i * (3.14159265f * 2.0f) / 64.0f;
        verts[i] = CCPoint{ radius * cosf(angle), radius * sinf(angle) };
    }

    ccColor4F color = getTierColor(info.tier);

    // Double ring visual style
    circle->drawPolygon(verts, 64, { 0.f, 0.f, 0.f, 0.f }, 4.0f, { 0.f, 0.f, 0.f, color.a });
    circle->drawPolygon(verts, 64, { 0.f, 0.f, 0.f, 0.f }, 2.0f, color);
    markerNode->addChild(circle, 0);

    // Label with Tier, Click Index, and Percent
    std::string text = fmt::format("{} #{} ({:.1f}%)", static_cast<char>(info.tier), info.clickIndex, info.percent);
    auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    label->setAnchorPoint({ 1.0f, 0.5f });
    label->setPosition({ -18.0f, 0.0f });
    label->setScale(0.5f);
    label->setColor({
        static_cast<GLubyte>(color.r * 255),
        static_cast<GLubyte>(color.g * 255),
        static_cast<GLubyte>(color.b * 255)
    });
    label->setOpacity(static_cast<GLubyte>(color.a * 255));
    markerNode->addChild(label, 1);

    return markerNode;
}

void ClickShowcase::onPhysicsStep(PlayLayer* layer) {
    if (!layer || m_clicks.empty()) return;
    if (layer->m_isPaused) return;

    auto player = layer->m_player1;
    if (!player || player->m_isDead) return;

    int currentFrame = static_cast<int>(layer->m_gameState.m_currentProgress * (m_fps / 60.0f));
    if (currentFrame <= 0) {
        float progress = layer->m_player1->getPositionX() / std::max(1.0f, layer->m_levelLength);
        currentFrame = static_cast<int>(progress * (m_clicks.back().frame));
    }

    for (auto& click : m_clicks) {
        if (!click.hasTriggered && currentFrame >= click.frame) {
            click.hasTriggered = true;
            m_currentClicksHit = click.clickIndex;
            m_tierCounts[click.tier]++;

            if (m_showMarkers) {
                CCPoint spawnPos = player->getPosition();
                if (click.isPlayer2 && layer->m_player2) {
                    spawnPos = layer->m_player2->getPosition();
                }

                auto marker = createMarker(click);
                if (layer->m_objectLayer) {
                    marker->setPosition(layer->m_objectLayer->convertToWorldSpace(spawnPos));
                    marker->setScale(std::abs(layer->m_objectLayer->getScaleY()));
                } else {
                    marker->setPosition(spawnPos);
                }

                if (layer->m_uiLayer) {
                    layer->m_uiLayer->addChild(marker);
                } else {
                    layer->addChild(marker, 9999);
                }
            }

            if (m_playAudio) {
                std::string sfx = getTierSound(click.tier);
                FMODAudioEngine::sharedEngine()->playEffect(sfx.c_str(), 1.0f, 0.0f, 1.0f);
            }
        }
    }
}

void ClickShowcase::onPlayerDeath() {
    reset();
}

void ClickShowcase::exportClicksJson(const std::filesystem::path& path) {
    std::vector<matjson::Value> arr;
    for (const auto& click : m_clicks) {
        matjson::Value item;
        item["click"] = click.clickIndex;
        item["frame"] = click.frame;
        item["percent"] = click.percent;
        std::string tierStr(1, static_cast<char>(click.tier));
        item["tier"] = tierStr;
        item["p2"] = click.isPlayer2;
        arr.push_back(item);
    }

    matjson::Value root = matjson::makeObject({
        { "clicks", matjson::Value(arr) }
    });

    std::ofstream f(path);
    if (f) {
        f << root.dump(matjson::TWO_SPACES);
        f.close();
        geode::Notification::create("Exported Clicks to JSON!", geode::NotificationIcon::Success)->show();
    }
}

void ClickShowcase::loadClicksJson(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    auto parseRes = matjson::parse(content);
    if (!parseRes) return;
    auto root = parseRes.unwrap();

    std::vector<matjson::Value> arr;
    if (root.contains("clicks") && root["clicks"].isArray()) {
        auto unwrapRes = root["clicks"].as<std::vector<matjson::Value>>();
        if (unwrapRes.isOk()) arr = unwrapRes.unwrap();
    } else if (root.isArray()) {
        auto unwrapRes = root.as<std::vector<matjson::Value>>();
        if (unwrapRes.isOk()) arr = unwrapRes.unwrap();
    }

    if (arr.empty()) return;

    m_clicks.clear();
    reset();

    for (const auto& item : arr) {
        ClickMarkerInfo info;
        info.clickIndex = item["click"].as<int>().unwrapOr(0);
        info.frame = item["frame"].as<int>().unwrapOr(0);
        info.percent = static_cast<float>(item["percent"].as<double>().unwrapOr(0.0));
        std::string tierStr = item["tier"].as<std::string>().unwrapOr("C");
        info.tier = !tierStr.empty() ? static_cast<ClickTier>(tierStr[0]) : ClickTier::C;
        info.isPlayer2 = item.contains("p2") ? item["p2"].as<bool>().unwrapOr(false) : false;
        m_clicks.push_back(info);
    }

    geode::Notification::create(fmt::format("Loaded {} clicks from JSON!", m_clicks.size()).c_str(), geode::NotificationIcon::Success)->show();
}
