#include "guides.hpp"
#include <algorithm>
#include <iterator>

namespace cine {
namespace {
constexpr int32_t z_order = 9100;   // above the CSSX overlay root content

}

Guide guide_from(const std::string& id) {
    const auto& catalog = content().guides.catalog;
    for (size_t i = 0; i < catalog.size(); ++i) if (catalog[i].id == id) return static_cast<Guide>(i + 1);
    return Guide::Off;
}
const char* guide_id(Guide g) {
    const size_t i = static_cast<size_t>(g);
    return i > 0 && i <= content().guides.catalog.size() ? content().guides.catalog[i - 1].id.c_str() : "off";
}
double guide_aspect(Guide g) {
    const size_t i = static_cast<size_t>(g);
    if (!i || i > content().guides.catalog.size()) return 0;
    const auto& guide = content().guides.catalog[i - 1];
    return guide.width / guide.height;
}
std::vector<std::string> guide_ids() {
    std::vector<std::string> ids{"off"};
    for (const auto& guide : content().guides.catalog) ids.push_back(guide.id);
    return ids;
}
Json guide_options() {
    Json out = Json::array({{{"id", "off"}, {"label", "Off"}}});
    for (const auto& guide : content().guides.catalog) out.push_back({{"id", guide.id}, {"label", guide.label}});
    return out;
}

Rect guide_opening(double vw, double vh, double aspect) {
    if (vw <= 0 || vh <= 0 || aspect <= 0) return {0, 0, vw, vh};
    if (vw / vh > aspect) { const double w = vh * aspect; return {(vw - w) / 2, 0, w, vh}; }
    const double h = vw / aspect; return {0, (vh - h) / 2, vw, h};
}

void Guides::build() {
    if (!hud_ || built_) return;
    // On the screen itself: CINE hides the game's HUD widget in Cine World, and layers inside
    // it would go with it. An older CSSX has no screen root; then the HUD root it is.
    CssxLayer root = CSSX_LAYER_SCREEN;
    bars_[0] = hud_->image(context_, root);
    if (!bars_[0]) { root = 0; bars_[0] = hud_->image(context_, root); }
    for (size_t i = 1; i < std::size(bars_); ++i) bars_[i] = hud_->image(context_, root);
    for (auto& b : border_) b = hud_->image(context_, root);
    help_ = hud_->text(context_, root);
    count_ = hud_->text(context_, root);
    built_ = true; shown_ = true;
    refresh_style();
    help_text_.clear(); count_text_.clear();
}

void Guides::refresh_style() {
    if (!hud_ || !built_) return;
    auto color = [&](CssxLayer layer, const auto& c) { if (layer) hud_->set_color(context_, layer, float(c[0]), float(c[1]), float(c[2]), float(c[3])); };
    for (auto b : bars_) color(b, content().guides.matte);
    for (auto b : border_) color(b, content().guides.border);
    auto face = [&](CssxLayer text, double size, int spacing) { if (face_) face_->apply(text, size, spacing); else hud_->set_font(context_, text, float(size)); };
    if (help_) { face(help_, content().guides.help_font, content().guides.help_spacing); color(help_, content().guides.help_color); }
    if (count_) { face(count_, content().guides.count_font, content().guides.count_spacing); color(count_, content().guides.count_color); }
}

void Guides::show(bool on) {
    if (!built_ || shown_ == on) return;
    for (auto b : bars_) if (b) hud_->set_visible(context_, b, on);
    for (auto b : border_) if (b) hud_->set_visible(context_, b, on);
    if (help_) hud_->set_visible(context_, help_, on);
    if (count_) hud_->set_visible(context_, count_, on);
    shown_ = on;
}

void Guides::hide() { show(false); }
void Guides::forget() { built_ = false; shown_ = false; for (auto& b : bars_) b = 0; for (auto& b : border_) b = 0; help_ = count_ = 0; }

void Guides::update(const CssxFrame& frame, const Overlay& o) {
    if (!hud_) return;
    if (frame.world_generation != world_) { forget(); world_ = frame.world_generation; }
    if (!o.visible) { hide(); return; }
    if (!frame.world_ready) return;
    if (built_ && ++checked_ >= 60) { checked_ = 0; if (bars_[0] && !hud_->layer_object(context_, bars_[0])) forget(); }
    build();
    if (!built_) return;
    show(true);
    const float vw = float(frame.viewport_w), vh = float(frame.viewport_h);
    const bool framing = o.guide != Guide::Off;
    const Rect r = guide_opening(vw, vh, guide_aspect(o.guide));
    // Bars: left, right, top, bottom of the opening (zero-sized when not needed).
    const float x = float(r.x), y = float(r.y), w = float(r.w), h = float(r.h);
    const float rects[4][4] = {{0, 0, x, vh}, {x + w, 0, vw - x - w, vh}, {x, 0, w, y}, {x, y + h, w, vh - y - h}};
    for (int i = 0; i < 4; ++i) {
        if (!bars_[i]) continue;
        const bool on = framing && rects[i][2] > 0.5f && rects[i][3] > 0.5f;
        hud_->set_visible(context_, bars_[i], on);
        if (on) hud_->set_rect(context_, bars_[i], rects[i][0], rects[i][1], rects[i][2], rects[i][3], 0, 0, z_order);
    }
    const float border_px = float(content().guides.border_px);
    const float lines[4][4] = {{x, y, w, border_px}, {x, y + h - border_px, w, border_px}, {x, y, border_px, h}, {x + w - border_px, y, border_px, h}};
    for (int i = 0; i < 4; ++i) {
        if (!border_[i]) continue;
        hud_->set_visible(context_, border_[i], framing);
        if (framing) hud_->set_rect(context_, border_[i], lines[i][0], lines[i][1], lines[i][2], lines[i][3], 0, 0, z_order + 1);
    }
    if (help_) {
        hud_->set_visible(context_, help_, !o.help.empty());
        if (o.help != help_text_) { help_text_ = o.help; hud_->set_text(context_, help_, help_text_.data(), help_text_.size()); }
        hud_->set_rect(context_, help_, x + float(content().guides.help_x), y + h - float(content().guides.help_bottom), std::max(float(content().guides.help_min_width), w - 2 * float(content().guides.help_x)), float(content().guides.help_height), 0, 0, z_order + 2);
    }
    if (count_) {
        hud_->set_visible(context_, count_, !o.countdown.empty());
        if (o.countdown != count_text_) { count_text_ = o.countdown; hud_->set_text(context_, count_, count_text_.data(), count_text_.size()); }
        hud_->set_rect(context_, count_, x + float(content().guides.count_rect[0]), y + float(content().guides.count_rect[1]), float(content().guides.count_rect[2]), float(content().guides.count_rect[3]), 0, 0, z_order + 2);
    }
}

} // namespace cine
