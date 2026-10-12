#include "ShowcasePopup.hpp"
#include "ClickShowcase.hpp"
#include <Geode/utils/file.hpp>
#include <string>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

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

    bool isRec = ClickShowcase::get().m_isRecording;
    m_recStatusLabel = CCLabelBMFont::create(
        isRec ? "* RECORDING ACTIVE *" : "",
        "bigFont.fnt"
    );
    m_recStatusLabel->setScale(0.32f);
    m_recStatusLabel->setColor({ 255, 60, 60 });
    m_recStatusLabel->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 112.f });
    m_mainLayer->addChild(m_recStatusLabel);

    // Top menu
    auto topMenu = CCMenu::create();
    topMenu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(topMenu);

    // Record button (top-left)
    auto recSpr = ButtonSprite::create(
        isRec ? "Stop REC" : "Record",
        "goldFont.fnt",
        isRec ? "GJ_button_06.png" : "GJ_button_01.png",
        0.50f
    );
    m_recordBtn = CCMenuItemSpriteExtra::create(
        recSpr, this, menu_selector(ShowcasePopup::onToggleRecord)
    );
    m_recordBtn->setPosition({ -175.f, 126.f });
    topMenu->addChild(m_recordBtn);

    // Folder button (top-left, opens fwc_recordings)
    auto folderSpr = ButtonSprite::create("Folder", "goldFont.fnt", "GJ_button_04.png", 0.50f);
    auto folderBtn = CCMenuItemSpriteExtra::create(
        folderSpr, this, menu_selector(ShowcasePopup::onOpenRecordingsFolder)
    );
    folderBtn->setPosition({ -115.f, 126.f });
    topMenu->addChild(folderBtn);

    // Close button (top right)
    auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSpr, this, menu_selector(ShowcasePopup::onClose)
    );
    closeBtn->setPosition({ 200.f, 126.f });
    topMenu->addChild(closeBtn);

    // ── Page Navigation Bar (y = +100.f) ──────────────────────────────────────
    auto prevSpr = ButtonSprite::create("<", "bigFont.fnt", "GJ_button_01.png", 0.55f);
    m_prevBtn = CCMenuItemSpriteExtra::create(
        prevSpr, this, menu_selector(ShowcasePopup::onPrevPage)
    );
    m_prevBtn->setPosition({ -140.f, 100.f });
    topMenu->addChild(m_prevBtn);

    m_pageLabel = CCLabelBMFont::create("Page 1 / 1 (0 inputs)", "bigFont.fnt");
    m_pageLabel->setScale(0.42f);
    m_pageLabel->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 100.f });
    m_mainLayer->addChild(m_pageLabel);

    auto nextSpr = ButtonSprite::create(">", "bigFont.fnt", "GJ_button_01.png", 0.55f);
    m_nextBtn = CCMenuItemSpriteExtra::create(
        nextSpr, this, menu_selector(ShowcasePopup::onNextPage)
    );
    m_nextBtn->setPosition({ 140.f, 100.f });
    topMenu->addChild(m_nextBtn);

    // ── Quick Assign Bar: Assign Here: [A] [B] [C] [D] [E] [None] (y = +74.f) ──
    auto assignLabel = CCLabelBMFont::create("Assign Here:", "goldFont.fnt");
    assignLabel->setScale(0.40f);
    assignLabel->setPosition({ winSize.width / 2.f - 140.f, winSize.height / 2.f + 74.f });
    m_mainLayer->addChild(assignLabel);

    auto assignMenu = CCMenu::create();
    assignMenu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(assignMenu);

    struct QuickTier {
        const char* label;
        ClickTier tier;
        float xOffset;
        ccColor4F color;
    };
    QuickTier quickTiers[] = {
        { "A", ClickTier::A, -70.f, getTierColor(ClickTier::A) },
        { "B", ClickTier::B, -32.f, getTierColor(ClickTier::B) },
        { "C", ClickTier::C,   6.f, getTierColor(ClickTier::C) },
        { "D", ClickTier::D,  44.f, getTierColor(ClickTier::D) },
        { "E", ClickTier::E,  82.f, getTierColor(ClickTier::E) },
        { "None", ClickTier::None, 135.f, getTierColor(ClickTier::None) }
    };

    for (const auto& qt : quickTiers) {
        auto btnSpr = ButtonSprite::create(qt.label, "bigFont.fnt", "GJ_button_01.png", 0.40f);
        btnSpr->setColor({
            static_cast<GLubyte>(qt.color.r * 255),
            static_cast<GLubyte>(qt.color.g * 255),
            static_cast<GLubyte>(qt.color.b * 255)
        });
        auto btn = CCMenuItemSpriteExtra::create(btnSpr, this, menu_selector(ShowcasePopup::onQuickAssignCurrent));
        btn->setTag(static_cast<int>(qt.tier));
        btn->setPosition({ qt.xOffset, 74.f });
        assignMenu->addChild(btn);
    }

    // ── ScrollLayer Table (400 x 148) ────────────────────────────────────────
    m_scrollLayer = ScrollLayer::create({ 400.f, 148.f });
    m_scrollLayer->setPosition({ winSize.width / 2.f - 200.f, winSize.height / 2.f - 88.f });
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

    // ── Bottom Action Menu (Cleanly spaced, no overlap) ──────────────────────
    auto bottomMenu = CCMenu::create();
    bottomMenu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(bottomMenu);

    // Import button
    auto importSpr = ButtonSprite::create("Import", "goldFont.fnt", "GJ_button_01.png", 0.58f);
    auto importBtn = CCMenuItemSpriteExtra::create(
        importSpr, this, menu_selector(ShowcasePopup::onImport)
    );
    importBtn->setPosition({ -160.f, -114.f });
    bottomMenu->addChild(importBtn);

    // Export button
    auto exportSpr = ButtonSprite::create("Export", "goldFont.fnt", "GJ_button_01.png", 0.58f);
    auto exportBtn = CCMenuItemSpriteExtra::create(
        exportSpr, this, menu_selector(ShowcasePopup::onExport)
    );
    exportBtn->setPosition({ -105.f, -114.f });
    bottomMenu->addChild(exportBtn);

    // Preview button
    auto previewSpr = ButtonSprite::create("Preview", "goldFont.fnt", "GJ_button_02.png", 0.58f);
    auto previewBtn = CCMenuItemSpriteExtra::create(
        previewSpr, this, menu_selector(ShowcasePopup::onPreview)
    );
    previewBtn->setPosition({ -45.f, -114.f });
    bottomMenu->addChild(previewBtn);

    // Checkboxes: Markers, Sounds, Bot Only, Letters (bottom-left HUD)
    struct ToggleDef {
        const char* label;
        float x;
        bool value;
        SEL_MenuHandler handler;
    };
    ToggleDef toggles[] = {
        { "Markers",  5.f,   ClickShowcase::get().m_showMarkers, menu_selector(ShowcasePopup::onToggleMarkers) },
        { "Sounds",   60.f,  ClickShowcase::get().m_playAudio,   menu_selector(ShowcasePopup::onToggleAudio) },
        { "Bot Only", 115.f, ClickShowcase::get().m_botOnly,     menu_selector(ShowcasePopup::onToggleBotOnly) },
        { "Letters",  170.f, ClickShowcase::get().m_showHUD,     menu_selector(ShowcasePopup::onToggleLetters) },
    };
    for (const auto& t : toggles) {
        auto toggler = CCMenuItemToggler::createWithStandardSprites(this, t.handler, 0.55f);
        toggler->toggle(t.value);
        toggler->setPosition({ t.x, -108.f });
        bottomMenu->addChild(toggler);

        auto lbl = CCLabelBMFont::create(t.label, "goldFont.fnt");
        lbl->setScale(0.30f);
        lbl->setPosition({ winSize.width / 2.f + t.x, winSize.height / 2.f - 126.f });
        m_mainLayer->addChild(lbl);
    }

    this->setKeypadEnabled(true);
    this->setTouchEnabled(true);

    // Auto-navigate to page closest to current player position
    if (auto pl = PlayLayer::get()) {
        int currFrame = getCurrentPhysicsFrame();
        auto& clicks = ClickShowcase::get().m_clicks;
        int closestIdx = 0;
        int minDiff = 999999;
        for (size_t i = 0; i < clicks.size(); i++) {
            int diff = std::abs(clicks[i].frame - currFrame);
            if (diff < minDiff) {
                minDiff = diff;
                closestIdx = static_cast<int>(i);
            }
        }
        if (!clicks.empty() && minDiff < 999999) {
            m_currentPage = closestIdx / ITEMS_PER_PAGE;
        }
    }

    refreshList(true);

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
    clickText->setPosition({ 12.f, 18.f });
    clickText->setScale(0.55f);
    clickText->setID("click-text"_spr);
    cell->addChild(clickText);

    // 2. 1P / 2P toggle button
    auto pSpr = ButtonSprite::create("1P");
    pSpr->setScale(0.38f);
    auto pBtn = CCMenuItemSpriteExtra::create(pSpr, this, menu_selector(ShowcasePopup::onRowTogglePlayer));
    pBtn->setPosition({ 105.f, 18.f });
    pBtn->setID("p-btn"_spr);
    menu->addChild(pBtn);

    // 3. Percent text
    auto percentText = CCLabelBMFont::create("0.00%", "goldFont.fnt");
    percentText->setAnchorPoint({ 0.f, 0.5f });
    percentText->setPosition({ 145.f, 18.f });
    percentText->setScale(0.45f);
    percentText->setID("percent-text"_spr);
    cell->addChild(percentText);

    // 4. Tier label
    auto tierLabel = CCLabelBMFont::create("Tier:", "chatFont.fnt");
    tierLabel->setAnchorPoint({ 0.f, 0.5f });
    tierLabel->setPosition({ 225.f, 18.f });
    tierLabel->setScale(0.45f);
    cell->addChild(tierLabel);

    // 5. Tier button (cycles None -> A -> B -> C -> D -> E -> None)
    auto tierSpr = ButtonSprite::create("None", "bigFont.fnt", "GJ_button_01.png", 0.45f);
    auto tierBtn = CCMenuItemSpriteExtra::create(tierSpr, this, menu_selector(ShowcasePopup::onRowTierClicked));
    tierBtn->setPosition({ 282.f, 18.f });
    tierBtn->setID("tier-btn"_spr);
    menu->addChild(tierBtn);

    // 6. Frame label
    auto frameText = CCLabelBMFont::create("F:0", "chatFont.fnt");
    frameText->setAnchorPoint({ 0.f, 0.5f });
    frameText->setPosition({ 325.f, 18.f });
    frameText->setScale(0.45f);
    frameText->setID("frame-text"_spr);
    cell->addChild(frameText);

    // 7. Delete / Mute button (sets tier to None)
    auto delSpr = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
    delSpr->setScale(0.55f);
    auto delBtn = CCMenuItemSpriteExtra::create(delSpr, this, menu_selector(ShowcasePopup::onRowDelete));
    delBtn->setPosition({ 372.f, 18.f });
    delBtn->setID("del-btn"_spr);
    menu->addChild(delBtn);

    return cell;
}

void ShowcasePopup::refreshList(bool resetScroll) {
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
    float contentHeight = std::max(175.f, displayCount * rowHeight);
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
                    clickText->setColor({ 180, 220, 255 }); // cyan tint for releases
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

    if (resetScroll) {
        m_scrollLayer->moveToTop();
    }
}

void ShowcasePopup::onPrevPage(CCObject*) {
    if (m_currentPage > 0) {
        m_currentPage--;
        refreshList(true); // Reset scroll to top on page navigation
    }
}

void ShowcasePopup::onNextPage(CCObject*) {
    auto& clicks = ClickShowcase::get().m_clicks;
    int totalPages = std::max(1, (static_cast<int>(clicks.size()) + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    if (m_currentPage + 1 < totalPages) {
        m_currentPage++;
        refreshList(true); // Reset scroll to top on page navigation
    }
}

void ShowcasePopup::onRowTierClicked(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    int clickIndex = node->getTag();
    ClickTier newTier = ClickShowcase::get().cycleClickTier(clickIndex);

    if (auto pl = PlayLayer::get()) {
        for (auto& c : ClickShowcase::get().m_clicks) {
            if (c.clickIndex == clickIndex) {
                // If position is not set yet, or if player is currently near this click
                if (c.position.x == 0.f && c.position.y == 0.f) {
                    if (pl->m_player1) {
                        float currPercent = pl->getCurrentPercent();
                        if (std::abs(currPercent - c.percent) <= 1.5f) {
                            c.position = pl->m_player1->getPosition();
                        } else {
                            float speedMult = (pl->m_player1->m_playerSpeed > 0.1f) ? pl->m_player1->m_playerSpeed : 1.0f;
                            c.position.x = c.frame * 1.29825f * speedMult;
                            c.position.y = pl->m_player1->getPositionY();
                        }
                    } else {
                        c.position.x = c.frame * 1.29825f;
                        c.position.y = 105.0f;
                    }
                }

                if (c.tier != ClickTier::None) {
                    spawnMarkerInPlayLayer(c.position, getTierColor(c.tier), c.clickIndex, !c.isHold);
                } else {
                    removeMarkerInPlayLayer(c.clickIndex);
                }
                break;
            }
        }
    }

    refreshList(false); // DO NOT reset scroll, keeps user's scroll position!
    triggerPlayLayerHUDUpdate();
}

void ShowcasePopup::onRowDelete(CCObject* sender) {
    auto node = typeinfo_cast<CCNode*>(sender);
    if (!node) return;

    int clickIndex = node->getTag();
    ClickShowcase::get().assignByClick(clickIndex, ClickTier::None);
    removeMarkerInPlayLayer(clickIndex);
    refreshList(false); // DO NOT reset scroll, keeps user's scroll position!
    triggerPlayLayerHUDUpdate();
}

void ShowcasePopup::onQuickAssignCurrent(CCObject* sender) {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_player1) {
        FLAlertLayer::create("Info", "Enter a level and pause to assign your current position!", "OK")->show();
        return;
    }

    auto btn = typeinfo_cast<CCNode*>(sender);
    if (!btn) return;

    ClickTier targetTier = static_cast<ClickTier>(btn->getTag());
    CCPoint playerPos = pl->m_player1->getPosition();
    int currFrame = getCurrentPhysicsFrame();
    float totalDur = ClickShowcase::get().m_duration;
    if (totalDur <= 0.0f && !ClickShowcase::get().m_clicks.empty()) {
        totalDur = static_cast<float>(ClickShowcase::get().m_clicks.back().frame) / ClickShowcase::get().m_fps;
    }
    float currPercent = (totalDur > 0.0f)
        ? std::clamp(((static_cast<float>(currFrame) / ClickShowcase::get().m_fps) / totalDur) * 100.0f, 0.0f, 100.0f)
        : pl->getCurrentPercent();

    auto& clicks = ClickShowcase::get().m_clicks;
    ClickMarkerInfo* match = nullptr;
    int bestDiff = 999999;
    for (auto& c : clicks) {
        int diff = std::abs(c.frame - currFrame);
        if (diff < bestDiff && diff <= 30) {
            bestDiff = diff;
            match = &c;
        }
    }

    if (match) {
        match->tier = targetTier;
        match->position = playerPos;
        match->percent = currPercent;
        ClickShowcase::get().m_manualClickTiers[match->clickIndex] = targetTier;
    } else {
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
                match = &clicks[i];
            }
        }
        if (match && targetTier != ClickTier::None) {
            ClickShowcase::get().m_manualClickTiers[match->clickIndex] = targetTier;
        }
    }

    int cIdx = match ? match->clickIndex : static_cast<int>(clicks.size());
    if (targetTier != ClickTier::None) {
        spawnMarkerInPlayLayer(playerPos, getTierColor(targetTier), cIdx, match ? !match->isHold : false);
    } else {
        removeMarkerInPlayLayer(cIdx);
    }

    triggerPlayLayerHUDUpdate();

    int targetIdx = match ? (match->clickIndex - 1) : (static_cast<int>(clicks.size()) - 1);
    m_currentPage = std::max(0, targetIdx / ITEMS_PER_PAGE);
    refreshList(false);

    if (targetTier != ClickTier::None) {
        Notification::create(
            fmt::format("Assigned {} at {:.2f}% (Frame {})", static_cast<char>(targetTier), currPercent, currFrame),
            NotificationIcon::Success
        )->show();
    } else {
        Notification::create("Cleared marker at current position", NotificationIcon::Info)->show();
    }
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
    refreshList(false); // DO NOT reset scroll, keeps user's scroll position!
}

void ShowcasePopup::onClose(CCObject*) {
    triggerPlayLayerHUDUpdate();
    this->removeFromParentAndCleanup(true);
}

void ShowcasePopup::onToggleMarkers(CCObject*) {
    ClickShowcase::get().m_showMarkers = !ClickShowcase::get().m_showMarkers;
}

void ShowcasePopup::onToggleAudio(CCObject*) {
    ClickShowcase::get().m_playAudio = !ClickShowcase::get().m_playAudio;
}

void ShowcasePopup::onToggleBotOnly(CCObject*) {
    ClickShowcase::get().m_botOnly = !ClickShowcase::get().m_botOnly;
    triggerPlayLayerHUDUpdate();
}

void ShowcasePopup::onToggleLetters(CCObject*) {
    ClickShowcase::get().m_showHUD = !ClickShowcase::get().m_showHUD;
    triggerPlayLayerHUDUpdate();
}

void ShowcasePopup::onImport(CCObject*) {
    file::FilePickOptions options;
    options.defaultPath = ClickShowcase::get().getRecordingsDir();
    options.filters.push_back({
        "Replay / JSON Files (*.gdr2, *.gdr, *.json)",
        { "*.gdr2", "*.gdr", "*.json" }
    });

    auto self = Ref(this);
    async::spawn(file::pick(file::PickMode::OpenFile, options), [self](Result<std::optional<std::filesystem::path>> res) {
        if (!self) return;
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

            self->m_currentPage = 0;
            self->refreshList(true);
            triggerPlayLayerHUDUpdate();
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

void ShowcasePopup::onToggleRecord(CCObject*) {
    togglePlayLayerRecording();

    bool isRec = ClickShowcase::get().m_isRecording;
    if (m_recordBtn) {
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_recordBtn->getNormalImage())) {
            spr->setString(isRec ? "Stop REC" : "Record");
        }
    }
    if (m_recStatusLabel) {
        m_recStatusLabel->setString(isRec ? "* RECORDING ACTIVE *" : "");
    }

    refreshList(true);
}

void ShowcasePopup::onOpenRecordingsFolder(CCObject*) {
    auto dir = ClickShowcase::get().getRecordingsDir();
#ifdef _WIN32
    ShellExecuteA(NULL, "open", dir.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
    file::openFolder(dir);
#endif
    Notification::create(fmt::format("Path: {}", dir.string()), NotificationIcon::Info, 3.5f)->show();
}

void ShowcasePopup::onPreview(CCObject*) {
    file::FilePickOptions options;
    options.defaultPath = ClickShowcase::get().getRecordingsDir();
    options.filters.push_back({
        "Replay / JSON Files (*.gdr2, *.gdr, *.json)",
        { "*.gdr2", "*.gdr", "*.json" }
    });

    auto self = Ref(this);
    async::spawn(file::pick(file::PickMode::OpenFile, options), [self](Result<std::optional<std::filesystem::path>> res) {
        if (!self) return;
        if (res.isOk() && res.unwrap().has_value()) {
            self->startPlaybackPreview(res.unwrap().value().string());
        }
    });
}

void ShowcasePopup::startPlaybackPreview(const std::string& filename) {
    std::filesystem::path path(filename);
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

    if (ClickShowcase::get().m_clicks.empty()) {
        FLAlertLayer::create("Empty File", "The recording file contains no click inputs!", "OK")->show();
        return;
    }

    ClickShowcase::get().m_isPreviewing = true;
    ClickShowcase::get().reset();

    triggerPlayLayerHUDUpdate();

    this->removeFromParentAndCleanup(true);

    auto scene = CCDirector::sharedDirector()->getRunningScene();
    auto pauseLayer = scene ? scene->getChildByType<PauseLayer>(0) : nullptr;
    if (pauseLayer) {
        // Normal in-game restart path (also resumes), safe with other mods' hooks.
        pauseLayer->onRestart(nullptr);
        Notification::create(
            fmt::format("Previewing: {}", path.filename().string()),
            NotificationIcon::Success
        )->show();
    } else {
        ClickShowcase::get().m_isPreviewing = false;
    }
}
