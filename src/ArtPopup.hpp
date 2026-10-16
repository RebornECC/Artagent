#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

class ArtPopup : public geode::Popup {
protected:
    using Holder = geode::async::TaskHolder<geode::utils::web::WebResponse>;
    geode::TextInput* m_input = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_info = nullptr;
    Holder m_planL, m_imgL;
    // силуэт выделения
    std::vector<uint8_t> m_mask; int m_mw = 0, m_mh = 0; cocos2d::CCRect m_area; float m_fill = 1.f;
    // состояние сессии
    std::string m_brief;
    int m_variant = 0;
    std::vector<uint8_t> m_src; int m_sw = 0, m_sh = 0;
    bool m_overhang = false;
    int m_limit = 800;
    cocos2d::CCArray* m_placed = nullptr;
    bool m_busy = false;

    bool init();
    void keyBackClicked() override;
    void onKeep(cocos2d::CCObject*);
    bool buildMask();
    void send();
    void variant();
    void plan(std::string const& userText);
    void draw();
    void onImage(std::vector<uint8_t> const& png);
    void fit();
    void clearPlaced();
    void commit();
    void post(Holder& l, std::string const& url, matjson::Value body, std::function<void(matjson::Value const&)> cb);
    void setStatus(std::string const& s);
public:
    static ArtPopup* create();
};
