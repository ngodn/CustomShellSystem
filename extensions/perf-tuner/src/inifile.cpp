#include "inifile.hpp"
#include "catalog.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace tuner {
namespace {
std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    size_t i = 0; while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
std::string lower(std::string s) { for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c))); return s; }
bool starts(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }
// Sections whose name=value lines the engine turns into console variables or settings.
bool read_section(const std::string& section) {
    const auto s = lower(section);
    return s == "consolevariables" || s == "systemsettings" || starts(s, "/script/") || s == "texturestreaming"
        || s == "shaderpipelinecache" || s == "renderingthread" || s == "physics" || s == "core.system";
}
}

IniScan scan_engine_ini(const std::string& text) {
    IniScan scan;
    std::istringstream in(text);
    std::string raw, section;
    int number = 0;
    while (std::getline(in, raw)) {
        ++number;
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        auto line = trim(raw);
        if (line.empty()) continue;
        if (line[0] == ';' || line[0] == '#') {
            const auto l = lower(line);
            if (l.find("metadata=(diff=true") != std::string::npos) scan.written_by_game = true;
            if (scan.pack.empty()) {
                if (l.find("ultimate engine tweaks") != std::string::npos) scan.pack = "Ultimate Engine Tweaks (P40L0)";
                else if (l.find("koala") != std::string::npos || l.find("stutter fix") != std::string::npos) scan.pack = "UE4-5 Stutter Fix (KOALAaufPILLEN)";
            }
            continue;
        }
        if (line[0] == '[') { const auto end = line.find(']'); section = end == std::string::npos ? line.substr(1) : line.substr(1, end - 1); continue; }
        if (!read_section(section)) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        IniLine l;
        l.section = section; l.number = number;
        l.name = trim(line.substr(0, eq));
        auto value = line.substr(eq + 1);
        if (const auto c = value.find(';'); c != std::string::npos) value = value.substr(0, c);   // a trailing comment
        l.value = trim(value);
        if (!l.name.empty() && (l.name[0] == '+' || l.name[0] == '-' || l.name[0] == '.' || l.name[0] == '!')) l.name.erase(0, 1);
        if (l.name.empty()) continue;
        l.setting = l.name.find('.') == std::string::npos;   // bSmoothFrameRate, MinDesiredFrameRate: engine settings, not console variables
        scan.lines.push_back(std::move(l));
    }
    scan.total_lines = number;
    return scan;
}

bool game_owned_cvar(const std::string& name) {
    const auto n = lower(name);
    return n == "r.raytracing.enable" || starts(n, "r.streamline.") || starts(n, "t.streamline.") || starts(n, "r.fidelityfx.") || starts(n, "r.ngx.") || n == "r.nis.enable";
}

bool picture_cvar(const std::string& name) {
    if (!forbidden_reason(name).empty()) return true;
    const auto n = lower(name);
    static const char* exact[] = {"r.tonemapper.sharpen", "r.tonemapper.quality", "r.tonemapper.grainquantization", "r.filmgrain", "r.motionblurquality", "r.motionblur.amount", "r.motionblur.max",
        "r.motionblur.scale", "r.motionblur.separate", "r.motionblurperobjectsize", "r.motionblurpause", "r.scenecolorfringequality", "r.scenecolorfringe.max", "r.bloomquality", "r.bloom.screenpercentage",
        "r.lensflarequality", "r.depthoffieldquality", "r.scenecolorformat", "r.dbuffer", "r.maxanisotropy", "r.vt.maxanisotropy", "r.streaming.boost", "r.streaming.hiddenprimitivescale",
        "r.postprocessaaquality", "r.temporalaasamples", "r.temporalaafiltersize", "r.temporalaacurrentframeweight", "r.temporalaasharpness", "r.temporalaacatmullrom", "r.temporalaa.upsampling",
        "r.volumetricfog.gridpixelsize", "r.volumetricfog.gridsizez", "r.volumetricfog.historymisssupersamplecount", "r.shadow.radiusthreshold", "r.shadow.nanitelodbias", "r.shadow.distancescale",
        "r.shadow.maxresolution", "r.shadow.csm.maxcascades", "r.capsuleshadows", "r.contactshadows", "r.ambientocclusionlevels", "r.ambientocclusion.maxquality", "r.ssr.quality", "r.ssgi.quality",
        "r.sss.halfres", "r.sss.scale", "r.refractionquality", "r.translucencylightingvolumedim", "r.emitterspawnratescale", "r.particlelodbias", "r.skeletalmeshlodradiusscale",
        "r.hzbocclusion", "r.nanite.culling.minlod", "r.nanite.tessellation", "r.anisotropicmaterials", "r.vrs.contrastadaptiveshading", "r.vrs.enable", "r.lumen.reflections.radiancecache",
        "r.lumen.screenprobegather.temporalfilterprobes", "r.lumen.hardwareraytracing.lightingmode", "r.lumen.hardwareraytracing", "r.raytracing.culling", "r.raytracing.culling.radius", "r.raytracing.culling.angle",
        "r.skipdrawonpsoprecaching", "a.uro.forceanimrate", "foliage.densityscale", "grass.densityscale", "foliage.lodbiasscale", "r.lightfunctionquality", "r.lightshaftquality", "r.fastblurthreshold",
        "r.upscale.quality", "r.eyeadaptationquality"};
    for (const char* e : exact) if (n == e) return true;
    static const char* prefixes[] = {"r.shadow.virtual.smrt.", "r.shadow.virtual.resolutionlodbias", "r.shadow.virtual.clipmap.", "r.shadow.virtual.nonnanite.", "r.shadow.virtual.normalbias", "r.shadow.virtual.markpixelpages",
        "r.lumen.screenprobegather.", "r.lumen.reflections.", "r.lumen.translucency", "r.lumenscene.", "r.lumen.tracemeshsdfs", "r.tsr.", "r.nanite.maxpixelsperedge", "r.streaming.mipbias", "r.mipmaplodbias",
        "r.ngx.dlss.sharpness", "r.ngx.dlss.preset", "r.streamline.dlss.sharpness", "r.streamline.nis.", "r.fidelityfx.", "r.nis.", "r.tonemapper.", "r.motionblur", "r.distancefieldshadowing", "r.distancefieldao",
        "r.aoglobaldistancefield", "r.detailmode", "r.viewdistancescale", "r.skeletalmeshlodbias", "r.staticmeshlodbias", "fx.niagara.qualitylevel", "r.materialqualitylevel", "r.heterogeneousvolumes",
        "r.volumetriccloud", "r.skyatmosphere", "r.water.", "r.hairstrands.", "r.substrate", "r.ssfs."};
    for (const char* p : prefixes) if (starts(n, p)) return true;
    return false;
}
}
