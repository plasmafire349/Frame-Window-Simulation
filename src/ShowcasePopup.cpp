#include "ShowcasePopup.hpp"
#include "ClickShowcase.hpp"
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

ShowcasePopup* ShowcasePopup::create() {
    auto ret = new ShowcasePopup();
    if (ret && ret->initAnchored(320.f, 240.f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool ShowcasePopup::setup() {
    this->setTitle("Click Showcase");

    auto winSize = m_mainLayer->getContentSize();

    // 1. Status / Info Label
    size_t clickCount = ClickShowcase::get().m_clicks.size();
    auto infoLbl = CCLabelBMFont::create(
        fmt::format("Loaded Clicks: {}", clickCount).c_str(),
        "bigFont.fnt"
    );
    infoLbl->setScale(0.5f);
    infoLbl->setPosition({ winSize.width / 2.f, winSize.height - 60.f });
    m_mainLayer->addChild(infoLbl);

    // 2. Menu for Buttons and Toggles
    auto menu = CCMenu::create();
    menu->setPosition({ winSize.width / 2.f, winSize.height / 2.f - 20.f });
    m_mainLayer->addChild(menu);

    // Toggles for Markers and Audio
    auto togglerMarkers = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleMarkers), 0.7f
    );
    togglerMarkers->toggle(ClickShowcase::get().m_showMarkers);
    togglerMarkers->setPosition({ -70.f, 30.f });
    menu->addChild(togglerMarkers);

    auto lblMarkers = CCLabelBMFont::create("Markers", "goldFont.fnt");
    lblMarkers->setScale(0.55f);
    lblMarkers->setPosition({ -70.f, 5.f });
    menu->addChild(lblMarkers);

    auto togglerAudio = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(ShowcasePopup::onToggleAudio), 0.7f
    );
    togglerAudio->toggle(ClickShowcase::get().m_playAudio);
    togglerAudio->setPosition({ 70.f, 30.f });
    menu->addChild(togglerAudio);

    auto lblAudio = CCLabelBMFont::create("Sounds", "goldFont.fnt");
    lblAudio->setScale(0.55f);
    lblAudio->setPosition({ 70.f, 5.f });
    menu->addChild(lblAudio);

    // Import Replay Button
    auto importSpr = ButtonSprite::create("Import", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto importBtn = CCMenuItemSpriteExtra::create(
        importSpr, this, menu_selector(ShowcasePopup::onImport)
    );
    importBtn->setPosition({ -60.f, -45.f });
    menu->addChild(importBtn);

    // Export Setup Button
    auto exportSpr = ButtonSprite::create("Export", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto exportBtn = CCMenuItemSpriteExtra::create(
        exportSpr, this, menu_selector(ShowcasePopup::onExport)
    );
    exportBtn->setPosition({ 60.f, -45.f });
    menu->addChild(exportBtn);

    return true;
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
