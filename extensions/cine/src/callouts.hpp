#pragma once
// Callouts: a text label and an arrow for each component a breakdown shows, plus a caption
// naming the layers on screen. Drawn with the CSSX HUD's retained layers (text, and thin
// rotated images for the leader and the arrowhead), so the only per-frame work is moving
// them. The layout is pure and tested: labels sit in two columns at the screen edges, ordered
// like their targets so leaders never cross.
#include <cssx/hud.h>
#include <string>
#include <vector>

namespace cine {

// Where a callout points, in the HUD's logical pixels. `visible` is false when the target is
// behind the camera or off screen: its label is dropped for that frame.
struct CalloutTarget { std::string text; double x = 0, y = 0; bool visible = false; };
// One placed callout: the label's top-left corner, and the leader from the label to the target.
struct CalloutPlace { size_t target = 0; double label_x = 0, label_y = 0, from_x = 0, from_y = 0, to_x = 0, to_y = 0; };

constexpr size_t max_callouts = 6;
constexpr double callout_font = 17, callout_line = 26;
// Rough width of a label, for where its leader starts (the HUD does not report text sizes).
double label_width(const std::string& text);
// Pure: at most max_callouts placements for the visible targets.
std::vector<CalloutPlace> layout_callouts(double viewport_w, double viewport_h, const std::vector<CalloutTarget>& targets);

class Callouts {
public:
    void attach(const CssxHudApi* hud, void* context) { hud_ = hud; context_ = context; }
    void update(const CssxFrame& frame, const std::vector<CalloutTarget>& targets, const std::string& caption);
    void hide();
    void forget();   // the world changed: the host dropped our layers
private:
    struct Slot { CssxLayer text = 0, leader = 0, head[2]{}; std::string shown; };
    void build();
    void line(CssxLayer layer, double x0, double y0, double x1, double y1, float thickness, int z);
    const CssxHudApi* hud_ = nullptr;
    void* context_ = nullptr;
    uint32_t world_ = 0;
    bool built_ = false, visible_ = false;
    Slot slots_[max_callouts];
    CssxLayer caption_ = 0, caption_bar_ = 0;
    std::string caption_shown_;
};

} // namespace cine
