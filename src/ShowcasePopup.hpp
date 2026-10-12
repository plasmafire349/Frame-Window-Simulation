#pragma once
#include <Geode/Geode.hpp>

class ShowcasePopup : public FLAlertLayer {
protected:
    bool init() override;

    void onImport(cocos2d::CCObject*);
    void onExport(cocos2d::CCObject*);
    void onToggleMarkers(cocos2d::CCObject*);
    void onToggleAudio(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject*);

public:
    static ShowcasePopup* create();
};
