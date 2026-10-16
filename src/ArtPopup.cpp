#include "ArtPopup.hpp"
#include "Trace.hpp"
#include "Knowledge.hpp"
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/GJEffectManager.hpp>
#include <Geode/binding/UndoObject.hpp>
#include <Geode/utils/base64.hpp>
using namespace geode::prelude;

constexpr int   kBlockId = 211;
constexpr int   kFirstChan = 900;
constexpr float kUnit = 30.f;
constexpr int   kMaxSide = 512;

ArtPopup* ArtPopup::create() {
    auto r = new ArtPopup();
    if (r->init()) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

static CCMenuItemSpriteExtra* mkBtn(char const* t, float sc, std::function<void()> f) {
    auto s = ButtonSprite::create(t, "bigFont.fnt", "GJ_button_01.png", sc);
    return CCMenuItemExt::createSpriteExtra(s, [f](CCObject*) { f(); });
}

bool ArtPopup::init() {
    if (!Popup::init(430.f, 270.f)) return false;
    setTitle("Art Agent");
    m_limit = Mod::get()->getSettingValue<int64_t>("limit");
    auto sz = m_mainLayer->getContentSize();
    m_input = TextInput::create(380.f, "Опиши арт или правку: больше снега", "chatFont.fnt");
    m_input->setPosition(sz.width / 2, sz.height - 60.f);
    m_mainLayer->addChild(m_input);
    m_status = CCLabelBMFont::create("Выдели блоки, опиши арт, нажми Send", "chatFont.fnt");
    m_status->setPosition(sz.width / 2, sz.height - 95.f);
    m_mainLayer->addChild(m_status);
    m_info = CCLabelBMFont::create("", "chatFont.fnt");
    m_info->setPosition(sz.width / 2, sz.height - 120.f);
    m_mainLayer->addChild(m_info);

    auto menu = CCMenu::create();
    menu->setPosition(0, 0);
    auto add = [&](char const* t, float x, float y, std::function<void()> f) {
        auto b = mkBtn(t, .6f, std::move(f)); b->setPosition(x, y); menu->addChild(b);
    };
    add("Send",     60,  110, [this] { send(); });
    add("Variant",  150, 110, [this] { variant(); });
    add("Inside",   250, 110, [this] { m_overhang = !m_overhang; fit(); });
    add("Limit -",  60,  65,  [this] { m_limit = std::max(50, m_limit - 100); fit(); });
    add("Limit +",  150, 65,  [this] { m_limit += 100; fit(); });
    add("Undo",     250, 65,  [this] { clearPlaced(); setStatus("Арт убран"); });
    add("Keep",     370, 65,  [this] { onKeep(nullptr); });
    m_mainLayer->addChild(menu);

    // крестик тоже сохраняет арт в Undo
    if (m_closeBtn) m_closeBtn->setTarget(this, menu_selector(ArtPopup::onKeep));
    return true;
}

void ArtPopup::onKeep(CCObject*) {
    commit();
    this->keyBackClicked();
}

void ArtPopup::keyBackClicked() {
    commit();
    Popup::keyBackClicked();
}

void ArtPopup::setStatus(std::string const& s) { m_status->setString(s.c_str()); }

bool ArtPopup::buildMask() {
    auto ui = EditorUI::get();
    auto sel = ui ? ui->getSelectedObjects() : nullptr;
    if (!sel || !sel->count()) return false;
    CCRect all; bool first = true;
    for (auto o : CCArrayExt<GameObject*>(sel)) {
        auto b = o->boundingBox();
        if (first) { all = b; first = false; continue; }
        float x0 = std::min(all.getMinX(), b.getMinX()), y0 = std::min(all.getMinY(), b.getMinY());
        float x1 = std::max(all.getMaxX(), b.getMaxX()), y1 = std::max(all.getMaxY(), b.getMaxY());
        all = CCRect(x0, y0, x1 - x0, y1 - y0);
    }
    m_area = all;
    float s = kMaxSide / std::max(all.size.width, all.size.height);
    m_mw = std::max(1, (int)(all.size.width * s)); m_mh = std::max(1, (int)(all.size.height * s));
    m_mask.assign(m_mw * m_mh, 0);
    for (auto o : CCArrayExt<GameObject*>(sel)) {
        auto b = o->boundingBox();
        int x0 = std::max(0, (int)((b.getMinX() - all.getMinX()) * s)), x1 = std::min(m_mw, (int)((b.getMaxX() - all.getMinX()) * s));
        int y0 = std::max(0, (int)((all.getMaxY() - b.getMaxY()) * s)), y1 = std::min(m_mh, (int)((all.getMaxY() - b.getMinY()) * s));
        for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) m_mask[y * m_mw + x] = 255;
    }
    size_t on = 0; for (auto v : m_mask) on += v ? 1 : 0;
    m_fill = (float)on / m_mask.size();
    return true;
}

void ArtPopup::post(Holder& l, std::string const& url, matjson::Value body, std::function<void(matjson::Value const&)> cb) {
    std::string key = Mod::get()->getSettingValue<std::string>("api-key");
    auto req = web::WebRequest();
    req.header("Authorization", "Bearer " + key);
    req.bodyJSON(body);
    l.spawn(req.post(url), [this, cb](web::WebResponse res) {
        m_busy = false;
        if (!res.ok()) { setStatus(fmt::format("Ошибка API: {}", res.code())); return; }
        auto j = res.json();
        if (!j) { setStatus("Ответ не JSON"); return; }
        cb(j.unwrap());
    });
}

void ArtPopup::send() {
    if (m_busy) return;
    auto text = std::string(m_input->getString());
    if (text.empty()) { setStatus("Напиши, что рисовать"); return; }
    if (Mod::get()->getSettingValue<std::string>("api-key").empty()) { setStatus("Укажи API key в настройках мода"); return; }
    if (!buildMask()) { setStatus("Сначала выдели блоки"); return; }
    m_variant = 0;
    plan(text);
}

void ArtPopup::variant() {
    if (m_busy) return;
    if (m_brief.empty()) { setStatus("Сначала нажми Send"); return; }
    ++m_variant; draw();
}

void ArtPopup::plan(std::string const& userText) {
    m_busy = true; setStatus("Plan...");
    std::string shape = fmt::format("silhouette aspect {:.2f}:1 (width:height), {:.0f}% of the box is filled", m_area.size.width / m_area.size.height, m_fill * 100);
    std::string sys = "You write briefs for an image model that paints flat vector game art for Geometry Dash. "
        "Reply with ONE English paragraph only: subject, shape, 4-6 named colors, flat shading, clean outlines, no text, "
        "centered on a plain white background. If a previous brief is given, apply the requested change to it. ";
    sys += kStyleProfile;
    std::string user = (m_brief.empty() ? "" : "Previous brief: " + m_brief + "\nChange: ") + userText + "\nShape: " + shape;
    auto body = matjson::makeObject({
        {"model", Mod::get()->getSettingValue<std::string>("text-model")},
        {"messages", std::vector<matjson::Value>{
            matjson::makeObject({{"role", "system"}, {"content", sys}}),
            matjson::makeObject({{"role", "user"}, {"content", user}})}}});
    post(m_planL, Mod::get()->getSettingValue<std::string>("text-url"), body, [this](matjson::Value const& j) {
        auto c = j["choices"][(size_t)0]["message"]["content"].asString();
        if (!c) { setStatus("План не получен"); return; }
        m_brief = c.unwrap();
        draw();
    });
}

void ArtPopup::draw() {
    m_busy = true; setStatus(m_variant ? fmt::format("Drawing (вариант {})...", m_variant) : std::string("Drawing..."));
    std::string prompt = m_brief + (m_variant ? fmt::format(" Variation #{}: different composition.", m_variant) : std::string());
    auto msg = matjson::makeObject({{"role", "user"}, {"content", prompt}});
    auto body = matjson::makeObject({
        {"model", Mod::get()->getSettingValue<std::string>("img-model")},
        {"modalities", std::vector<matjson::Value>{"image", "text"}},
        {"messages", std::vector<matjson::Value>{msg}}});
    post(m_imgL, Mod::get()->getSettingValue<std::string>("img-url"), body, [this](matjson::Value const& j) {
        auto url = j["choices"][(size_t)0]["message"]["images"][(size_t)0]["image_url"]["url"].asString();
        if (!url) { setStatus("Модель не вернула картинку"); return; }
        auto u = url.unwrap();
        auto pos = u.find("base64,");
        auto png = utils::base64::decode(pos == std::string::npos ? u : u.substr(pos + 7));
        if (!png) { setStatus("Не декодируется"); return; }
        onImage(png.unwrap());
    });
}

void ArtPopup::onImage(std::vector<uint8_t> const& png) {
    auto img = new CCImage();
    if (!img->initWithImageData(const_cast<uint8_t*>(png.data()), png.size())) { img->release(); setStatus("Битая картинка"); return; }
    m_sw = img->getWidth(); m_sh = img->getHeight();
    m_src.assign(img->getData(), img->getData() + (size_t)m_sw * m_sh * 4);
    img->release();
    fit();
}

void ArtPopup::clearPlaced() {
    if (!m_placed) return;
    auto lel = LevelEditorLayer::get();
    for (auto o : CCArrayExt<GameObject*>(m_placed)) lel->removeObject(o, false);
    CC_SAFE_RELEASE_NULL(m_placed);
}

void ArtPopup::fit() {
    if (m_src.empty()) { setStatus("Сначала нарисуй (Send)"); return; }
    clearPlaced();
    float pad = m_overhang ? 1.3f : 1.f;
    int W = (int)(m_mw * pad), H = (int)(m_mh * pad);
    std::vector<uint8_t> px((size_t)W * H * 4);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        int sx = x * m_sw / W, sy = y * m_sh / H;
        const uint8_t* s = &m_src[((size_t)sy * m_sw + sx) * 4]; uint8_t* d = &px[((size_t)y * W + x) * 4];
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        bool white = s[0] > 245 && s[1] > 245 && s[2] > 245;
        bool inMask = m_overhang ? true : (x < m_mw && y < m_mh && m_mask[y * m_mw + x] != 0);
        d[3] = (inMask && !white) ? 255 : 0;
    }
    auto res = trace::run(px.data(), W, H, m_limit, Mod::get()->getSettingValue<int64_t>("colors"));
    auto lel = LevelEditorLayer::get();
    for (size_t i = 0; i < res.palette.size(); ++i)
        lel->m_effectManager->getColorAction(kFirstChan + (int)i)->m_color = {res.palette[i][0], res.palette[i][1], res.palette[i][2]};
    CCRect area = m_area;
    if (m_overhang) {
        float w = m_area.size.width * pad, h = m_area.size.height * pad;
        area = CCRect(m_area.getMidX() - w / 2, m_area.getMidY() - h / 2, w, h);
    }
    float k = area.size.width / W;
    m_placed = CCArray::create(); m_placed->retain();
    for (auto& r : res.rects) {
        float wu = r.w * k, hu = r.h * k;
        auto o = lel->createObject(kBlockId, ccp(area.getMinX() + r.x * k + wu / 2, area.getMaxY() - r.y * k - hu / 2), false);
        if (!o) continue;
        o->updateCustomScaleX(wu / kUnit); o->updateCustomScaleY(hu / kUnit);
        o->m_baseColor->m_colorID = kFirstChan + r.color;
        m_placed->addObject(o);
    }
    m_info->setString(fmt::format("{} | объектов {}/{}", m_overhang ? "Overhang" : "Inside", m_placed->count(), m_limit).c_str());
    setStatus("Готово. Напиши правку и Send, Variant или Keep");
}

void ArtPopup::commit() {
    if (!m_placed || !m_placed->count()) { CC_SAFE_RELEASE_NULL(m_placed); return; }
    LevelEditorLayer::get()->addToUndoList(UndoObject::createWithArray(m_placed, UndoCommand::Paste), false);
    CC_SAFE_RELEASE_NULL(m_placed);
}
