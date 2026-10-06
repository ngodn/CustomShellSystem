#include "catalog.hpp"
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>

namespace tuner {
namespace {
std::string lower(std::string s) { for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c))); return s; }
bool starts(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }
}

bool valid_cvar_name(const std::string& name) {
    if (name.size() < 2 || name.size() > 96) return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '.' || c == '_'; });
}

std::string forbidden_reason(const std::string& cvar) {
    const auto n = lower(cvar);
    if (starts(n, "sg.")) return "scalability groups are the game's own quality levels";
    if (n == "r.screenpercentage" || starts(n, "r.screenpercentage.") || starts(n, "r.dynamicres.") || n == "r.setres" || n == "r.resolutionquality")
        return "resolution and screen percentage are picture quality";
    if (n == "t.maxfps" || n == "r.vsync" || n == "rhi.syncinterval" || n == "r.vsynceditor" || n == "t.overridefps")
        return "frame caps and v-sync are the player's choice in video settings";
    if (n == "r.mipmaplodbias" || n == "r.streaming.mipbias") return "a mip bias blurs textures";
    if (n == "r.materialqualitylevel" || n == "r.detailmode" || n == "r.viewdistancescale" || n == "r.skeletalmeshlodbias" || n == "r.staticmeshlodbias")
        return "quality levels and LOD biases change what is drawn";
    if (starts(n, "r.ngx.") || starts(n, "r.streamline.dlssg") || starts(n, "r.fidelityfx.")) return "the upscaler is the player's choice in video settings";
    if (n == "r.lumen.diffuseindirect.allow" || n == "r.lumen.reflections.allow" || n == "r.dynamicglobalilluminationmethod" || n == "r.reflectionmethod")
        return "turning lighting features off is a picture change";
    if (n == "r.shadow.virtual.enable" || n == "r.shadowquality" || n == "r.shadow.virtual.maxphysicalpages")
        return "shadow features and the shadow page pool change the picture (and the pool resize crashed this GPU once)";
    if (n == "r.nanite" || n == "r.nanite.projectenabled" || n == "r.nanite.maxpixelsperedge") return "Nanite detail is picture quality";
    return "";
}

std::string console_number(double value) {
    char text[32];
    if (std::fabs(value - std::round(value)) < 1e-9 && std::fabs(value) < 1e15) std::snprintf(text, sizeof text, "%.0f", value);
    else std::snprintf(text, sizeof text, "%.6f", value);
    std::string s = text;
    if (s.find('.') != std::string::npos) { while (s.ends_with('0')) s.pop_back(); if (s.ends_with('.')) s.pop_back(); }
    return s;
}

double Setting::resolve(double vram_mb) const {
    if (vram_fraction <= 0 || vram_mb <= 0) return value;
    return std::round(std::clamp(vram_mb * vram_fraction, vram_min, vram_max));
}

Catalog Catalog::parse(const Json& file) {
    if (!file.is_object() || file.value("schema", 0) != 1) throw std::invalid_argument("tweaks.json: 'schema' must be 1");
    const auto& list = file.contains("tweaks") ? file.at("tweaks") : Json();
    if (!list.is_array() || list.empty()) throw std::invalid_argument("tweaks.json: 'tweaks' must be a non-empty list");
    if (list.size() > 64) throw std::invalid_argument("tweaks.json: at most 64 tweaks");
    Catalog c;
    std::set<std::string> ids, cvars;
    for (const auto& j : list) {
        if (!j.is_object()) throw std::invalid_argument("tweaks.json: every tweak must be an object");
        Tweak t;
        t.id = j.value("id", std::string());
        const std::string where = "tweak '" + t.id + "': ";
        if (t.id.size() < 2 || t.id.size() > 48 || !std::all_of(t.id.begin(), t.id.end(), [](unsigned char ch) { return std::islower(ch) || std::isdigit(ch) || ch == '_'; }))
            throw std::invalid_argument(where + "'id' must be 2 to 48 lowercase letters, digits or underscores");
        if (!ids.insert(t.id).second) throw std::invalid_argument(where + "duplicate id");
        t.title = j.value("title", std::string());
        if (t.title.empty() || t.title.size() > 48) throw std::invalid_argument(where + "'title' must be 1 to 48 characters");
        t.group = j.value("group", std::string("Tweaks"));
        if (t.group.empty() || t.group.size() > 32) throw std::invalid_argument(where + "'group' must be 1 to 32 characters");
        t.what = j.value("what", std::string());
        if (t.what.empty() || t.what.size() > 600) throw std::invalid_argument(where + "'what' must say in 1 to 600 characters what changes and why it costs nothing visible");
        t.side = j.value("side", std::string("gpu"));
        if (t.side != "gpu" && t.side != "cpu" && t.side != "both") throw std::invalid_argument(where + "'side' must be gpu, cpu or both");
        t.picture = j.value("picture", std::string("none"));
        if (t.picture != "none" && t.picture != "check") throw std::invalid_argument(where + "'picture' must be none or check; a tweak that lowers the picture does not belong here");
        if (j.contains("on")) { if (!j.at("on").is_boolean()) throw std::invalid_argument(where + "'on' must be true or false"); t.on = j.at("on").get<bool>(); }
        auto names = [&](const char* key, std::initializer_list<const char*> allowed) {
            std::vector<std::string> out;
            if (!j.contains(key)) return out;
            if (!j.at(key).is_array()) throw std::invalid_argument(where + "'" + key + "' must be a list");
            for (const auto& v : j.at(key)) {
                const auto s = v.is_string() ? v.get<std::string>() : std::string();
                if (std::find(allowed.begin(), allowed.end(), s) == allowed.end()) throw std::invalid_argument(where + "'" + key + "' has an unknown entry '" + s + "'");
                out.push_back(s);
            }
            return out;
        };
        t.platforms = names("platform", {"windows", "proton"});
        t.gpus = names("gpu", {"nvidia", "amd", "intel"});
        if (j.contains("min_vram_mb")) {
            if (!j.at("min_vram_mb").is_number() || j.at("min_vram_mb").get<double>() < 0) throw std::invalid_argument(where + "'min_vram_mb' must be a number of megabytes");
            t.min_vram_mb = j.at("min_vram_mb").get<double>();
        }
        const auto& set = j.contains("set") ? j.at("set") : Json();
        if (!set.is_array() || set.empty() || set.size() > 16) throw std::invalid_argument(where + "'set' must list 1 to 16 console variables");
        for (const auto& s : set) {
            if (!s.is_object()) throw std::invalid_argument(where + "every 'set' entry must be an object with 'cvar' and 'value'");
            Setting one;
            one.cvar = s.value("cvar", std::string());
            if (!valid_cvar_name(one.cvar)) throw std::invalid_argument(where + "'" + one.cvar + "' is not a console variable name");
            if (const auto why = forbidden_reason(one.cvar); !why.empty()) throw std::invalid_argument(where + one.cvar + ": " + why);
            if (!s.contains("value") || !s.at("value").is_number()) throw std::invalid_argument(where + one.cvar + " needs a numeric 'value'");
            one.value = s.at("value").get<double>();
            if (!std::isfinite(one.value)) throw std::invalid_argument(where + one.cvar + ": value must be finite");
            if (s.contains("from_vram")) {
                const auto& v = s.at("from_vram");
                if (!v.is_object() || !v.value("fraction", Json()).is_number() || !v.value("min", Json()).is_number() || !v.value("max", Json()).is_number())
                    throw std::invalid_argument(where + one.cvar + ": 'from_vram' needs numeric 'fraction', 'min' and 'max'");
                one.vram_fraction = v.at("fraction").get<double>(); one.vram_min = v.at("min").get<double>(); one.vram_max = v.at("max").get<double>();
                if (one.vram_fraction <= 0 || one.vram_min < 0 || one.vram_max < one.vram_min) throw std::invalid_argument(where + one.cvar + ": 'from_vram' values are out of order");
            }
            if (!cvars.insert(lower(one.cvar)).second) throw std::invalid_argument(where + one.cvar + " is already set by another tweak");
            t.set.push_back(std::move(one));
        }
        c.tweaks.push_back(std::move(t));
    }
    return c;
}

std::string unfit_reason(const Tweak& t, const Machine& m) {
    if (!t.platforms.empty() && std::find(t.platforms.begin(), t.platforms.end(), m.platform()) == t.platforms.end())
        return m.wine ? "For Windows only; this game runs through Proton here" : "For Proton only; this is Windows";
    if (!t.gpus.empty()) {
        if (m.gpu_vendor.empty()) return "Needs a known graphics card maker; the adapter could not be read";
        if (std::find(t.gpus.begin(), t.gpus.end(), m.gpu_vendor) == t.gpus.end()) {
            std::string list; for (const auto& g : t.gpus) list += (list.empty() ? "" : ", ") + (g == "nvidia" ? std::string("NVIDIA") : g == "amd" ? "AMD" : "Intel");
            return "For " + list + " cards; this is " + (m.gpu_vendor == "nvidia" ? "NVIDIA" : m.gpu_vendor == "amd" ? "AMD" : "Intel");
        }
    }
    if (t.min_vram_mb > 0 && m.vram_mb > 0 && m.vram_mb < t.min_vram_mb)
        return "Needs " + console_number(std::round(t.min_vram_mb / 1024)) + " GB of video memory or more; this card has " + console_number(std::round(m.vram_mb / 1024)) + " GB";
    if (m.steam_deck && (t.min_vram_mb > 0 || t.side != "cpu")) return "Not for Steam Deck: its memory is shared with the CPU and its graphics settings are pinned by the game's Deck profile";
    return "";
}

const Tweak* Catalog::find(const std::string& id) const {
    for (const auto& t : tweaks) if (t.id == id) return &t;
    return nullptr;
}
}
