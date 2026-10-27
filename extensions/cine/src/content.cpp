#include "content.hpp"
#include "core_embedded.hpp"
#include "layers.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <stdexcept>

namespace cine {
#include "core_decode.inc"
namespace {

[[noreturn]] void bad(const std::string& path, const std::string& reason) {
    throw std::invalid_argument(path + ": " + reason);
}
void validate(const Json& value, const Json& schema, const std::string& path) {
    const auto type = schema.at("type").get<std::string>();
    if (type == "object") {
        if (!value.is_object()) bad(path, "expected an object");
        const auto& fields = schema.at("fields");
        if (value.size() != fields.size()) bad(path, "missing or unknown field");
        for (const auto& [key, field] : fields.items()) {
            if (!value.contains(key)) bad(path + "." + key, "required");
            validate(value.at(key), field, path + "." + key);
        }
    } else if (type == "array") {
        if (!value.is_array() || value.size() < schema.at("min").get<size_t>() || value.size() > schema.at("max").get<size_t>()) bad(path, "invalid array length");
        for (const auto& item : value) validate(item, schema.at("items"), path + "[]");
    } else if (type == "number" || type == "integer") {
        if (!value.is_number() || (type == "integer" && !value.is_number_integer())) bad(path, "expected " + type);
        const double number = value.get<double>();
        if (!std::isfinite(number) || number < schema.at("min").get<double>() || number > schema.at("max").get<double>()) bad(path, "number outside supported range");
    } else if (type == "string") {
        if (!value.is_string()) bad(path, "expected text");
        const auto& text = value.get_ref<const std::string&>();
        if (text.size() < schema.at("min").get<size_t>() || text.size() > schema.at("max").get<size_t>() || text.find('\0') != std::string::npos) bad(path, "invalid text length or embedded NUL");
    } else if (type == "boolean" && !value.is_boolean()) bad(path, "expected a boolean");
}
bool object_path(const std::string& s) {
    return s.starts_with("/Game/CSSX/CINE/") && s.find("..") == std::string::npos &&
           s.find('.') != std::string::npos && s.find_first_of("\\\r\n\t ") == std::string::npos;
}
void relationships(const CoreContent& c) {
    if (c.materials.root + "/Core/DT_CINE_Core.DT_CINE_Core" != core_table_path) bad("style.materials.root", "core package root is fixed by the loader ABI");
    if (c.action.framing != "follow" && c.action.framing != "duel" && c.action.framing != "shoulder") bad("recipe.action.framing", "unknown framing");
    if (c.action.distance_max < c.action.distance) bad("recipe.action.distance_max", "less than distance");
    if (c.motion.split_min >= c.motion.split_max) bad("recipe.motion", "reversed split bounds");
    const auto& b = c.breakdown;
    for (const auto* path : {&b.collection, &b.wire, &b.clay, &b.xray, &b.buffer_post, &c.callouts.font_asset})
        if (!object_path(*path)) bad("assets", "expected a CINE object path");
    // Recovery recognizes our MID names. Keep this contract until ownership is identity-based.
    for (const auto* path : {&b.wire, &b.clay, &b.xray, &b.buffer_post})
        if (path->find("_Cine_") == std::string::npos) bad("assets", "material name must retain _Cine_ ownership marker");
    if (c.callouts.font_file.starts_with('/') || c.callouts.font_file.find("..") != std::string::npos || c.callouts.font_file.find_first_of(":\\") != std::string::npos)
        bad("style.callouts.font_file", "expected an extension-relative path");
    if (c.geometry.stick_min >= c.geometry.stick_max || c.geometry.radius_min > c.geometry.radius_max) bad("style.geometry", "reversed bounds");
    if (c.callouts.scale_min > c.callouts.scale_max || c.callouts.row_top > c.callouts.row_bottom) bad("style.callouts", "reversed bounds");
    if (size_t(c.callouts.signature_skip) >= c.callouts.signature_break.size()) bad("style.callouts.signature_skip", "must retain part of the separator");
    std::set<std::string> ids;
    for (const auto& layer : b.layers) {
        if (!layer_from(layer.id) || !ids.insert(layer.id).second) bad("style.breakdown.layers", "unknown or repeated layer");
    }
    ids.clear();
    for (const auto& guide : c.guides.catalog) {
        if (guide.id == "off" || guide.id.size() > 16 || !ids.insert(guide.id).second) bad("style.guides.catalog", "invalid or repeated guide id");
    }
    for (size_t i = 0; i < c.body.targets.size(); ++i) {
        const auto& target = c.body.targets[i];
        if (target.bones.size() != target.weights.size() || (i == 0 && !target.bones.empty()) ||
            (i > 0 && (target.bones.empty() || std::abs(std::accumulate(target.weights.begin(), target.weights.end(), 0.0) - 1.0) > 1e-9)))
            bad("recipe.body.targets", "bones and normalized weights must match the subject roles");
    }
    for (const auto& profile : {c.picture.sharp, c.picture.full})
        for (size_t i = 0; i < profile.size(); ++i)
            if (profile[i] != -1e9 && (profile[i] < 0 || profile[i] > (i < 6 ? 64 : 200))) bad("recipe.picture", "invalid target or work limit exceeded");
    if (c.motion.plan_turns.front() != 0 || c.motion.start_speed > c.motion.walk_speed) bad("recipe.motion", "invalid initial turn or speed");
    if (c.palettes.start + c.palettes.span > 1 || std::find(c.palettes.exclude.begin(), c.palettes.exclude.end(), "original") == c.palettes.exclude.end()) bad("recipe.palettes", "invalid cycle span or missing original exclusion");
    double previous = -1;
    for (const auto& step : c.reel.steps) {
        if (step.at <= previous) bad("recipe.reel.steps", "must be in time order");
        previous = step.at;
        (void)layers_from(step.layers);
    }
    const auto cover = layers_from(c.transitions.cover);
    if (!cover || (cover & bit(Layer::Lighting)) || ((cover & bit(Layer::Clay)) && (cover & bit(Layer::Wireframe)))) bad("recipe.transitions.cover", "invalid cover layers");
    for (const auto* reference : {&c.defaults.walk_reference, &c.defaults.pose_reference})
        if (*reference != "head" && *reference != "capsule") bad("recipe.defaults.reference", "unknown reference");
    if (c.defaults.ease != "linear" && c.defaults.ease != "in" && c.defaults.ease != "out" && c.defaults.ease != "in_out" && c.defaults.ease != "whip" && c.defaults.ease != "smooth") bad("recipe.defaults.ease", "unknown easing");
}
CoreContent& current() { static CoreContent value = parse_content(default_content_rows()); return value; }
}

const Json& default_content_rows() { static const Json rows = Json::parse(core_default_json); return rows; }
const Json& content_schema() { static const Json schema = Json::parse(core_schema_json); return schema; }
void validate_content_row(const std::string& name, const Json& row) {
    const auto it = content_schema().find(name);
    if (it == content_schema().end()) bad(name, "unknown core row");
    validate(row, *it, name);
}
CoreContent parse_content(const Json& rows) {
    if (!rows.is_object() || rows.size() != content_schema().size()) bad("core", "missing or unknown row");
    for (const auto& [name, schema] : content_schema().items()) {
        (void)schema;
        if (!rows.contains(name)) bad(name, "required");
        validate_content_row(name, rows.at(name));
    }
    auto out = decode_content(rows);
    auto fold = [](std::string& value) { for (auto& c : value) c = char(std::tolower(static_cast<unsigned char>(c))); };
    for (auto& group : out.bones.spring_groups) for (auto& prefix : group.prefixes) fold(prefix);
    fold(out.bones.virtual_prefix); fold(out.bones.ik_prefix);
    for (auto& prefix : out.bones.helper_prefixes) fold(prefix);
    for (auto& part : out.bones.helper_contains) fold(part);
    for (auto& bone : out.annotations.head) fold(bone);
    for (auto& bone : out.annotations.torso) fold(bone);
    for (auto& bone : out.annotations.physics_bones) fold(bone);
    for (auto& group : out.annotations.socket_groups) for (auto& part : group.contains) fold(part);
    relationships(out);
    return out;
}
ContentOverride resolve_content(const Json& core, const std::map<std::string, Json>& rows) {
    Json merged = core;
    ContentOverride out{parse_content(core), {}};
    for (const auto& [name, patch] : rows) {
        if (!name.starts_with("style.") && !name.starts_with("recipe.")) continue;
        try {
            if (!content_schema().contains(name)) bad(name, "unknown core row");
            if (!patch.is_object()) bad(name, "expected an object");
            // Objects merge by field; arrays replace whole lists. Null never deletes a field.
            const auto merge = [&](auto&& self, Json& target, const Json& edit) -> void {
                for (const auto& [key, value] : edit.items()) {
                    if (!target.contains(key)) bad(name + "." + key, "unknown field");
                    if (value.is_object() && target.at(key).is_object()) self(self, target[key], value);
                    else target[key] = value;
                }
            };
            auto candidate = merged;
            merge(merge, candidate[name], patch);
            auto value = parse_content(candidate);
            merged = std::move(candidate);
            out.value = std::move(value);
        } catch (const std::exception& e) { out.problems.push_back(name + ": " + e.what()); }
    }
    return out;
}
const CoreContent& content() { return current(); }
void reset_content() { current() = parse_content(default_content_rows()); }
void install_content(CoreContent value) { current() = std::move(value); }
std::string annotation(const std::string& pattern, size_t count, const std::string& group) {
    std::string out = pattern;
    for (const auto& [key, value] : {std::pair<std::string, std::string>{"{count}", std::to_string(count)}, {"{group}", group}}) {
        size_t at = 0;
        while ((at = out.find(key, at)) != std::string::npos) { out.replace(at, key.size(), value); at += value.size(); }
    }
    return out;
}

bool ContentLoader::advance(const cssx::Client& host) {
    if (done_) return true;
    try {
        if (table_.is_null()) {
            table_ = host.request({{"op", "load"}, {"path", core_table_path}});
            if (!table_.is_object() || !table_.contains("$object")) throw std::runtime_error("core table is missing; install the current CINE asset pak");
            return false;
        }
        if (!listed_) {
            const auto names = host.request({{"op", "table.rows"}, {"target", table_}});
            if (!names.is_array() || names.size() != content_schema().size()) throw std::runtime_error("core table row set is incomplete or unsupported");
            std::set<std::string> unique;
            for (const auto& name : names) {
                if (!name.is_string() || !content_schema().contains(name.get<std::string>()) || !unique.insert(name.get<std::string>()).second) throw std::runtime_error("invalid core row name");
                names_.push_back(name.get<std::string>());
            }
            listed_ = true;
            return false;
        }
        const auto& name = names_.at(next_);
        const auto row = host.request({{"op", "table.row"}, {"target", table_}, {"row", name}});
        if (!row.is_object() || !row.contains("DevComment") || !row.at("DevComment").is_string() || row.at("DevComment").get_ref<const std::string&>().size() > 256 * 1024) throw std::runtime_error("invalid core row " + name);
        auto value = Json::parse(row.at("DevComment").get_ref<const std::string&>(), [](int depth, Json::parse_event_t, Json&) { if (depth > 16) throw std::invalid_argument("core JSON is nested too deeply"); return true; });
        validate_content_row(name, value);
        rows_[name] = std::move(value);
        if (++next_ < names_.size()) return false;
        auto parsed = parse_content(rows_);
        core_rows_ = std::move(rows_);
        install_content(std::move(parsed));
    } catch (const std::exception& e) { problem_ = e.what(); }
    table_ = nullptr; rows_.clear(); names_.clear(); done_ = true;
    return true;
}
}
