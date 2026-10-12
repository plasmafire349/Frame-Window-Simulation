#pragma once
#include <Geode/Geode.hpp>

class ShowcasePopup : public FLAlertLayer {
protected:
    CCTextInputNode* m_inputField = nullptr;
    CCLabelBMFont* m_infoLabel = nullptr;

    bool init() override;

    void onImport(cocos2d::CCObject*);
    void onExport(cocos2d::CCObject*);
    void onToggleMarkers(cocos2d::CCObject*);
    void onToggleAudio(cocos2d::CCObject*);
    void onAssignTier(cocos2d::CCObject* sender);
    void onClose(cocos2d::CCObject*);

    void refreshInfo();

public:
    static ShowcasePopup* create();
};
