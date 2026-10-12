#include "ShowcasePopup.hpp"
#include "ClickShowcase.hpp"
#include <Geode/utils/file.hpp>
#include <string>
#include <sstream>
#include <algorithm>

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

    // Dialog background (380 x 290)
    auto bg = CCScale9Sprite::create("GJ_square01.png");
    bg->setContentSize({ 380.f, 290.f });
    bg->setPosition(winSize / 2.f);
    m_mainLayer->addChild(bg);

    // Title
    auto title = CCLabelBMFont::create("Click Showcase", "goldFont.fnt");
    title->setScale(0.8f);
    title->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 124.f });
    m_mainLayer->addChild(title);

    // Main button menu
    auto mainMenu = CCMenu::create();
    mainMenu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(mainMenu);

    // Close button (top right)
    auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSpr, this, menu_selector(ShowcasePopup::onClose)
    );
    closeBtn->setPosition({ -175.f, 130.f });
    mainMenu->addChild(closeBtn);

    // ── Page Navigation Bar ──────────────────────────────────────────────────
    auto prevSpr = ButtonSprite::create("<", "bigFont.fnt", "GJ_button_01.png", 0.55f);
    auto prevBtn = CCMenuItemSpriteExtra::create(
        prevSpr, this, menu_selector(ShowcasePopup::onPrevPage)
    );
    prevBtn->setPosition({ -145.f, 96.f });
    mainMenu->addChild(prevBtn);

    m_pageLabel = CCLabelBMFont::create("Page 1 / 1 (0 inputs)", "bigFont.fnt");
    m_pageLabel->setScale(0.42f);
    m_pageLabel->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 96.f });
    m_mainLayer->addChild(m_pageLabel);

    auto nextSpr = ButtonSprite::create(">", "bigFont.fnt", "GJ_button_01.png", 0.55f);
    auto nextBtn = CCMenuItemSpriteExtra::create(
        nextSpr, this, menu_selector(ShowcasePopup::onNextPage)
    );
    nextBtn->setPosition({ 145.f, 96.f });
    mainMenu->addChild(nextBtn);

    // ── Table Layer and Table Menu (Centered at winSize / 2) ─────────────────
    m_tableLayer = CCLayer::create();
    m_tableLayer->setPosition(winSize / 2.f);
    m_mainLayer->addChild(m_tableLayer);

    m_tableMenu = CCMenu::create();
    m_tableMenu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(m_tableMenu);

    // ── Quick Assign Bar (y = -56.f) ─────────────────────────────────────────
    auto inputBg = CCScale9Sprite::create("square02_small.png");
    inputBg->setContentSize({ 85.f, 22.f });
    inputBg->setOpacity(120);
    inputBg->setPosition({ winSize.width / 2.f - 118.f, winSize.height / 2.f - 56.f });
    m_mainLayer->addChild(inputBg);

    m_rangeInput = CCTextInputNode::create(80.f, 18.f, "# or %", "bigFont.fnt");
    m_rangeInput->setAllowedChars("0123456789.-% ");
    m_rangeInput->setPosition({ winSize.width / 2.f - 118.f, winSize.height / 2.f - 56.f });
    m_rangeInput->setScale(0.6f);
    m_mainLayer->addChild(m_rangeInput);

    char tiers[] = { 'A', 'B', 'C', 'D', 'E', '-' };
    float tierStartX = -50.f;
    for (int i = 0; i < 6; i++) {
        char t = tiers[i];
        ClickTier cTier = static_cast<ClickTier>(t);
        ccColor4F cColor = getTierColor(cTier);

        std::string labelStr = (t == '-') ? "None" : std::string(1, t);
        auto btnSpr = ButtonSprite::create(labelStr.c_str(), "bigFont.fnt", "GJ_button_01.png", (t == '-') ? 0.45f : 0.55f);
        btnSpr->setColor({
            static_cast<GLubyte>(cColor.r * 255),
            static_cast<GLubyte>(cColor.g * 255),
            static_cast<GLubyte>(cColor.b * 255)
        });

        auto tierBtn = CCMenuItemSpriteExtra::create(
            btnSpr, this, menu_selector(ShowcasePopup::onQuickSetRange)
        );
        tierBtn->setTag(static_cast<int>(t));
        tierBtn->setPosition({ tierStartX + (i * 37.f), -56.f });
        mainMenu->addChild(tierBtn);
    }

    // ── Bottom Action Controls (y = -102.f) ──────────────────────────────────
    auto importSpr = ButtonSprite::create("Import", "goldFont.fnt", "GJ_button_01.png", 0.7f);
    auto importBtn = CCMenuItemSpriteExtra::create(
        importSpr, this, menu_selector(ShowcasePopup::onImport)
    );
    importBtn->setPosition({ -115.f, -102.f });
    mainMenu->addChild(importBtn);

    auto exportSpr = ButtonSprite::create("Export", "goldFont.fnt", "GJ_button_01.png", 0.7f);
    auto exportBtn = CCMenuItemSpriteExtra::create(
        exportSpr, this, menu_selector(ShowcasePopup::onExport)
    );
    exportBtn->setPosition({ -38.f, -102.f });
    mainMenu->addChild(exportBtn);

    auto togglerMarkers = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleMarkers), 0.60f
    );
    togglerMarkers->toggle(ClickShowcase::get().m_showMarkers);
    togglerMarkers->setPosition({ 45.f, -95.f });
    mainMenu->addChild(togglerMarkers);

    auto lblMarkers = CCLabelBMFont::create("Markers", "goldFont.fnt");
    lblMarkers->setScale(0.40f);
    lblMarkers->setPosition({ winSize.width / 2.f + 45.f, winSize.height / 2.f - 116.f });
    m_mainLayer->addChild(lblMarkers);

    auto togglerAudio = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleAudio), 0.60f
    );
    togglerAudio->toggle(ClickShowcase::get().m_playAudio);
    togglerAudio->setPosition({ 125.f, -95.f });
    mainMenu->addChild(togglerAudio);

    auto lblAudio = CCLabelBMFont::create("Sounds", "goldFont.fnt");
    lblAudio->setScale(0.40f);
    lblAudio->setPosition({ winSize.width / 2.f + 125.f, winSize.height / 2.f - 116.f });
    m_mainLayer->addChild(lblAudio);

    this->setKeypadEnabled(true);
    this->setTouchEnabled(true);

    updateTable();

    return true;
}

void ShowcasePopup::updateTable() {
    if (!m_tableLayer || !m_tableMenu) return;
    m_tableLayer->removeAllChildrenWithCleanup(true);
    m_tableMenu->removeAllChildrenWithCleanup(true);

    auto& clicks = ClickShowcase::get().m_clicks;
    size_t totalClicks = clicks.size();
    int totalPages = std::max(1, static_cast<int>((totalClicks + CLICKS_PER_PAGE - 1) / CLICKS_PER_PAGE));

    if (m_currentPage < 0) m_currentPage = 0;
    if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;

    if (m_pageLabel) {
        if (totalClicks == 0) {
            m_pageLabel->setString("No clicks loaded (0)");
        } else {
            m_pageLabel->setString(fmt::format("Page {} / {} ({} inputs)", m_currentPage + 1, totalPages, totalClicks).c_str());
        }
    }

    if (clicks.empty()) {
        auto emptyLabel = CCLabelBMFont::create("Tap 'Import' below to load a .gdr2 or .json replay", "bigFont.fnt");
        emptyLabel->setScale(0.42f);
        emptyLabel->setPosition({ 0.f, 20.f });
        m_tableLayer->addChild(emptyLabel);
        return;
    }

    int startIndex = m_currentPage * CLICKS_PER_PAGE;

    for (int i = 0; i < CLICKS_PER_PAGE; i++) {
        int idx = startIndex + i;
        if (idx >= static_cast<int>(clicks.size())) break;

        const auto& click = clicks[idx];
        float y = 68.f - (i * 19.f);

        // Row background strip
        auto rowBg = CCScale9Sprite::create("square02_small.png");
        rowBg->setContentSize({ 345.f, 17.f });
        rowBg->setOpacity((i % 2 == 0) ? 65 : 35);
        rowBg->setPosition({ 0.f, y });
        m_tableLayer->addChild(rowBg);

        // Column 1: Click Index + Action Type (Press or Release)
        std::string actionType = click.isHold ? "Press" : "Rel";
        std::string pStr = click.isPlayer2 ? " [P2]" : "";
        std::string col1Text = fmt::format("#{} {}{}", click.clickIndex, actionType, pStr);
        auto col1 = CCLabelBMFont::create(col1Text.c_str(), "bigFont.fnt");
        col1->setScale(0.35f);
        col1->setAnchorPoint({ 0.0f, 0.5f });
        col1->setPosition({ -158.f, y });
        m_tableLayer->addChild(col1);

        // Column 2: Percent
        std::string col2Text = fmt::format("{:.2f}%", click.percent);
        auto col2 = CCLabelBMFont::create(col2Text.c_str(), "goldFont.fnt");
        col2->setScale(0.42f);
        col2->setAnchorPoint({ 0.5f, 0.5f });
        col2->setPosition({ 10.f, y });
        m_tableLayer->addChild(col2);

        // Column 3: Tier Button (Cycles on click!)
        char tierChar = static_cast<char>(click.tier);
        std::string btnText = (tierChar == '-') ? "None" : std::string(1, tierChar);
        ccColor4F color = getTierColor(click.tier);

        auto btnSpr = ButtonSprite::create(btnText.c_str(), "bigFont.fnt", "GJ_button_01.png", (tierChar == '-') ? 0.42f : 0.50f);
        btnSpr->setColor({
            static_cast<GLubyte>(color.r * 255),
            static_cast<GLubyte>(color.g * 255),
            static_cast<GLubyte>(color.b * 255)
        });

        auto tierBtn = CCMenuItemSpriteExtra::create(
            btnSpr, this, menu_selector(ShowcasePopup::onRowTierClicked)
        );
        tierBtn->setTag(click.clickIndex);
        tierBtn->setPosition({ 130.f, y });
        m_tableMenu->addChild(tierBtn);
    }
}

void ShowcasePopup::onPrevPage(CCObject*) {
    if (m_currentPage > 0) {
        m_currentPage--;
        updateTable();
    }
}

void ShowcasePopup::onNextPage(CCObject*) {
    auto& clicks = ClickShowcase::get().m_clicks;
    int totalPages = std::max(1, static_cast<int>((clicks.size() + CLICKS_PER_PAGE - 1) / CLICKS_PER_PAGE));
    if (m_currentPage + 1 < totalPages) {
        m_currentPage++;
        updateTable();
    }
}

void ShowcasePopup::onRowTierClicked(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    int clickIndex = node->getTag();
    ClickShowcase::get().cycleClickTier(clickIndex);
    updateTable();
}

void ShowcasePopup::onQuickSetRange(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    char tierChar = static_cast<char>(node->getTag());
    ClickTier tier = static_cast<ClickTier>(tierChar);

    if (!m_rangeInput) return;
    std::string text = m_rangeInput->getString();
    // Trim whitespace
    text.erase(0, text.find_first_not_of(" \t\r\n"));
    text.erase(text.find_last_not_of(" \t\r\n") + 1);

    if (text.empty()) {
        auto alert = FLAlertLayer::create(
            "Quick Assign",
            "Type a Click # (e.g. <cr>4</c> or <cr>1-10</c>) or % (e.g. <cg>3.61%</c> or <cg>10-25%</c>)",
            "OK"
        );
        alert->show();
        return;
    }

    bool isPercent = (text.find('%') != std::string::npos);
    std::string clean = text;
    clean.erase(std::remove(clean.begin(), clean.end(), '%'), clean.end());
    clean.erase(std::remove(clean.begin(), clean.end(), '#'), clean.end());

    auto dashPos = clean.find('-');
    if (dashPos != std::string::npos) {
        // Range: e.g. "1-10" or "10-25%"
        try {
            float val1 = std::stof(clean.substr(0, dashPos));
            float val2 = std::stof(clean.substr(dashPos + 1));
            float minVal = std::min(val1, val2);
            float maxVal = std::max(val1, val2);

            if (isPercent || (minVal < 1.0f && maxVal <= 100.0f && clean.find('.') != std::string::npos)) {
                int affected = ClickShowcase::get().assignByPercentRange(minVal, maxVal, tier);
                geode::Notification::create(
                    fmt::format("Assigned {} clicks in {:.1f}%-{:.1f}% to {}!", affected, minVal, maxVal, tierChar),
                    geode::NotificationIcon::Success
                )->show();
            } else {
                int minC = static_cast<int>(minVal);
                int maxC = static_cast<int>(maxVal);
                int affected = 0;
                for (int c = minC; c <= maxC; c++) {
                    if (ClickShowcase::get().assignByClick(c, tier)) affected++;
                }
                geode::Notification::create(
                    fmt::format("Assigned {} clicks (#{}-#{}) to {}!", affected, minC, maxC, tierChar),
                    geode::NotificationIcon::Success
                )->show();
            }
        } catch (...) {}
    } else {
        // Single value: e.g. "4" or "3.61%"
        try {
            if (isPercent || (clean.find('.') != std::string::npos)) {
                float p = std::stof(clean);
                ClickShowcase::get().assignByPercent(p, tier);
                geode::Notification::create(
                    fmt::format("Assigned click at {:.2f}% to {}!", p, tierChar),
                    geode::NotificationIcon::Success
                )->show();
            } else {
                int c = std::stoi(clean);
                ClickShowcase::get().assignByClick(c, tier);
                geode::Notification::create(
                    fmt::format("Assigned Click #{} to {}!", c, tierChar),
                    geode::NotificationIcon::Success
                )->show();
                m_currentPage = std::max(0, (c - 1) / CLICKS_PER_PAGE);
            }
        } catch (...) {}
    }

    updateTable();
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
            auto alert = FLAlertLayer::create("Success", fmt::format("Loaded {} clicks/inputs!", count).c_str(), "OK");
            alert->show();

            m_currentPage = 0;
            this->updateTable();
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
