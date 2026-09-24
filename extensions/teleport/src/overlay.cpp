#include "teleport.hpp"
#include <cstring>

// The on-screen prompt and confirmation are drawn as a small retained HUD overlay:
// a tinted panel plus centre-justified text. Layers are created on demand and only
// toggled/relaid afterwards; the host drops them on a world change, at which point
// render() zeroes the ids and they rebuild. Nothing here touches the engine except
// through the HUD table.

namespace teleport {

CssxLayer Extension::make_image(CssxLayer parent) { return hud_ ? hud_->image(ctx_, parent) : 0; }
CssxLayer Extension::make_text(CssxLayer parent) { return hud_ ? hud_->text(ctx_, parent) : 0; }

void Extension::ensure_texture() {
    if (tex_tried_ || !hud_) return;
    tex_tried_ = true;
    const char* p = "assets/panel.png";
    panel_tex_ = hud_->texture(ctx_, p, std::strlen(p));
}

namespace {
void place(const CssxHudApi* h, void* c, CssxLayer l, float x, float y, float w, float ht, int z) {
    if (l) h->set_rect(c, l, x, y, w, ht, 0.5f, 0.5f, z);
}
constexpr float kGoldR = 0.86f, kGoldG = 0.73f, kGoldB = 0.39f;
constexpr float kInkR = 0.93f, kInkG = 0.93f, kInkB = 0.90f;
constexpr float kDimR = 0.72f, kDimG = 0.72f, kDimB = 0.74f;
}

void Extension::show_prompt(const CssxFrame* f, const std::string& name) {
    if (!hud_) return;
    ensure_texture();
    const float vw = static_cast<float>(f->viewport_w), vh = static_cast<float>(f->viewport_h);
    if (vw <= 1 || vh <= 1) return;
    if (!prompt_bg_) {
        prompt_bg_ = make_image(0);
        if (prompt_bg_ && panel_tex_) hud_->set_brush(ctx_, prompt_bg_, panel_tex_);
        if (prompt_bg_) hud_->set_color(ctx_, prompt_bg_, 0.04f, 0.04f, 0.05f, 0.86f);
    }
    if (!prompt_label_) {
        prompt_label_ = make_text(0);
        if (prompt_label_) { hud_->set_font(ctx_, prompt_label_, 26.f); hud_->set_color(ctx_, prompt_label_, kInkR, kInkG, kInkB, 1.f); }
    }
    if (!prompt_key_) {
        prompt_key_ = make_text(0);
        if (prompt_key_) { hud_->set_font(ctx_, prompt_key_, 18.f); hud_->set_color(ctx_, prompt_key_, kGoldR, kGoldG, kGoldB, 1.f); }
    }
    const std::string label = "Teleport to " + name;
    float pw = 360.f + static_cast<float>(label.size()) * 11.f;
    if (pw > 920.f) pw = 920.f;
    if (pw < 360.f) pw = 360.f;
    const float ph = 98.f, cx = vw * 0.5f, py = vh - 150.f;
    place(hud_, ctx_, prompt_bg_, cx, py, pw, ph, 10);
    place(hud_, ctx_, prompt_label_, cx, py - 16.f, pw - 40.f, 38.f, 11);
    place(hud_, ctx_, prompt_key_, cx, py + 24.f, pw - 40.f, 28.f, 11);
    if (prompt_label_) hud_->set_text(ctx_, prompt_label_, label.data(), label.size());
    static const char* keyhint = "T   /   R3";
    if (prompt_key_) hud_->set_text(ctx_, prompt_key_, keyhint, std::strlen(keyhint));
    if (!prompt_visible_) {
        if (prompt_bg_) hud_->set_visible(ctx_, prompt_bg_, 1);
        if (prompt_label_) hud_->set_visible(ctx_, prompt_label_, 1);
        if (prompt_key_) hud_->set_visible(ctx_, prompt_key_, 1);
        prompt_visible_ = true;
    }
    prompt_label_text_ = label;
}

void Extension::hide_prompt() {
    if (!hud_ || !prompt_visible_) return;
    if (prompt_bg_) hud_->set_visible(ctx_, prompt_bg_, 0);
    if (prompt_label_) hud_->set_visible(ctx_, prompt_label_, 0);
    if (prompt_key_) hud_->set_visible(ctx_, prompt_key_, 0);
    prompt_visible_ = false;
}

void Extension::show_confirm(const CssxFrame* f, const std::string& name) {
    if (!hud_) return;
    ensure_texture();
    const float vw = static_cast<float>(f->viewport_w), vh = static_cast<float>(f->viewport_h);
    if (vw <= 1 || vh <= 1) return;
    if (!confirm_bg_) {
        confirm_bg_ = make_image(0);
        if (confirm_bg_ && panel_tex_) hud_->set_brush(ctx_, confirm_bg_, panel_tex_);
        if (confirm_bg_) hud_->set_color(ctx_, confirm_bg_, 0.03f, 0.03f, 0.04f, 0.92f);
    }
    if (!confirm_title_) {
        confirm_title_ = make_text(0);
        if (confirm_title_) { hud_->set_font(ctx_, confirm_title_, 30.f); hud_->set_color(ctx_, confirm_title_, kGoldR, kGoldG, kGoldB, 1.f); }
    }
    if (!confirm_msg_) {
        confirm_msg_ = make_text(0);
        if (confirm_msg_) { hud_->set_font(ctx_, confirm_msg_, 23.f); hud_->set_color(ctx_, confirm_msg_, kInkR, kInkG, kInkB, 1.f); }
    }
    if (!confirm_yes_) {
        confirm_yes_ = make_text(0);
        if (confirm_yes_) { hud_->set_font(ctx_, confirm_yes_, 19.f); hud_->set_color(ctx_, confirm_yes_, kGoldR, kGoldG, kGoldB, 1.f); }
    }
    if (!confirm_no_) {
        confirm_no_ = make_text(0);
        if (confirm_no_) { hud_->set_font(ctx_, confirm_no_, 19.f); hud_->set_color(ctx_, confirm_no_, kDimR, kDimG, kDimB, 1.f); }
    }
    const float cx = vw * 0.5f, cy = vh * 0.5f;
    const float pw = 620.f, ph = 210.f;
    place(hud_, ctx_, confirm_bg_, cx, cy, pw, ph, 20);
    place(hud_, ctx_, confirm_title_, cx, cy - 62.f, pw - 40.f, 40.f, 21);
    place(hud_, ctx_, confirm_msg_, cx, cy - 8.f, pw - 60.f, 34.f, 21);
    place(hud_, ctx_, confirm_yes_, cx - 145.f, cy + 58.f, 260.f, 28.f, 21);
    place(hud_, ctx_, confirm_no_, cx + 145.f, cy + 58.f, 260.f, 28.f, 21);
    static const char* title = "Teleport";
    const std::string msg = "Travel to " + name + "?";
    static const char* yes = "T / R3    Confirm";
    static const char* no = "Esc / B    Cancel";
    if (confirm_title_) hud_->set_text(ctx_, confirm_title_, title, std::strlen(title));
    if (confirm_msg_) hud_->set_text(ctx_, confirm_msg_, msg.data(), msg.size());
    if (confirm_yes_) hud_->set_text(ctx_, confirm_yes_, yes, std::strlen(yes));
    if (confirm_no_) hud_->set_text(ctx_, confirm_no_, no, std::strlen(no));
    if (!confirm_visible_) {
        for (CssxLayer l : {confirm_bg_, confirm_title_, confirm_msg_, confirm_yes_, confirm_no_}) if (l) hud_->set_visible(ctx_, l, 1);
        confirm_visible_ = true;
    }
}

void Extension::hide_confirm() {
    if (!hud_ || !confirm_visible_) return;
    for (CssxLayer l : {confirm_bg_, confirm_title_, confirm_msg_, confirm_yes_, confirm_no_}) if (l) hud_->set_visible(ctx_, l, 0);
    confirm_visible_ = false;
}

void Extension::drop_overlay() {
    if (!hud_) return;
    for (CssxLayer* l : {&prompt_bg_, &prompt_key_, &prompt_label_, &confirm_bg_, &confirm_title_, &confirm_msg_, &confirm_yes_, &confirm_no_}) {
        if (*l) { hud_->destroy(ctx_, *l); *l = 0; }
    }
    prompt_visible_ = confirm_visible_ = false;
}

}
