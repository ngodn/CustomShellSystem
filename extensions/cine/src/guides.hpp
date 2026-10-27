#pragma once
// On-screen helpers drawn with the CSSX HUD (retained layers): the frame guide (dark bars
// outside the chosen aspect plus a thin border), one help line, and the countdown. They are
// for framing only: everything except the countdown is hidden while a take runs, and the
// countdown is gone before the take starts. Per-frame cost: a few epsilon-gated setters.
#include "callouts.hpp"
#include <cssx/hud.h>
#include <string>

namespace cine {

enum class Guide { Off, Wide, Tall, Square, Scope };   // Legacy ordinals; other catalog entries use further values.
Guide guide_from(const std::string& id);
const char* guide_id(Guide);
double guide_aspect(Guide);
std::vector<std::string> guide_ids();
Json guide_options();

struct Overlay {
    bool visible = false;      // anything CINE draws at all
    Guide guide = Guide::Off;
    std::string help;          // empty hides the line
    std::string countdown;     // empty hides it
};

// Pure: where the opening of an aspect sits inside a viewport (for tests and for drawing).
Rect guide_opening(double viewport_w, double viewport_h, double aspect);

class Guides {
public:
    void attach(const CssxHudApi* hud, void* context, Typeface* face) { hud_ = hud; context_ = context; face_ = face; }
    void update(const CssxFrame& frame, const Overlay& overlay);
    void hide();
    void refresh_style();
    void forget();             // the world changed: the host dropped our layers
private:
    void build();
    void show(bool on);
    const CssxHudApi* hud_ = nullptr;
    void* context_ = nullptr;
    uint32_t world_ = 0;
    bool built_ = false, shown_ = false;
    CssxLayer bars_[4]{}, border_[4]{}, help_ = 0, count_ = 0;
    Typeface* face_ = nullptr;
    int checked_ = 0;
    std::string help_text_, count_text_;
};

} // namespace cine
