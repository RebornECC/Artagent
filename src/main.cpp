#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include "ArtPopup.hpp"
using namespace geode::prelude;

class $modify(AAPause, EditorPauseLayer) {
    void customSetup() {
        EditorPauseLayer::customSetup();
        auto spr = ButtonSprite::create("Art Agent", "bigFont.fnt", "GJ_button_01.png", .6f);
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCObject*) { ArtPopup::create()->show(); });
        auto menu = CCMenu::create(btn, nullptr);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition(70.f, win.height - 30.f);
        this->addChild(menu);
    }
};
