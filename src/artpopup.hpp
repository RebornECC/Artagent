#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
class ArtPopup : public geode::Popup<> {
protected:
    using Listener = geode::EventListener<geode::utils::web::WebTask>;
    geode::TextInput* m_input = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_info = nullptr;
    Listener m_planL, m_imgL;
    // силуэт выделения
    std::vector<uint8_t> m_mask; int m_mw = 0, m_mh = 0; cocos2d::CCRect m_area; float m_fill = 1.f;
    // состояние сессии
    std::string m_brief;                       // текущий план (бриф)
    int m_variant = 0;
    std::vector<uint8_t> m_src; int m_sw = 0, m_sh = 0;   // последний нарисованный рисунок (RGBA)
    bool m_overhang = false;
    int m_limit = 800;
    cocos2d::CCArray* m_placed = nullptr;      // объекты текущего арта
    bool m_busy = false;

    bool setup() override;
    void onClose(cocos2d::CCObject*) override;
    bool buildMask();
    void send();                 // Send: новый арт или правка
    void variant();              // Another variant
    void plan(std::string const& userText);
    void draw();
    void onImage(std::vector<uint8_t> const& png);
    void fit();                  // Vector + Place по кэшированному рисунку
    void clearPlaced();
    void commit();               // Keep: один шаг Undo
    void post(Listener& l, std::string const& url, matjson::Value body, std::function<void(matjson::Value const&)> cb);
    void setStatus(std::string const& s);
public:
    static ArtPopup* create();
};
