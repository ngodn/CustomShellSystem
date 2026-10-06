#pragma once
// The tweak catalog: what the tuner may change, read from tweaks.json in the extension
// folder. Pure data and validation, unit-tested off-game.
//
// The one goal of the tuner is more frames per second with no visible loss of picture
// quality, so the catalog cannot carry anything that lowers the picture by construction:
// no resolution or screen percentage, no scalability group, no frame cap, no quality level.
// Those are the player's own choices in the game's video settings and stay there.
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace tuner {
using Json = nlohmann::json;

// One console variable and the value the tweak sets it to. Values are numbers: every
// variable the tuner touches is a number, and a number reads back for restoring.
struct Setting {
    std::string cvar; double value = 0;
    // Optional: the value scales with the card's memory, value = clamp(vram_mb * fraction, min, max);
    // `value` is used when the memory could not be read.
    double vram_fraction = 0, vram_min = 0, vram_max = 0;
    double resolve(double vram_mb) const;
};

struct Tweak {
    std::string id, title, group;
    std::string what;          // plain words: what changes and why it costs nothing visible
    std::string side = "gpu";  // "gpu", "cpu" or "both": where the time is saved
    std::vector<Setting> set;
    bool on = true;            // on by default when the tuner is enabled
    // "none": the picture is the same by construction. "check": the same in practice, but
    // worth a look (the tuner offers a side-by-side screenshot). The catalog refuses
    // anything else: a tweak that is known to lower the picture is not a tweak.
    std::string picture = "none";
    // Where the tweak applies; empty means everywhere. platform: "windows", "proton"
    // (Wine); gpu: "nvidia", "amd", "intel". A tweak for another setup is shown, disabled,
    // with the reason.
    std::vector<std::string> platforms, gpus;
    double min_vram_mb = 0;    // offered only on cards with at least this much dedicated memory
};

// What the tuner found out about the machine, once, at start.
struct Machine {
    bool wine = false;             // Proton or another Wine
    std::string wine_version;
    std::string gpu_name;          // the adapter the game draws with
    std::string gpu_vendor;        // "nvidia", "amd", "intel" or "" when unknown
    double vram_mb = 0;            // dedicated video memory, 0 when unknown
    bool steam_deck = false;
    std::string platform() const { return wine ? "proton" : "windows"; }
};
// Why the tweak does not fit this machine, or empty when it does.
std::string unfit_reason(const Tweak& t, const Machine& m);

struct Catalog {
    std::vector<Tweak> tweaks;
    // Parse tweaks.json. Throws std::invalid_argument naming the tweak and the problem.
    static Catalog parse(const Json& file);
    const Tweak* find(const std::string& id) const;
};

// Why a console variable may never appear in the catalog, or empty when it may.
std::string forbidden_reason(const std::string& cvar);
// A console variable name: letters, digits, dots and underscores, 2 to 96 characters.
bool valid_cvar_name(const std::string& name);
// The number as the console takes it: "0", "1", "0.5", "2048", never "1e+03".
std::string console_number(double value);
}
