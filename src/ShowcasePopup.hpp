#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

class ShowcasePopup : public geode::Popup<> {
protected:
    bool setup() override;

    void onImport(cocos2d::CCObject*);
    void onExport(cocos2d::CCObject*);
    void onToggleMarkers(cocos2d::CCObject*);
    void onToggleAudio(cocos2d::CCObject*);

public:
    static ShowcasePopup* create();
};
