#include "ShowcasePopup.hpp"
#include "ClickShowcase.hpp"
#include <Geode/utils/file.hpp>

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
    bg->setContentSize({ 320.f, 240.f });
    bg->setPosition(winSize / 2.f);
    m_mainLayer->addChild(bg);

    // Title
    auto title = CCLabelBMFont::create("Click Showcase", "goldFont.fnt");
    title->setScale(0.8f);
    title->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 95.f });
    m_mainLayer->addChild(title);

    // Info Label
    size_t clickCount = ClickShowcase::get().m_clicks.size();
    auto infoLbl = CCLabelBMFont::create(
        fmt::format("Loaded Clicks: {}", clickCount).c_str(),
        "bigFont.fnt"
    );
    infoLbl->setScale(0.45f);
    infoLbl->setPosition({ winSize.width / 2.f, winSize.height / 2.f + 65.f });
    m_mainLayer->addChild(infoLbl);

    // Interactive Menu
    auto menu = CCMenu::create();
    menu->setPosition(winSize / 2.f);
    m_mainLayer->addChild(menu);

    // Close button (top right)
    auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSpr, this, menu_selector(ShowcasePopup::onClose)
    );
    closeBtn->setPosition({ -145.f, 105.f });
    menu->addChild(closeBtn);

    // Toggles for Markers and Audio
    auto togglerMarkers = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleMarkers), 0.7f
    );
    togglerMarkers->toggle(ClickShowcase::get().m_showMarkers);
    togglerMarkers->setPosition({ -60.f, 15.f });
    menu->addChild(togglerMarkers);

    auto lblMarkers = CCLabelBMFont::create("Markers", "goldFont.fnt");
    lblMarkers->setScale(0.55f);
    lblMarkers->setPosition({ winSize.width / 2.f - 60.f, winSize.height / 2.f - 10.f });
    m_mainLayer->addChild(lblMarkers);

    auto togglerAudio = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleAudio), 0.7f
    );
    togglerAudio->toggle(ClickShowcase::get().m_playAudio);
    togglerAudio->setPosition({ 60.f, 15.f });
    menu->addChild(togglerAudio);

    auto lblAudio = CCLabelBMFont::create("Sounds", "goldFont.fnt");
    lblAudio->setScale(0.55f);
    lblAudio->setPosition({ winSize.width / 2.f + 60.f, winSize.height / 2.f - 10.f });
    m_mainLayer->addChild(lblAudio);

    // Import Replay Button
    auto importSpr = ButtonSprite::create("Import", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto importBtn = CCMenuItemSpriteExtra::create(
        importSpr, this, menu_selector(ShowcasePopup::onImport)
    );
    importBtn->setPosition({ -65.f, -60.f });
    menu->addChild(importBtn);

    // Export Setup Button
    auto exportSpr = ButtonSprite::create("Export", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto exportBtn = CCMenuItemSpriteExtra::create(
        exportSpr, this, menu_selector(ShowcasePopup::onExport)
    );
    exportBtn->setPosition({ 65.f, -60.f });
    menu->addChild(exportBtn);

    this->setKeypadEnabled(true);
    this->setTouchEnabled(true);

    return true;
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
        "Replay Files (*.gdr2, *.gdr, *.json)",
        { "*.gdr2", "*.gdr", "*.json" }
    });

    async::spawn(file::pick(file::PickMode::OpenFile, options), [this](Result<std::optional<std::filesystem::path>> res) {
        if (res.isOk() && res.unwrap().has_value()) {
            auto path = res.unwrap().value();
            float levelLen = 10000.f;
            if (auto pl = PlayLayer::get()) {
                levelLen = pl->m_levelLength;
            }
            ClickShowcase::get().loadReplayFile(path, levelLen);

            size_t count = ClickShowcase::get().m_clicks.size();
            auto alert = FLAlertLayer::create("Success", fmt::format("Loaded {} clicks!", count).c_str(), "OK");
            alert->show();

            this->onClose(nullptr);
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
