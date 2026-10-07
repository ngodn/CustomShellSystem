#pragma once
// Callouts: a label and an arrow for each component a breakdown shows, a caption naming the
// layers on screen, and the signature block. Drawn with the CSSX HUD's retained layers (text,
// and thin rotated images for lines), so the only per-frame work is moving the arrows.
//
// Everything is laid out inside a frame: the opening of the chosen frame guide (16:9, 9:16,
// 1:1, 2.39:1) or the whole viewport, so it survives the crop to that aspect. Sizes scale
// down with a narrow frame. The layout is pure and tested: labels sit in two columns at the
// frame's edges, ordered like their targets so leaders never cross; the caption is top left
// and the signature bottom left.
//
// The look is a technical readout: upper-case letter-spaced text in the extension's own
// typeface, an accent bar at the label's outer end, a shelf line under it, and the arrow
// leaving the shelf's inner end.
#include <cssx/client.hpp>
#include <cssx/hud.h>
#include <string>
#include <vector>

namespace cine {
using Json = nlohmann::json;

// A rectangle in HUD units.
struct Rect { double x = 0, y = 0, w = 0, h = 0; };

// Where a callout points, in HUD units. `visible` is false when the target is behind the
// camera: its label is dropped for that frame, as it is when the target is outside the frame.
struct CalloutTarget { std::string text; double x = 0, y = 0; bool visible = false; std::vector<std::pair<double, double>> more; };   // `more`: further arrows from the same label
// One placed callout: the label's box (top-left corner and width), which side it is on, and
// the leader from the shelf under the label to the target.
struct CalloutPlace { size_t target = 0; bool on_left = true; double label_x = 0, label_y = 0, width = 0, from_x = 0, from_y = 0, to_x = 0, to_y = 0;
                      std::vector<std::pair<double, double>> more_to; };   // the further arrows' tips, inside the frame (and left of a split line)

constexpr size_t max_callouts = 8, max_signature_lines = 6;
constexpr double callout_font = 18, callout_line = 26, callout_shelf = 3;
// 1 for a frame as wide as a 16:9 screen, smaller for narrow frames (never below 0.62).
double frame_scale(const Rect& frame);
// Room a label needs at a scale (the HUD does not report text sizes): upper-case, letter-spaced.
double label_width(const std::string& text, double scale = 1.0);
// Pure: at most max_callouts placements for the visible targets inside the frame. `line_x` is
// where a split breakdown's line stands (HUD units): the layers are left of it, so every
// label goes to the left column in a fixed row, and a target right of the line gets no label
// (one on the line itself, her spine or head, keeps its label). Past the frame: no split.
std::vector<CalloutPlace> layout_callouts(const Rect& frame, const std::vector<CalloutTarget>& targets, double line_x = 1e12);
// "Spring bones: chest" -> "SPRING BONES: CHEST" (ASCII; other bytes pass through).
std::string readout(const std::string& text);
// Signature lines fitted to a width: a line too long for it breaks before each " + ".
// The first line is the name (drawn large). At most max_signature_lines come back.
std::vector<std::string> fit_signature(const std::vector<std::string>& lines, double width, double scale);

// CINE's label typeface: the TTF in the extension's fonts folder, made into a font object in
// the game on first use (no cooked asset needed). Any failure leaves the HUD's own font.
class Typeface {
public:
    void attach(const cssx::Client* host, const CssxHudApi* hud, void* context) { host_ = host; hud_ = hud; context_ = context; }
    // Text layer `layer` in this face. Size in HUD units, letter spacing in 1/1000 em.
    void apply(CssxLayer layer, double size, int spacing);
    // 0 left, 1 centre, 2 right (the HUD centres text by default).
    void justify(CssxLayer layer, int side);
    void forget() { font_ = nullptr; failed_ = false; }   // the world changed: the object is gone
private:
    bool ensure();
    const cssx::Client* host_ = nullptr;
    const CssxHudApi* hud_ = nullptr;
    void* context_ = nullptr;
    Json font_;
    bool failed_ = false;   // one attempt per world
};

class Callouts {
public:
    void attach(const CssxHudApi* hud, void* context, Typeface* face) { hud_ = hud; context_ = context; face_ = face; }
    // `frame` is where to lay out (see above). Empty caption and signature hide those parts.
    void update(const CssxFrame& frame_data, const Rect& frame, const std::vector<CalloutTarget>& targets,
                const std::string& caption, const std::vector<std::string>& signature, double line_x = 1e12);
    void hide();
    void forget();   // the world changed: the host dropped our layers
private:
    static constexpr size_t extra_arrows = 3;   // beyond the first: four arrows a label
    struct Slot { CssxLayer text = 0, bar = 0, shelf = 0, leader = 0, head[2]{}; CssxLayer more_leader[extra_arrows]{}, more_head[extra_arrows][2]{}; std::string shown; int side = -1; };
    void build();
    void restyle(double scale);
    void line(CssxLayer layer, double x0, double y0, double x1, double y1, float thickness, int z);
    void box(CssxLayer layer, double x, double y, double w, double h, int z);
    void off(CssxLayer layer) { if (layer) hud_->set_visible(context_, layer, 0); }
    const CssxHudApi* hud_ = nullptr;
    void* context_ = nullptr;
    Typeface* face_ = nullptr;
    uint32_t world_ = 0;
    bool built_ = false, visible_ = false;
    int checked_ = 0;
    double scale_ = 0;               // the scale the fonts were last set for
    Slot slots_[max_callouts];
    CssxLayer caption_ = 0, caption_bar_ = 0, caption_accent_ = 0;
    CssxLayer sign_[max_signature_lines]{}, sign_accent_ = 0;
    std::string caption_shown_, caption_text_;
    std::vector<std::string> sign_source_, sign_fitted_;
    double sign_width_ = -1, sign_scale_ = 0;
};

} // namespace cine
