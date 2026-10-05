#include "callouts.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace cine {
namespace {
constexpr int32_t z_order = 9150;   // above the frame guides
constexpr double margin = 0.045;    // label columns sit this far in from the left and right edges
constexpr double pad = 10;          // gap between a label and the start of its leader
}

double label_width(const std::string& text) { return 0.56 * callout_font * double(text.size()); }

std::vector<CalloutPlace> layout_callouts(double w, double h, const std::vector<CalloutTarget>& targets) {
    std::vector<CalloutPlace> out;
    if (!(w > 100) || !(h > 100)) return out;
    // Each visible target goes to the column on its own side, so a leader never crosses her.
    std::vector<size_t> left, right;
    for (size_t i = 0; i < targets.size(); ++i) {
        const auto& t = targets[i];
        if (!t.visible || !std::isfinite(t.x) || !std::isfinite(t.y) || t.x < 0 || t.y < 0 || t.x > w || t.y > h) continue;
        (t.x < w / 2 ? left : right).push_back(i);
    }
    // Keep the columns balanced enough to fit, moving the innermost targets across.
    auto by_x = [&](size_t a, size_t b) { return targets[a].x < targets[b].x; };
    std::sort(left.begin(), left.end(), by_x); std::sort(right.begin(), right.end(), by_x);
    while (left.size() + right.size() > max_callouts) (left.size() > right.size() ? left : right).pop_back();
    const size_t per_column = (max_callouts + 1) / 2;
    while (left.size() > per_column) { right.insert(right.begin(), left.back()); left.pop_back(); }
    while (right.size() > per_column) { left.push_back(right.front()); right.erase(right.begin()); }
    auto column = [&](std::vector<size_t>& ids, bool on_left) {
        // Top to bottom in the order of the targets, so leaders in one column do not cross.
        std::sort(ids.begin(), ids.end(), [&](size_t a, size_t b) { return targets[a].y < targets[b].y; });
        const double top = h * 0.22, bottom = h * 0.72;
        const double step = ids.size() > 1 ? std::min((bottom - top) / double(ids.size() - 1), callout_line * 3.2) : 0;
        const double first = ids.size() > 1 ? (top + bottom) / 2 - step * double(ids.size() - 1) / 2 : (top + bottom) / 2;
        for (size_t row = 0; row < ids.size(); ++row) {
            const auto& t = targets[ids[row]];
            CalloutPlace p;
            p.target = ids[row];
            const double width = label_width(t.text);
            p.label_x = on_left ? w * margin : w * (1 - margin) - width;
            p.label_y = first + step * double(row) - callout_line / 2;
            p.from_x = on_left ? p.label_x + width + pad : p.label_x - pad;
            p.from_y = p.label_y + callout_line / 2;
            p.to_x = t.x; p.to_y = t.y;
            out.push_back(p);
        }
    };
    column(left, true);
    column(right, false);
    return out;
}

void Callouts::build() {
    if (!hud_ || built_) return;
    // On the screen itself, like the frame guides: the game's HUD widget is hidden during a take.
    CssxLayer root = CSSX_LAYER_SCREEN;
    caption_bar_ = hud_->image(context_, root);
    if (!caption_bar_) { root = 0; caption_bar_ = hud_->image(context_, root); }
    for (auto& s : slots_) {
        s.leader = hud_->image(context_, root);
        for (auto& head : s.head) head = hud_->image(context_, root);
        s.text = hud_->text(context_, root);
        for (CssxLayer l : {s.leader, s.head[0], s.head[1]}) if (l) { hud_->set_color(context_, l, 1, 1, 1, 0.9f); hud_->set_pivot(context_, l, 0, 0.5f); hud_->set_visible(context_, l, 0); }
        if (s.text) { hud_->set_font(context_, s.text, float(callout_font)); hud_->set_color(context_, s.text, 1, 1, 1, 0.96f); hud_->set_visible(context_, s.text, 0); }
        s.shown.clear();
    }
    caption_ = hud_->text(context_, root);
    if (caption_bar_) { hud_->set_color(context_, caption_bar_, 0, 0, 0, 0.55f); hud_->set_visible(context_, caption_bar_, 0); }
    if (caption_) { hud_->set_font(context_, caption_, 22); hud_->set_color(context_, caption_, 1, 1, 1, 0.97f); hud_->set_visible(context_, caption_, 0); }
    caption_shown_.clear();
    built_ = true; visible_ = true;
}

// A thin image from (x0, y0) to (x1, y1): laid out flat from its start, then turned about it.
void Callouts::line(CssxLayer layer, double x0, double y0, double x1, double y1, float thickness, int z) {
    if (!layer) return;
    const double dx = x1 - x0, dy = y1 - y0, length = std::hypot(dx, dy);
    hud_->set_rect(context_, layer, float(x0), float(y0 - thickness / 2), float(length), thickness, 0, 0, z);
    hud_->set_angle(context_, layer, float(std::atan2(dy, dx) * 180 / std::numbers::pi));
    hud_->set_visible(context_, layer, length > 1 ? 1 : 0);
}

void Callouts::hide() {
    if (!built_ || !visible_) return;
    for (auto& s : slots_) for (CssxLayer l : {s.text, s.leader, s.head[0], s.head[1]}) if (l) hud_->set_visible(context_, l, 0);
    if (caption_) hud_->set_visible(context_, caption_, 0);
    if (caption_bar_) hud_->set_visible(context_, caption_bar_, 0);
    visible_ = false;
}

void Callouts::forget() {
    built_ = visible_ = false;
    for (auto& s : slots_) s = Slot{};
    caption_ = caption_bar_ = 0;
    caption_shown_.clear();
}

void Callouts::update(const CssxFrame& frame, const std::vector<CalloutTarget>& targets, const std::string& caption) {
    if (!hud_) return;
    if (frame.world_generation != world_) { forget(); world_ = frame.world_generation; }
    if (!frame.world_ready) return;
    if (targets.empty() && caption.empty()) { hide(); return; }
    build();
    if (!built_) return;
    visible_ = true;
    const double w = frame.viewport_w, h = frame.viewport_h;
    auto places = layout_callouts(w, h, targets);
    // Half-pixel steps: sub-pixel jitter in a still shot sends nothing to the HUD.
    for (auto& p : places) { p.to_x = std::round(p.to_x * 2) / 2; p.to_y = std::round(p.to_y * 2) / 2; }
    for (size_t i = 0; i < max_callouts; ++i) {
        auto& s = slots_[i];
        if (i >= places.size()) { for (CssxLayer l : {s.text, s.leader, s.head[0], s.head[1]}) if (l) hud_->set_visible(context_, l, 0); continue; }
        const auto& p = places[i];
        const auto& text = targets[p.target].text;
        if (s.text) {
            if (text != s.shown) { s.shown = text; hud_->set_text(context_, s.text, text.data(), text.size()); }
            hud_->set_rect(context_, s.text, float(p.label_x), float(p.label_y), float(label_width(text) + 24), float(callout_line), 0, 0, z_order + 1);
            hud_->set_visible(context_, s.text, 1);
        }
        line(s.leader, p.from_x, p.from_y, p.to_x, p.to_y, 1.6f, z_order);
        // Arrowhead: two short strokes back from the tip, 24 degrees either side of the leader.
        const double angle = std::atan2(p.from_y - p.to_y, p.from_x - p.to_x);
        for (int k = 0; k < 2; ++k) {
            const double a = angle + (k ? 0.42 : -0.42);
            line(s.head[k], p.to_x, p.to_y, p.to_x + std::cos(a) * 13, p.to_y + std::sin(a) * 13, 1.8f, z_order);
        }
    }
    const bool captioned = !caption.empty();
    if (caption_) {
        if (caption != caption_shown_) { caption_shown_ = caption; hud_->set_text(context_, caption_, caption.data(), caption.size()); }
        hud_->set_rect(context_, caption_, float(w * margin + 14), float(h * 0.86), float(std::max(240.0, w * 0.6)), 34, 0, 0, z_order + 1);
        hud_->set_visible(context_, caption_, captioned ? 1 : 0);
    }
    if (caption_bar_) {
        hud_->set_rect(context_, caption_bar_, float(w * margin), float(h * 0.86 - 6), float(0.62 * 22 * double(caption.size()) + 28), 44, 0, 0, z_order);
        hud_->set_visible(context_, caption_bar_, captioned ? 1 : 0);
    }
}

} // namespace cine
