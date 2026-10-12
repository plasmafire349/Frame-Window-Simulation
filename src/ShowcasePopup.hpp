#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/ScrollLayer.hpp>

class ShowcasePopup : public FLAlertLayer {
protected:
    int m_currentPage = 0;
    static constexpr int ITEMS_PER_PAGE = 20;

    geode::ScrollLayer* m_scrollLayer = nullptr;
    cocos2d::CCNode* m_contentNode = nullptr;
    cocos2d::CCLabelBMFont* m_pageLabel = nullptr;
    CCMenuItemSpriteExtra* m_prevBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextBtn = nullptr;

    bool init() override;

    cocos2d::CCNode* createCellTemplate(int index);
    void refreshList(bool resetScroll = false);

    void onPrevPage(cocos2d::CCObject*);
    void onNextPage(cocos2d::CCObject*);
    void onRowTierClicked(cocos2d::CCObject* sender);
    void onRowDelete(cocos2d::CCObject* sender);
    void onRowTogglePlayer(cocos2d::CCObject* sender);

    void onImport(cocos2d::CCObject*);
    void onExport(cocos2d::CCObject*);
    void onToggleMarkers(cocos2d::CCObject*);
    void onToggleAudio(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject*);

public:
    static ShowcasePopup* create();
};
