#pragma once
#include <Geode/Geode.hpp>

class ShowcasePopup : public FLAlertLayer {
protected:
    int m_currentPage = 0;
    static constexpr int CLICKS_PER_PAGE = 6;

    cocos2d::CCLayer* m_tableLayer = nullptr;
    cocos2d::CCMenu* m_tableMenu = nullptr;
    cocos2d::CCLabelBMFont* m_pageLabel = nullptr;
    CCTextInputNode* m_rangeInput = nullptr;

    bool init() override;

    void onPrevPage(cocos2d::CCObject*);
    void onNextPage(cocos2d::CCObject*);
    void onRowTierClicked(cocos2d::CCObject* sender);
    void onQuickSetRange(cocos2d::CCObject* sender);

    void onImport(cocos2d::CCObject*);
    void onExport(cocos2d::CCObject*);
    void onToggleMarkers(cocos2d::CCObject*);
    void onToggleAudio(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject*);

    void updateTable();

public:
    static ShowcasePopup* create();
};
