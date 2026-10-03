#include "guides.hpp"
#include <algorithm>

namespace cine {
namespace {
constexpr int32_t z_order = 9100;   // above the CSSX overlay root content
constexpr float border_px = 2.0f;
}

Guide guide_from(const std::string& id) {
    if (id == "16:9") return Guide::Wide;
    if (id == "9:16") return Guide::Tall;
    if (id == "1:1") return Guide::Square;
    if (id == "2.39:1") return Guide::Scope;
    return Guide::Off;
}
const char* guide_id(Guide g) {
    switch (g) { case Guide::Wide: return "16:9"; case Guide::Tall: return "9:16"; case Guide::Square: return "1:1"; case Guide::Scope: return "2.39:1"; default: return "off"; }
}
double guide_aspect(Guide g) {
    switch (g) { case Guide::Wide: return 16.0 / 9.0; case Guide::Tall: return 9.0 / 16.0; case Guide::Square: return 1.0; case Guide::Scope: return 2.39; default: return 0; }
}

Rect guide_opening(double vw, double vh, double aspect) {
    if (vw <= 0 || vh <= 0 || aspect <= 0) return {0, 0, vw, vh};
    if (vw / vh > aspect) { const double w = vh * aspect; return {(vw - w) / 2, 0, w, vh}; }
    const double h = vw / aspect; return {0, (vh - h) / 2, vw, h};
}

void Guides::build() {
    if (!hud_ || built_) return;
    for (auto& b : bars_) b = hud_->image(context_, 0);
    for (auto& b : border_) b = hud_->image(context_, 0);
    help_ = hud_->text(context_, 0);
    count_ = hud_->text(context_, 0);
    for (auto b : bars_) if (b) hud_->set_color(context_, b, 0, 0, 0, 0.62f);
    for (auto b : border_) if (b) hud_->set_color(context_, b, 1, 1, 1, 0.55f);
    if (help_) { hud_->set_font(context_, help_, 15); hud_->set_color(context_, help_, 1, 1, 1, 0.85f); }
    if (count_) { hud_->set_font(context_, count_, 22); hud_->set_color(context_, count_, 1, 1, 1, 0.75f); }
    built_ = true; shown_ = true;
    help_text_.clear(); count_text_.clear();
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
    const float lines[4][4] = {{x, y, w, border_px}, {x, y + h - border_px, w, border_px}, {x, y, border_px, h}, {x + w - border_px, y, border_px, h}};
    for (int i = 0; i < 4; ++i) {
        if (!border_[i]) continue;
        hud_->set_visible(context_, border_[i], framing);
        if (framing) hud_->set_rect(context_, border_[i], lines[i][0], lines[i][1], lines[i][2], lines[i][3], 0, 0, z_order + 1);
    }
    if (help_) {
        hud_->set_visible(context_, help_, !o.help.empty());
        if (o.help != help_text_) { help_text_ = o.help; hud_->set_text(context_, help_, help_text_.data(), help_text_.size()); }
        hud_->set_rect(context_, help_, x + 24, y + h - 44, std::max(200.0f, w - 48), 30, 0, 0, z_order + 2);
    }
    if (count_) {
        hud_->set_visible(context_, count_, !o.countdown.empty());
        if (o.countdown != count_text_) { count_text_ = o.countdown; hud_->set_text(context_, count_, count_text_.data(), count_text_.size()); }
        hud_->set_rect(context_, count_, x + 24, y + 20, 240, 36, 0, 0, z_order + 2);
    }
}

} // namespace cine
