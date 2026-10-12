#include "ShowcasePopup.hpp"
#include "ClickShowcase.hpp"
#include <Geode/utils/file.hpp>
#include <string>
#include <sstream>
#include <algorithm>

using namespace geode::prelude;

// ClippedMenu: Prevents touches outside ScrollLayer bounds from triggering cell buttons
class ClippedMenu : public CCMenu {
public:
    CCNode* m_clipTarget = nullptr;

    static ClippedMenu* create(CCNode* clipTarget) {
        auto ret = new ClippedMenu();
        if (ret && ret->init()) {
            ret->m_clipTarget = clipTarget;
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        if (m_clipTarget) {
            CCPoint localPt = m_clipTarget->convertTouchToNodeSpace(touch);
            CCRect bounds = { 0, 0, m_clipTarget->getContentSize().width, m_clipTarget->getContentSize().height };
            if (!bounds.containsPoint(localPt)) {
                return false;
            }
        }
        return CCMenu::ccTouchBegan(touch, event);
    }
};

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

    // Dialog background (430 x 285 matching Frame Window Counter)
    auto bg = CCScale9Sprite::create("GJ_square01.png");
    bg->setContentSize({ 430.f, 285.f });
    bg->setPosition(winSize / 2.f);
    m_mainLayer->addChild(bg);

    // Title
    auto title = CCLabelBMFont::create("Click Showcase", "goldFont.fnt");
    title->setScale(0.8f);
    title->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 124.f });
    m_mainLayer->addChild(title);

    // Top menu
    auto topMenu = CCMenu::create();
    topMenu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(topMenu);

    // Close button (top right)
    auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSpr, this, menu_selector(ShowcasePopup::onClose)
    );
    closeBtn->setPosition({ 200.f, 126.f });
    topMenu->addChild(closeBtn);

    // ── Page Navigation Bar (y = +96.f) ──────────────────────────────────────
    auto prevSpr = ButtonSprite::create("<", "bigFont.fnt", "GJ_button_01.png", 0.55f);
    m_prevBtn = CCMenuItemSpriteExtra::create(
        prevSpr, this, menu_selector(ShowcasePopup::onPrevPage)
    );
    m_prevBtn->setPosition({ -140.f, 96.f });
    topMenu->addChild(m_prevBtn);

    m_pageLabel = CCLabelBMFont::create("Page 1 / 1 (0 inputs)", "bigFont.fnt");
    m_pageLabel->setScale(0.42f);
    m_pageLabel->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 96.f });
    m_mainLayer->addChild(m_pageLabel);

    auto nextSpr = ButtonSprite::create(">", "bigFont.fnt", "GJ_button_01.png", 0.55f);
    m_nextBtn = CCMenuItemSpriteExtra::create(
        nextSpr, this, menu_selector(ShowcasePopup::onNextPage)
    );
    m_nextBtn->setPosition({ 140.f, 96.f });
    topMenu->addChild(m_nextBtn);

    // ── ScrollLayer Table (400 x 160) from Frame Window Counter ──────────────
    m_scrollLayer = ScrollLayer::create({ 400.f, 160.f });
    m_scrollLayer->setPosition({ winSize.width / 2.f - 200.f, winSize.height / 2.f - 75.f });
    m_mainLayer->addChild(m_scrollLayer);

    m_contentNode = CCNode::create();
    m_contentNode->setPosition({ 0, 0 });
    m_contentNode->setID("content-node"_spr);
    m_scrollLayer->m_contentLayer->addChild(m_contentNode);

    // Pre-create cell templates (Frame Window Counter pattern)
    for (int i = 0; i < ITEMS_PER_PAGE; i++) {
        auto cell = createCellTemplate(i);
        m_contentNode->addChild(cell);
    }

    // ── Bottom Action & Quick Assign Menu (y = winSize / 2 - 105) ────────────
    auto bottomMenu = CCMenu::create();
    bottomMenu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(bottomMenu);

    // Quick range input box
    auto inputBg = CCScale9Sprite::create("square02_small.png");
    inputBg->setContentSize({ 75.f, 22.f });
    inputBg->setOpacity(120);
    inputBg->setPosition({ winSize.width / 2.f - 150.f, winSize.height / 2.f - 105.f });
    m_mainLayer->addChild(inputBg);

    m_rangeInput = CCTextInputNode::create(70.f, 18.f, "# or %", "bigFont.fnt");
    m_rangeInput->setAllowedChars("0123456789.-% ");
    m_rangeInput->setPosition({ winSize.width / 2.f - 150.f, winSize.height / 2.f - 105.f });
    m_rangeInput->setScale(0.55f);
    m_mainLayer->addChild(m_rangeInput);

    // Quick Tier Assignment Buttons (A, B, C, D, E, -)
    char tiers[] = { 'A', 'B', 'C', 'D', 'E', '-' };
    float tierStartX = -95.f;
    for (int i = 0; i < 6; i++) {
        char t = tiers[i];
        ClickTier cTier = static_cast<ClickTier>(t);
        ccColor4F cColor = getTierColor(cTier);

        std::string labelStr = (t == '-') ? "None" : std::string(1, t);
        auto btnSpr = ButtonSprite::create(labelStr.c_str(), "bigFont.fnt", "GJ_button_01.png", (t == '-') ? 0.38f : 0.45f);
        btnSpr->setColor({
            static_cast<GLubyte>(cColor.r * 255),
            static_cast<GLubyte>(cColor.g * 255),
            static_cast<GLubyte>(cColor.b * 255)
        });

        auto tierBtn = CCMenuItemSpriteExtra::create(
            btnSpr, this, menu_selector(ShowcasePopup::onQuickSetRange)
        );
        tierBtn->setTag(static_cast<int>(t));
        tierBtn->setPosition({ tierStartX + (i * 28.f), -105.f });
        bottomMenu->addChild(tierBtn);
    }

    // Import / Export buttons
    auto importSpr = ButtonSprite::create("Import", "goldFont.fnt", "GJ_button_01.png", 0.55f);
    auto importBtn = CCMenuItemSpriteExtra::create(
        importSpr, this, menu_selector(ShowcasePopup::onImport)
    );
    importBtn->setPosition({ 90.f, -105.f });
    bottomMenu->addChild(importBtn);

    auto exportSpr = ButtonSprite::create("Export", "goldFont.fnt", "GJ_button_01.png", 0.55f);
    auto exportBtn = CCMenuItemSpriteExtra::create(
        exportSpr, this, menu_selector(ShowcasePopup::onExport)
    );
    exportBtn->setPosition({ 140.f, -105.f });
    bottomMenu->addChild(exportBtn);

    // Markers & Audio togglers
    auto togglerMarkers = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleMarkers), 0.55f
    );
    togglerMarkers->toggle(ClickShowcase::get().m_showMarkers);
    togglerMarkers->setPosition({ 178.f, -100.f });
    bottomMenu->addChild(togglerMarkers);

    auto lblMarkers = CCLabelBMFont::create("Markers", "goldFont.fnt");
    lblMarkers->setScale(0.32f);
    lblMarkers->setPosition({ winSize.width / 2.f + 178.f, winSize.height / 2.f - 117.f });
    m_mainLayer->addChild(lblMarkers);

    auto togglerAudio = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleAudio), 0.55f
    );
    togglerAudio->toggle(ClickShowcase::get().m_playAudio);
    togglerAudio->setPosition({ 205.f, -100.f });
    bottomMenu->addChild(togglerAudio);

    auto lblAudio = CCLabelBMFont::create("Sounds", "goldFont.fnt");
    lblAudio->setScale(0.32f);
    lblAudio->setPosition({ winSize.width / 2.f + 205.f, winSize.height / 2.f - 117.f });
    m_mainLayer->addChild(lblAudio);

    this->setKeypadEnabled(true);
    this->setTouchEnabled(true);

    refreshList();

    return true;
}

CCNode* ShowcasePopup::createCellTemplate(int index) {
    auto cell = CCNode::create();
    cell->setContentSize({ 390.f, 36.f });
    cell->setAnchorPoint({ 0.5f, 0.5f });
    cell->setID(fmt::format("cell_{}", index));

    auto bg = CCScale9Sprite::create("square02_small.png");
    bg->setContentSize({ 390.f, 34.f });
    bg->setOpacity(100);
    bg->setPosition({ 195.f, 18.f });
    bg->setID("cell-bg"_spr);
    cell->addChild(bg);

    auto menu = ClippedMenu::create(m_scrollLayer);
    menu->setPosition({ 0.f, 0.f });
    menu->setID("cell-menu"_spr);
    cell->addChild(menu);

    // 1. Click text: #X Press or #X Rel
    auto clickText = CCLabelBMFont::create("#0 Press", "chatFont.fnt");
    clickText->setAnchorPoint({ 0.f, 0.5f });
    clickText->setPosition({ 8.f, 18.f });
    clickText->setScale(0.55f);
    clickText->setID("click-text"_spr);
    cell->addChild(clickText);

    // 2. 1P / 2P toggle button
    auto pSpr = ButtonSprite::create("1P");
    pSpr->setScale(0.38f);
    auto pBtn = CCMenuItemSpriteExtra::create(pSpr, this, menu_selector(ShowcasePopup::onRowTogglePlayer));
    pBtn->setPosition({ 88.f, 18.f });
    pBtn->setID("p-btn"_spr);
    menu->addChild(pBtn);

    // 3. Percent text
    auto percentText = CCLabelBMFont::create("0.00%", "goldFont.fnt");
    percentText->setAnchorPoint({ 0.f, 0.5f });
    percentText->setPosition({ 118.f, 18.f });
    percentText->setScale(0.42f);
    percentText->setID("percent-text"_spr);
    cell->addChild(percentText);

    // 4. Draw marker checkbox (toggles None / Active)
    auto drawToggle = CCMenuItemToggler::create(
        CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"),
        CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
        this, menu_selector(ShowcasePopup::onRowToggleDraw)
    );
    drawToggle->setScale(0.55f);
    drawToggle->setPosition({ 195.f, 18.f });
    drawToggle->setID("draw-toggle"_spr);
    menu->addChild(drawToggle);

    // 5. Tier label
    auto tierLabel = CCLabelBMFont::create("Tier:", "chatFont.fnt");
    tierLabel->setAnchorPoint({ 0.f, 0.5f });
    tierLabel->setPosition({ 222.f, 18.f });
    tierLabel->setScale(0.45f);
    cell->addChild(tierLabel);

    // 6. Tier button (cycles A -> B -> C -> D -> E -> None -> A)
    auto tierSpr = ButtonSprite::create("A", "bigFont.fnt", "GJ_button_01.png", 0.42f);
    auto tierBtn = CCMenuItemSpriteExtra::create(tierSpr, this, menu_selector(ShowcasePopup::onRowTierClicked));
    tierBtn->setPosition({ 275.f, 18.f });
    tierBtn->setID("tier-btn"_spr);
    menu->addChild(tierBtn);

    // 7. Frame label
    auto frameText = CCLabelBMFont::create("F:0", "chatFont.fnt");
    frameText->setAnchorPoint({ 0.f, 0.5f });
    frameText->setPosition({ 312.f, 18.f });
    frameText->setScale(0.45f);
    frameText->setID("frame-text"_spr);
    cell->addChild(frameText);

    // 8. Delete / Mute button
    auto delSpr = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
    delSpr->setScale(0.55f);
    auto delBtn = CCMenuItemSpriteExtra::create(delSpr, this, menu_selector(ShowcasePopup::onRowDelete));
    delBtn->setPosition({ 368.f, 18.f });
    delBtn->setID("del-btn"_spr);
    menu->addChild(delBtn);

    return cell;
}

void ShowcasePopup::refreshList() {
    auto& clicks = ClickShowcase::get().m_clicks;
    int totalItems = static_cast<int>(clicks.size());
    int totalPages = std::max(1, (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);

    if (m_currentPage < 0) m_currentPage = 0;
    if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;

    if (m_pageLabel) {
        if (totalItems == 0) {
            m_pageLabel->setString("No replay loaded (0)");
        } else {
            m_pageLabel->setString(fmt::format("Page {} / {} ({} inputs)", m_currentPage + 1, totalPages, totalItems).c_str());
        }
    }

    if (m_prevBtn) m_prevBtn->setVisible(m_currentPage > 0);
    if (m_nextBtn) m_nextBtn->setVisible(m_currentPage < totalPages - 1);

    if (!m_scrollLayer || !m_contentNode) return;

    int startIdx = m_currentPage * ITEMS_PER_PAGE;
    int endIdx = std::min(totalItems, startIdx + ITEMS_PER_PAGE);
    int displayCount = endIdx - startIdx;

    float rowHeight = 38.f;
    float contentHeight = std::max(160.f, displayCount * rowHeight);
    m_contentNode->setContentSize({ 400.f, contentHeight });
    m_scrollLayer->m_contentLayer->setContentSize({ 400.f, contentHeight });

    float currentY = contentHeight - (rowHeight / 2.f);

    for (int i = 0; i < ITEMS_PER_PAGE; i++) {
        auto cell = m_contentNode->getChildByID(fmt::format("cell_{}", i));
        if (!cell) continue;

        int dataIdx = startIdx + i;
        if (dataIdx < endIdx) {
            cell->setVisible(true);
            cell->setPosition({ 200.f, currentY });
            currentY -= rowHeight;

            const auto& click = clicks[dataIdx];
            cell->setTag(click.clickIndex);

            // Alternate row background opacity
            if (auto bg = static_cast<CCScale9Sprite*>(cell->getChildByID("cell-bg"_spr))) {
                bg->setOpacity((i % 2 == 0) ? 90 : 50);
            }

            // 1. Click text: #X Press or #X Rel
            if (auto clickText = static_cast<CCLabelBMFont*>(cell->getChildByID("click-text"_spr))) {
                clickText->setString(fmt::format("#{} {}", click.clickIndex, click.isHold ? "Press" : "Rel").c_str());
                if (!click.isHold) {
                    clickText->setColor({ 200, 220, 255 }); // subtle cyan tint for releases
                } else {
                    clickText->setColor({ 255, 255, 255 });
                }
            }

            // 2. Menu items
            if (auto menu = cell->getChildByID("cell-menu"_spr)) {
                // P1 / P2
                if (auto pBtn = static_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("p-btn"_spr))) {
                    pBtn->setTag(click.clickIndex);
                    if (auto spr = typeinfo_cast<ButtonSprite*>(pBtn->getNormalImage())) {
                        spr->setString(click.isPlayer2 ? "2P" : "1P");
                    }
                }

                // Draw toggle (checked if not None)
                if (auto drawToggle = static_cast<CCMenuItemToggler*>(menu->getChildByID("draw-toggle"_spr))) {
                    drawToggle->setTag(click.clickIndex);
                    drawToggle->toggle(click.tier != ClickTier::None);
                }

                // Tier button
                if (auto tierBtn = static_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("tier-btn"_spr))) {
                    tierBtn->setTag(click.clickIndex);
                    char tChar = static_cast<char>(click.tier);
                    std::string tStr = (tChar == '-') ? "None" : std::string(1, tChar);
                    ccColor4F color = getTierColor(click.tier);

                    if (auto spr = typeinfo_cast<ButtonSprite*>(tierBtn->getNormalImage())) {
                        spr->setString(tStr.c_str());
                        spr->setColor({
                            static_cast<GLubyte>(color.r * 255),
                            static_cast<GLubyte>(color.g * 255),
                            static_cast<GLubyte>(color.b * 255)
                        });
                    }
                }

                // Delete button
                if (auto delBtn = static_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("del-btn"_spr))) {
                    delBtn->setTag(click.clickIndex);
                }
            }

            // 3. Percent text
            if (auto pctText = static_cast<CCLabelBMFont*>(cell->getChildByID("percent-text"_spr))) {
                pctText->setString(fmt::format("{:.2f}%", click.percent).c_str());
            }

            // 4. Frame text
            if (auto frameText = static_cast<CCLabelBMFont*>(cell->getChildByID("frame-text"_spr))) {
                frameText->setString(fmt::format("F:{}", click.frame).c_str());
            }
        } else {
            cell->setVisible(false);
        }
    }

    m_scrollLayer->moveToTop();
}

void ShowcasePopup::onPrevPage(CCObject*) {
    if (m_currentPage > 0) {
        m_currentPage--;
        refreshList();
    }
}

void ShowcasePopup::onNextPage(CCObject*) {
    auto& clicks = ClickShowcase::get().m_clicks;
    int totalPages = std::max(1, (static_cast<int>(clicks.size()) + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    if (m_currentPage + 1 < totalPages) {
        m_currentPage++;
        refreshList();
    }
}

void ShowcasePopup::onRowTierClicked(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    int clickIndex = node->getTag();
    ClickShowcase::get().cycleClickTier(clickIndex);
    refreshList();
}

void ShowcasePopup::onRowToggleDraw(CCObject* sender) {
    auto toggle = typeinfo_cast<CCMenuItemToggler*>(sender);
    if (!toggle) return;

    int clickIndex = toggle->getTag();
    bool isNowActive = !toggle->isToggled();

    ClickShowcase::get().assignByClick(clickIndex, isNowActive ? ClickTier::C : ClickTier::None);
    refreshList();
}

void ShowcasePopup::onRowDelete(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    int clickIndex = node->getTag();
    ClickShowcase::get().assignByClick(clickIndex, ClickTier::None);
    refreshList();
}

void ShowcasePopup::onRowTogglePlayer(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    int clickIndex = node->getTag();
    for (auto& c : ClickShowcase::get().m_clicks) {
        if (c.clickIndex == clickIndex) {
            c.isPlayer2 = !c.isPlayer2;
            break;
        }
    }
    refreshList();
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
                m_currentPage = std::max(0, (c - 1) / ITEMS_PER_PAGE);
            }
        } catch (...) {}
    }

    refreshList();
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
            this->refreshList();
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
