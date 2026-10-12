#include "ShowcasePopup.hpp"
#include "ClickShowcase.hpp"
#include <Geode/utils/file.hpp>
#include <string>
#include <sstream>

using namespace geode::prelude;

ShowcasePopup* ShowcasePopup::create() {
    auto ret = new ShowcasePopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool ShowcasePopup::init() {
    if (!FLAlertLayer::init(150)) {
        return false;
    }

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    m_mainLayer = CCLayer::create();
    this->addChild(m_mainLayer);

    // Dialog background
    auto bg = CCScale9Sprite::create("GJ_square01.png");
    bg->setContentSize({ 340.f, 270.f });
    bg->setPosition(winSize / 2.f);
    m_mainLayer->addChild(bg);

    // Title
    auto title = CCLabelBMFont::create("Click Showcase", "goldFont.fnt");
    title->setScale(0.8f);
    title->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 110.f });
    m_mainLayer->addChild(title);

    // Info Label
    m_infoLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_infoLabel->setScale(0.4f);
    m_infoLabel->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 82.f });
    m_mainLayer->addChild(m_infoLabel);
    refreshInfo();

    auto menu = CCMenu::create();
    menu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(menu);

    // Close button (top right)
    auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSpr, this, menu_selector(ShowcasePopup::onClose)
    );
    closeBtn->setPosition({ -155.f, 120.f });
    menu->addChild(closeBtn);

    // ── Input Field for "Click # or %" ───────────────────────────────────────
    auto inputBg = CCScale9Sprite::create("square02_small.png");
    inputBg->setContentSize({ 150.f, 26.f });
    inputBg->setOpacity(120);
    inputBg->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 42.f });
    m_mainLayer->addChild(inputBg);

    m_inputField = CCTextInputNode::create(140.f, 22.f, "Click # or %", "bigFont.fnt");
    m_inputField->setAllowedChars("0123456789.-% ");
    m_inputField->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 42.f });
    m_inputField->setScale(0.7f);
    m_mainLayer->addChild(m_inputField);

    // ── Tier Assignment Buttons (A, B, C, D, E) ──────────────────────────────
    char tiers[] = { 'A', 'B', 'C', 'D', 'E' };
    float startX = -88.f;
    for (int i = 0; i < 5; i++) {
        char t = tiers[i];
        ClickTier cTier = static_cast<ClickTier>(t);
        ccColor4F cColor = getTierColor(cTier);

        auto btnSpr = ButtonSprite::create(std::string(1, t).c_str(), "bigFont.fnt", "GJ_button_01.png", 0.65f);
        btnSpr->setColor({
            static_cast<GLubyte>(cColor.r * 255),
            static_cast<GLubyte>(cColor.g * 255),
            static_cast<GLubyte>(cColor.b * 255)
        });

        auto tierBtn = CCMenuItemSpriteExtra::create(
            btnSpr, this, menu_selector(ShowcasePopup::onAssignTier)
        );
        tierBtn->setTag(static_cast<int>(t));
        tierBtn->setPosition({ startX + (i * 44.f), 10.f });
        menu->addChild(tierBtn);
    }

    // ── Toggles for Markers and Audio ────────────────────────────────────────
    auto togglerMarkers = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleMarkers), 0.65f
    );
    togglerMarkers->toggle(ClickShowcase::get().m_showMarkers);
    togglerMarkers->setPosition({ -60.f, -35.f });
    menu->addChild(togglerMarkers);

    auto lblMarkers = CCLabelBMFont::create("Markers", "goldFont.fnt");
    lblMarkers->setScale(0.5f);
    lblMarkers->setPosition({ winSize.width / 2.f - 60.f, winSize.height / 2.f - 55.f });
    m_mainLayer->addChild(lblMarkers);

    auto togglerAudio = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleAudio), 0.65f
    );
    togglerAudio->toggle(ClickShowcase::get().m_playAudio);
    togglerAudio->setPosition({ 60.f, -35.f });
    menu->addChild(togglerAudio);

    auto lblAudio = CCLabelBMFont::create("Sounds", "goldFont.fnt");
    lblAudio->setScale(0.5f);
    lblAudio->setPosition({ winSize.width / 2.f + 60.f, winSize.height / 2.f - 55.f });
    m_mainLayer->addChild(lblAudio);

    // ── Import and Export Buttons ────────────────────────────────────────────
    auto importSpr = ButtonSprite::create("Import", "goldFont.fnt", "GJ_button_01.png", 0.75f);
    auto importBtn = CCMenuItemSpriteExtra::create(
        importSpr, this, menu_selector(ShowcasePopup::onImport)
    );
    importBtn->setPosition({ -65.f, -95.f });
    menu->addChild(importBtn);

    auto exportSpr = ButtonSprite::create("Export", "goldFont.fnt", "GJ_button_01.png", 0.75f);
    auto exportBtn = CCMenuItemSpriteExtra::create(
        exportSpr, this, menu_selector(ShowcasePopup::onExport)
    );
    exportBtn->setPosition({ 65.f, -95.f });
    menu->addChild(exportBtn);

    this->setKeypadEnabled(true);
    this->setTouchEnabled(true);

    return true;
}

void ShowcasePopup::refreshInfo() {
    if (!m_infoLabel) return;
    size_t count = ClickShowcase::get().m_clicks.size();
    m_infoLabel->setString(fmt::format("Loaded Clicks: {}", count).c_str());
}

void ShowcasePopup::onAssignTier(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    char tierChar = static_cast<char>(node->getTag());
    ClickTier tier = static_cast<ClickTier>(tierChar);

    if (!m_inputField) return;
    std::string text = m_inputField->getString();
    // Trim whitespace
    text.erase(0, text.find_first_not_of(" \t\r\n"));
    text.erase(text.find_last_not_of(" \t\r\n") + 1);

    if (text.empty()) {
        auto alert = FLAlertLayer::create("Assign Tier", "Type a Click # (e.g. <cr>4</c>) or Percent (e.g. <cg>3.61%</c>)", "OK");
        alert->show();
        return;
    }

    // 1. Percentage assignment
    if (text.find('%') != std::string::npos) {
        std::string clean = text;
        clean.erase(std::remove(clean.begin(), clean.end(), '%'), clean.end());

        // Range: e.g. "10-25%"
        auto dashPos = clean.find('-');
        if (dashPos != std::string::npos) {
            try {
                float minP = std::stof(clean.substr(0, dashPos));
                float maxP = std::stof(clean.substr(dashPos + 1));
                int affected = ClickShowcase::get().assignByPercentRange(minP, maxP, tier);
                geode::Notification::create(
                    fmt::format("Assigned {} clicks in {:.1f}%-{:.1f}% to {}!", affected, minP, maxP, tierChar),
                    geode::NotificationIcon::Success
                )->show();
            } catch (...) {}
        } else {
            // Single percent: e.g. "3.61%"
            try {
                float p = std::stof(clean);
                ClickShowcase::get().assignByPercent(p, tier);
                geode::Notification::create(
                    fmt::format("Assigned click at {:.1f}% to {}!", p, tierChar),
                    geode::NotificationIcon::Success
                )->show();
            } catch (...) {}
        }
    }
    // 2. Click number assignment
    else {
        // Range: e.g. "1-10"
        auto dashPos = text.find('-');
        if (dashPos != std::string::npos) {
            try {
                int minC = std::stoi(text.substr(0, dashPos));
                int maxC = std::stoi(text.substr(dashPos + 1));
                int affected = 0;
                for (int c = minC; c <= maxC; c++) {
                    if (ClickShowcase::get().assignByClick(c, tier)) affected++;
                }
                geode::Notification::create(
                    fmt::format("Assigned {} clicks (#{}-#{}) to {}!", affected, minC, maxC, tierChar),
                    geode::NotificationIcon::Success
                )->show();
            } catch (...) {}
        } else {
            // Single click: e.g. "4"
            try {
                int c = std::stoi(text);
                bool ok = ClickShowcase::get().assignByClick(c, tier);
                if (ok) {
                    geode::Notification::create(
                        fmt::format("Assigned Click #{} to {}!", c, tierChar),
                        geode::NotificationIcon::Success
                    )->show();
                } else {
                    geode::Notification::create(
                        fmt::format("Click #{} recorded for tier {}!", c, tierChar),
                        geode::NotificationIcon::Info
                    )->show();
                }
            } catch (...) {}
        }
    }

    refreshInfo();
}

void ShowcasePopup::onClose(CCObject*) {
    this->removeFromParentAndCleanup(true);
}

void ShowcasePopup::onToggleMarkers(CCObject*) {
    ClickShowcase::get().m_showMarkers = !ClickShowcase::get().m_showMarkers;
}

void ShowcasePopup::onToggleAudio(CCObject*) {
    ClickShowcase::get().m_playAudio = !ClickShowcase::get().m_playAudio;
}

void ShowcasePopup::onImport(CCObject*) {
    file::FilePickOptions options;
    options.filters.push_back({
        "Replay / JSON Files (*.gdr2, *.gdr, *.json)",
        { "*.gdr2", "*.gdr", "*.json" }
    });

    async::spawn(file::pick(file::PickMode::OpenFile, options), [this](Result<std::optional<std::filesystem::path>> res) {
        if (res.isOk() && res.unwrap().has_value()) {
            auto path = res.unwrap().value();
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".json") {
                ClickShowcase::get().loadClicksJson(path);
            } else {
                float levelLen = 10000.f;
                if (auto pl = PlayLayer::get()) {
                    levelLen = pl->m_levelLength;
                }
                ClickShowcase::get().loadReplayFile(path, levelLen);
            }

            size_t count = ClickShowcase::get().m_clicks.size();
            auto alert = FLAlertLayer::create("Success", fmt::format("Loaded {} clicks!", count).c_str(), "OK");
            alert->show();

            this->refreshInfo();
        }
    });
}

void ShowcasePopup::onExport(CCObject*) {
    file::FilePickOptions options;
    options.defaultPath = "showcase_clicks.json";
    options.filters.push_back({
        "JSON Files (*.json)",
        { "*.json" }
    });

    async::spawn(file::pick(file::PickMode::SaveFile, options), [](Result<std::optional<std::filesystem::path>> res) {
        if (res.isOk() && res.unwrap().has_value()) {
            auto path = res.unwrap().value();
            ClickShowcase::get().exportClicksJson(path);
        }
    });
}
