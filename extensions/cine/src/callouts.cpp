#include "callouts.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>

namespace cine {
namespace {
constexpr int32_t z_order = 9150;   // above the frame guides
bool is_handle(const Json& j) { return j.is_object() && j.contains("$object"); }
// Width of upper-case letter-spaced text in this face, a little generous. A font size is in
// points: one em is 4/3 of it in HUD units (measured in game: 51 characters at size 20 and
// spacing 150 ran 880 units).
double text_width(size_t characters, double font, int spacing) { return (content().callouts.width_em + spacing / 1000.0) * font * (4.0 / 3.0) * double(characters); }
}

double frame_scale(const Rect& frame) { return std::clamp(frame.w / content().callouts.reference_width, content().callouts.scale_min, content().callouts.scale_max); }
double label_width(const std::string& text, double scale) { return text_width(text.size(), callout_font * scale, content().callouts.label_spacing) + content().callouts.label_padding * scale; }

std::string readout(const std::string& text) {
    std::string out = text;
    for (auto& c : out) if (content().callouts.uppercase && (c >= 'a' && c <= 'z')) c = char(c - 'a' + 'A');
    return out;
}

std::vector<std::string> fit_signature(const std::vector<std::string>& lines, double width, double scale) {
    std::vector<std::string> out;
    for (size_t i = 0; i < lines.size() && out.size() < max_signature_lines; ++i) {
        const bool name = i == 0;
        const double font = (name ? content().callouts.name_font : content().callouts.sign_font) * scale;
        const int spacing = name ? content().callouts.name_spacing : content().callouts.sign_spacing;
        std::string rest = lines[i];
        // Break before " + " while the line is too long: "A + B + C" becomes "A", "+ B", "+ C".
        while (out.size() < max_signature_lines) {
            if (text_width(rest.size(), font, spacing) <= width) { out.push_back(rest); break; }
            size_t cut = std::string::npos;
            for (size_t at = rest.find(content().callouts.signature_break, 1); at != std::string::npos; at = rest.find(content().callouts.signature_break, at + 1)) {
                if (text_width(at, font, spacing) > width) break;
                cut = at;
            }
            if (cut == std::string::npos) cut = rest.find(content().callouts.signature_break, 1);   // even the first part is too long: break there anyway
            if (cut == std::string::npos) { out.push_back(rest); break; }
            out.push_back(rest.substr(0, cut));
            rest = rest.substr(cut + size_t(content().callouts.signature_skip));   // keeps the "+ "
        }
    }
    return out;
}

std::vector<CalloutPlace> layout_callouts(const Rect& frame, const std::vector<CalloutTarget>& targets, double line_x) {
    std::vector<CalloutPlace> out;
    if (!(frame.w > 100) || !(frame.h > 100)) return out;
    const double scale = frame_scale(frame), line_height = callout_line * scale;
    const double right_edge = frame.x + frame.w, bottom_edge = frame.y + frame.h, centre = frame.x + frame.w / 2;
    // Each visible target goes to the column on its own side, so a leader never crosses her.
    const bool split = line_x < right_edge - 1;
    if (split) {
        // A split holds the same layers for a long time. Each label keeps the row of its place
        // in the list, shown or not, so nothing shuffles when a target leaves the frame or two
        // targets pass each other: only the arrows move.
        const size_t rows = std::min(targets.size(), max_callouts);
        const double top = frame.y + frame.h * content().callouts.row_top, bottom = frame.y + frame.h * content().callouts.row_bottom;
        const double step = rows > 1 ? std::min((bottom - top) / double(rows - 1), line_height * content().callouts.max_row_step) : 0;
        const double first = (top + bottom) / 2 - step * double(rows > 0 ? rows - 1 : 0) / 2;
        for (size_t row = 0; row < rows; ++row) {
            const auto& t = targets[row];
            if (!t.visible || !std::isfinite(t.x) || !std::isfinite(t.y) || t.x < frame.x || t.y < frame.y || t.x > right_edge || t.y > bottom_edge) continue;
            // Right of the line is the finished picture: there is nothing of this label to point
            // at there. A target on the line itself (her spine, her head) stays.
            if (t.x > line_x + frame.w * content().callouts.split_grace) continue;
            CalloutPlace p;
            p.target = row;
            p.on_left = true;
            p.width = std::min(label_width(t.text, scale), frame.w * content().callouts.max_width);
            p.label_x = frame.x + frame.w * content().callouts.margin;
            p.label_y = first + step * double(row) - line_height / 2;
            p.from_x = p.label_x + p.width;
            p.from_y = p.label_y + line_height + callout_shelf * scale;
            p.to_x = std::min(t.x, line_x); p.to_y = t.y;
            for (const auto& [x, y] : t.more)
                if (std::isfinite(x) && std::isfinite(y) && x >= frame.x && y >= frame.y && x <= right_edge && y <= bottom_edge && x <= line_x + frame.w * content().callouts.split_grace && p.more_to.size() < 3)
                    p.more_to.push_back({std::min(x, line_x), y});
            out.push_back(p);
        }
        return out;
    }
    std::vector<size_t> left, right;
    for (size_t i = 0; i < targets.size(); ++i) {
        const auto& t = targets[i];
        if (!t.visible || !std::isfinite(t.x) || !std::isfinite(t.y) || t.x < frame.x || t.y < frame.y || t.x > right_edge || t.y > bottom_edge) continue;
        (t.x < centre ? left : right).push_back(i);
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
        const double top = frame.y + frame.h * content().callouts.row_top, bottom = frame.y + frame.h * content().callouts.row_bottom;
        const double step = ids.size() > 1 ? std::min((bottom - top) / double(ids.size() - 1), line_height * content().callouts.max_row_step) : 0;
        const double first = ids.size() > 1 ? (top + bottom) / 2 - step * double(ids.size() - 1) / 2 : (top + bottom) / 2;
        for (size_t row = 0; row < ids.size(); ++row) {
            const auto& t = targets[ids[row]];
            CalloutPlace p;
            p.target = ids[row];
            p.on_left = on_left;
            p.width = std::min(label_width(t.text, scale), frame.w * content().callouts.max_width);
            p.label_x = on_left ? frame.x + frame.w * content().callouts.margin : right_edge - frame.w * content().callouts.margin - p.width;
            p.label_y = first + step * double(row) - line_height / 2;
            // The leader leaves the inner end of the shelf under the label.
            p.from_x = on_left ? p.label_x + p.width : p.label_x;
            p.from_y = p.label_y + line_height + callout_shelf * scale;
            p.to_x = t.x; p.to_y = t.y;
            for (const auto& [x, y] : t.more)
                if (std::isfinite(x) && std::isfinite(y) && x >= frame.x && y >= frame.y && x <= right_edge && y <= bottom_edge && p.more_to.size() < 3)
                    p.more_to.push_back({x, y});
            out.push_back(p);
        }
    };
    column(left, true);
    column(right, false);
    return out;
}

// ---------------------------------------------------------------- typeface

void Typeface::refresh() {
    const auto& c = content().callouts;
    if (asset_ != c.font_asset || file_ != c.font_file || face_name_ != c.face) {
        forget();
        asset_ = c.font_asset; file_ = c.font_file; face_name_ = c.face;
    }
}

bool Typeface::ensure() {
    refresh();
    if (!host_ || failed_) return false;
    try {
        if (is_handle(font_) && host_->request({{"op", "valid"}, {"target", font_}}) == true) return true;
        font_ = nullptr;
        failed_ = true;   // cleared again only when everything below worked
        const auto face = host_->request({{"op", "load"}, {"path", content().callouts.font_asset}});
        std::string path;
        if (!is_handle(face)) {
            const auto info = host_->request({{"op", "extension.info"}});
            const auto directory = info.at("directory").get<std::string>();
            std::error_code missing;
            if (!std::filesystem::is_regular_file(std::filesystem::path(directory) / content().callouts.font_file, missing)) return false;
            path = directory + "/" + content().callouts.font_file;
            std::replace(path.begin(), path.end(), '\\', '/');
        }
        const auto player = host_->player();
        const auto statics = host_->find("/Script/Engine.Default__GameplayStatics");
        const auto font_class = host_->request({{"op", "load"}, {"path", "/Script/Engine.Font"}});
        if (!is_handle(statics) || !is_handle(font_class) || !player.is_object() || !is_handle(player.value("controller", Json()))) return false;
        const auto made = host_->call(statics, "SpawnObject", {{"ObjectClass", font_class}, {"Outer", player.at("controller")}});
        if (!is_handle(made)) return false;
        host_->set(made, "FontCacheType", 1);   // a runtime (composite) font
        Json data = is_handle(face) ? Json{{"FontFaceAsset", face}} : Json{{"FontFilename", path}, {"Hinting", 0}, {"LoadingPolicy", 0}};
        host_->request({{"op", "set"}, {"target", made}, {"property", "CompositeFont"}, {"resize", true}, {"echo", false},
                        {"value", {{"DefaultTypeface", {{"Fonts", Json::array({{{"Name", content().callouts.face}, {"Font", data}}})}}}}}});
        font_ = made;
        failed_ = false;
        return true;
    } catch (...) { font_ = nullptr; failed_ = true; return false; }
}

void Typeface::apply(CssxLayer layer, double size, int spacing) {
    if (!hud_ || !layer) return;
    hud_->set_font(context_, layer, float(size));   // the size at least, whatever happens next
    if (!ensure()) return;
    try {
        const uint64_t widget = hud_->layer_object(context_, layer);
        if (!widget) return;
        // The text block keeps the font object alive from here on.
        host_->call(Json{{"$object", widget}}, "SetFont",
                    {{"InFontInfo", {{"FontObject", font_}, {"TypefaceFontName", content().callouts.face}, {"Size", size}, {"LetterSpacing", spacing}}}});
    } catch (...) {}
}

void Typeface::justify(CssxLayer layer, int side) {
    if (!hud_ || !host_ || !layer) return;
    try {
        const uint64_t widget = hud_->layer_object(context_, layer);
        if (widget) host_->call(Json{{"$object", widget}}, "SetJustification", {{"InJustification", side}});
    } catch (...) {}
}

// ---------------------------------------------------------------- drawing

void Callouts::build() {
    if (!hud_ || built_) return;
    // On the screen itself, like the frame guides: the game's HUD widget is hidden during a take.
    CssxLayer root = CSSX_LAYER_SCREEN;
    caption_bar_ = hud_->image(context_, root);
    if (!caption_bar_) { root = 0; caption_bar_ = hud_->image(context_, root); }
    auto paint = [&](CssxLayer l, const std::array<double, 4>& c) { if (l) { hud_->set_color(context_, l, float(c[0]), float(c[1]), float(c[2]), float(c[3])); hud_->set_visible(context_, l, 0); } };
    for (auto& s : slots_) {
        s.bar = hud_->image(context_, root);
        s.shelf = hud_->image(context_, root);
        s.leader = hud_->image(context_, root);
        for (auto& head : s.head) head = hud_->image(context_, root);
        for (size_t k = 0; k < extra_arrows; ++k) {
            s.more_leader[k] = hud_->image(context_, root);
            for (auto& head : s.more_head[k]) head = hud_->image(context_, root);
            for (CssxLayer l : {s.more_leader[k], s.more_head[k][0], s.more_head[k][1]}) { paint(l, content().callouts.trace); if (l) hud_->set_pivot(context_, l, 0, 0.5f); }
        }
        s.text = hud_->text(context_, root);
        paint(s.bar, content().callouts.accent);
        for (CssxLayer l : {s.shelf, s.leader, s.head[0], s.head[1]}) { paint(l, content().callouts.trace); if (l) hud_->set_pivot(context_, l, 0, 0.5f); }
        paint(s.text, content().callouts.ink);
        s.shown.clear(); s.side = -1;
    }
    caption_accent_ = hud_->image(context_, root);
    caption_ = hud_->text(context_, root);
    paint(caption_bar_, content().callouts.caption_background);
    paint(caption_accent_, content().callouts.accent);
    paint(caption_, content().callouts.ink);
    if (face_) face_->justify(caption_, 0);
    sign_accent_ = hud_->image(context_, root);
    paint(sign_accent_, content().callouts.accent);
    for (size_t i = 0; i < max_signature_lines; ++i) {
        sign_[i] = hud_->text(context_, root);
        paint(sign_[i], i == 0 ? content().callouts.ink : content().callouts.faint);
        if (face_) face_->justify(sign_[i], 0);
    }
    caption_shown_.clear(); sign_source_.clear(); sign_fitted_.clear(); sign_width_ = -1;
    scale_ = 0;   // fonts are set on the first update, for the frame it brings
    built_ = true; visible_ = true;
}

void Callouts::refresh_style() {
    if (!hud_ || !built_) return;
    auto paint = [&](CssxLayer l, const auto& c) { if (l) hud_->set_color(context_, l, float(c[0]), float(c[1]), float(c[2]), float(c[3])); };
    const auto& c = content().callouts;
    for (auto& s : slots_) {
        paint(s.bar, c.accent); paint(s.text, c.ink);
        for (CssxLayer l : {s.shelf, s.leader, s.head[0], s.head[1]}) paint(l, c.trace);
        for (size_t k = 0; k < extra_arrows; ++k)
            for (CssxLayer l : {s.more_leader[k], s.more_head[k][0], s.more_head[k][1]}) paint(l, c.trace);
        s.shown.clear();
    }
    paint(caption_bar_, c.caption_background); paint(caption_accent_, c.accent); paint(caption_, c.ink);
    paint(sign_accent_, c.accent);
    for (size_t i = 0; i < max_signature_lines; ++i) paint(sign_[i], i == 0 ? c.ink : c.faint);
    caption_shown_.clear(); sign_source_.clear(); sign_fitted_.clear(); sign_width_ = -1;
    scale_ = 0;
}

// Font sizes follow the frame: set again only when the frame's scale changes.
void Callouts::restyle(double scale) {
    if (std::abs(scale - scale_) < 0.01) return;
    scale_ = scale;
    auto face = [&](CssxLayer text, double size, int spacing) {
        if (!text) return;
        if (face_) face_->apply(text, size * scale, spacing); else hud_->set_font(context_, text, float(size * scale));
    };
    for (auto& s : slots_) face(s.text, callout_font, content().callouts.label_spacing);
    face(caption_, content().callouts.caption_font, content().callouts.caption_spacing);
    for (size_t i = 0; i < max_signature_lines; ++i) face(sign_[i], i == 0 ? content().callouts.name_font : content().callouts.sign_font, i == 0 ? content().callouts.name_spacing : content().callouts.sign_spacing);
    sign_scale_ = 0;   // the signature's own line breaks depend on the scale too
}

// A thin image from (x0, y0) to (x1, y1): laid out flat from its start, then turned about it.
void Callouts::line(CssxLayer layer, double x0, double y0, double x1, double y1, float thickness, int z) {
    if (!layer) return;
    const double dx = x1 - x0, dy = y1 - y0, length = std::hypot(dx, dy);
    hud_->set_rect(context_, layer, float(x0), float(y0 - thickness / 2), float(length), thickness, 0, 0, z);
    hud_->set_angle(context_, layer, float(std::atan2(dy, dx) * 180 / std::numbers::pi));
    hud_->set_visible(context_, layer, length > 1 ? 1 : 0);
}

void Callouts::box(CssxLayer layer, double x, double y, double w, double h, int z) {
    if (!layer) return;
    hud_->set_rect(context_, layer, float(x), float(y), float(w), float(h), 0, 0, z);
    hud_->set_visible(context_, layer, 1);
}

void Callouts::hide() {
    if (!built_ || !visible_) return;
    for (auto& s : slots_) {
        for (CssxLayer l : {s.text, s.bar, s.shelf, s.leader, s.head[0], s.head[1]}) off(l);
        for (size_t k = 0; k < extra_arrows; ++k) for (CssxLayer l : {s.more_leader[k], s.more_head[k][0], s.more_head[k][1]}) off(l);
    }
    for (CssxLayer l : {caption_, caption_bar_, caption_accent_, sign_accent_}) off(l);
    for (CssxLayer l : sign_) off(l);
    visible_ = false;
}

void Callouts::forget() {
    built_ = visible_ = false;
    for (auto& s : slots_) s = Slot{};
    caption_ = caption_bar_ = caption_accent_ = sign_accent_ = 0;
    for (auto& l : sign_) l = 0;
    caption_shown_.clear(); caption_text_.clear(); sign_source_.clear(); sign_fitted_.clear();
    sign_width_ = -1; scale_ = 0; sign_scale_ = 0;
}

void Callouts::update(const CssxFrame& frame_data, const Rect& frame, const std::vector<CalloutTarget>& targets,
                      const std::string& caption, const std::vector<std::string>& signature, double line_x) {
    if (!hud_) return;
    if (frame_data.world_generation != world_) { forget(); world_ = frame_data.world_generation; }
    if (!frame_data.world_ready) return;
    // Once a second: are our layers still there? A host that lost them gets them rebuilt.
    if (built_ && ++checked_ >= 60) { checked_ = 0; if (caption_bar_ && !hud_->layer_object(context_, caption_bar_)) forget(); }
    if (targets.empty() && caption.empty() && signature.empty()) { hide(); return; }
    if (!(frame.w > 100) || !(frame.h > 100)) { hide(); return; }
    build();
    if (!built_) return;
    visible_ = true;
    const double scale = frame_scale(frame);
    restyle(scale);
    const double line_height = callout_line * scale, left = frame.x + frame.w * content().callouts.margin;

    auto places = layout_callouts(frame, targets, line_x);
    // Half-unit steps: sub-pixel jitter in a still shot sends nothing to the HUD.
    for (auto& p : places) {
        p.to_x = std::round(p.to_x * 2) / 2; p.to_y = std::round(p.to_y * 2) / 2;
        for (auto& [x, y] : p.more_to) { x = std::round(x * 2) / 2; y = std::round(y * 2) / 2; }
    }
    for (size_t i = 0; i < max_callouts; ++i) {
        auto& s = slots_[i];
        if (i >= places.size()) {
            for (CssxLayer l : {s.text, s.bar, s.shelf, s.leader, s.head[0], s.head[1]}) off(l);
            for (size_t k = 0; k < extra_arrows; ++k) for (CssxLayer l : {s.more_leader[k], s.more_head[k][0], s.more_head[k][1]}) off(l);
            continue;
        }
        const auto& p = places[i];
        const auto& text = targets[p.target].text;
        if (s.text) {
            if (text != s.shown) { s.shown = text; const auto upper = readout(text); hud_->set_text(context_, s.text, upper.data(), upper.size()); }
            // Text hugs the accent bar: left-aligned in the left column, right-aligned in the right.
            const int side = p.on_left ? 0 : 2;
            if (side != s.side) { s.side = side; if (face_) face_->justify(s.text, side); }
            box(s.text, p.label_x, p.label_y, p.width, line_height, z_order + 1);
        }
        // Accent bar at the label's outer end, shelf under it, leader from the shelf's inner end.
        box(s.bar, p.on_left ? p.label_x + content().callouts.accent_left * scale : p.label_x + p.width + content().callouts.accent_right * scale, p.label_y + content().callouts.accent_y * scale, content().callouts.accent_width * scale, line_height - content().callouts.accent_inset * scale, z_order);
        box(s.shelf, p.label_x, p.from_y - content().callouts.shelf_width / 2, p.width, content().callouts.shelf_width, z_order);
        // Arrowhead: two strokes back from the tip, at the style's spread angle.
        auto arrow = [&](CssxLayer leader, CssxLayer* heads, double to_x, double to_y) {
            line(leader, p.from_x, p.from_y, to_x, to_y, float(content().callouts.leader_width), z_order);
            const double angle = std::atan2(p.from_y - to_y, p.from_x - to_x), reach = content().callouts.arrow_length * scale;
            for (int k = 0; k < 2; ++k) {
                const double a = angle + (k ? content().callouts.arrow_angle : -content().callouts.arrow_angle);
                line(heads[k], to_x, to_y, to_x + std::cos(a) * reach, to_y + std::sin(a) * reach, float(content().callouts.arrow_width), z_order);
            }
        };
        arrow(s.leader, s.head, p.to_x, p.to_y);
        // The label's further arrows, from the same shelf end; unused ones are off.
        for (size_t k = 0; k < extra_arrows; ++k) {
            if (k < p.more_to.size()) arrow(s.more_leader[k], s.more_head[k], p.more_to[k].first, p.more_to[k].second);
            else for (CssxLayer l : {s.more_leader[k], s.more_head[k][0], s.more_head[k][1]}) off(l);
        }
    }

    // Caption: top left of the frame.
    if (caption.empty()) { for (CssxLayer l : {caption_, caption_bar_, caption_accent_}) off(l); }
    else {
        if (caption != caption_shown_) {
            caption_shown_ = caption;
            caption_text_ = readout(caption);
            if (caption_) hud_->set_text(context_, caption_, caption_text_.data(), caption_text_.size());
        }
        const double height = content().callouts.caption_height * scale, y = frame.y + frame.h * content().callouts.caption_y;
        const double width = std::min(text_width(caption_text_.size(), content().callouts.caption_font * scale, content().callouts.caption_spacing) + content().callouts.caption_padding * scale, frame.w * (1 - 2 * content().callouts.margin));
        box(caption_bar_, left, y, width, height, z_order);
        box(caption_accent_, left, y, content().callouts.caption_accent * scale, height, z_order + 1);
        box(caption_, left + content().callouts.caption_text_x * scale, y + content().callouts.caption_text_y * scale, width - content().callouts.caption_inset * scale, content().callouts.caption_text_height * scale, z_order + 1);
    }

    // Signature: bottom left of the frame, the name large, the rest small print under it.
    if (signature.empty()) { off(sign_accent_); for (CssxLayer l : sign_) off(l); }
    else {
        const double room = frame.w * (1 - 2 * content().callouts.margin) - content().callouts.signature_inset * scale;
        if (signature != sign_source_ || std::abs(room - sign_width_) > 0.5 || std::abs(scale - sign_scale_) > 0.005) {
            sign_source_ = signature; sign_width_ = room; sign_scale_ = scale;
            sign_fitted_ = fit_signature(signature, room, scale);
            for (size_t i = 0; i < max_signature_lines; ++i)
                if (sign_[i] && i < sign_fitted_.size()) hud_->set_text(context_, sign_[i], sign_fitted_[i].data(), sign_fitted_[i].size());
        }
        const double name_height = content().callouts.name_height * scale, small_height = content().callouts.sign_height * scale;
        const double total = name_height + small_height * double(sign_fitted_.size() > 0 ? sign_fitted_.size() - 1 : 0);
        double y = frame.y + frame.h * content().callouts.signature_bottom - total;
        box(sign_accent_, left, y + content().callouts.signature_accent_y * scale, content().callouts.signature_accent_width * scale, total - content().callouts.signature_accent_inset * scale, z_order);
        for (size_t i = 0; i < max_signature_lines; ++i) {
            if (i >= sign_fitted_.size()) { off(sign_[i]); continue; }
            const double height = i == 0 ? name_height : small_height;
            box(sign_[i], left + content().callouts.signature_inset * scale, y, room, height, z_order + 1);
            y += height;
        }
    }
}

} // namespace cine
