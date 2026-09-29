#include "replacements.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

namespace css {
namespace {
// FIoStoreTocHeader (Runtime/Core/Internal/IO/IoStore.h): 144 bytes since UE 5.0. Only the
// magic, the version, the header size and the entry count are read; the chunk-id table
// follows the header directly, twelve bytes per chunk (FIoChunkId: id, index, pad, type).
constexpr std::array<unsigned char,16> toc_magic{'-','=','=','-','-','=','=','-','-','=','=','-','-','=','=','-'};
constexpr size_t toc_header_bytes=144;
constexpr size_t chunk_id_bytes=12;
constexpr uint8_t toc_version_min=3;          // EIoStoreTocVersion::PartitionSize, the first UE5 layout
constexpr uint8_t chunk_export_bundle=1;      // EIoChunkType::ExportBundleData: exactly one per package
constexpr uint32_t max_chunks=65536;          // a character mod has tens; the game's own containers are skipped by name
constexpr size_t max_folders=4096, max_table_ids=1u<<18;
uint32_t le32(const unsigned char* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
uint64_t le64(const unsigned char* p) { return uint64_t(le32(p))|uint64_t(le32(p+4))<<32; }
char lower(unsigned char c) { return char(c>='A' && c<='Z'?c+('a'-'A'):c); }
std::string package_of(const std::string& object_path) {
    const auto slash=object_path.rfind('/');
    const auto dot=object_path.find('.',slash==std::string::npos?0:slash);
    return dot==std::string::npos?object_path:object_path.substr(0,dot);
}
}

ReplacementTable ReplacementTable::load(const fs::path& path) {
    const auto document=read_json(path);
    if(!document.is_object() || document.value("schema",0)!=1 || !document.contains("folders") || !document.at("folders").is_object())
        throw std::runtime_error("Unsupported replacement table schema");
    ReplacementTable table;
    const auto& folders=document.at("folders");
    if(folders.size()>max_folders) throw std::runtime_error("Replacement table lists too many folders");
    size_t total=0;
    for(const auto& [folder,value]:folders.items()) {
        if(folder.size()<8 || folder.size()>256 || !folder.starts_with("/Game/") || !folder.ends_with("/"))
            throw std::runtime_error("Invalid replacement folder: "+folder);
        if(!value.is_string()) throw std::runtime_error("Replacement folder ids must be one hex string: "+folder);
        const auto& hex=value.get_ref<const std::string&>();
        if(hex.size()%16) throw std::runtime_error("Replacement ids are not 16 hex digits each: "+folder);
        total+=hex.size()/16;
        if(total>max_table_ids) throw std::runtime_error("Replacement table exceeds its id limit");
        const auto index=uint32_t(table.folders.size());
        table.folders.push_back(folder);
        for(size_t at=0;at<hex.size();at+=16) {
            uint64_t id=0;
            for(size_t k=0;k<16;++k) {
                const char c=hex[at+k];
                const int digit=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1;
                if(digit<0) throw std::runtime_error("Replacement id is not lowercase hex: "+folder);
                id=id<<4|uint64_t(digit);
            }
            table.ids.push_back({id,index});
        }
    }
    std::sort(table.ids.begin(),table.ids.end(),[](const auto& a,const auto& b) { return a.first<b.first; });
    const auto twice=std::adjacent_find(table.ids.begin(),table.ids.end(),[](const auto& a,const auto& b) { return a.first==b.first; });
    if(twice!=table.ids.end()) throw std::runtime_error("Replacement table lists a package twice");
    // Shared assets are optional in the document (an older table) and named, so a warning can
    // say which asset a container overrides.
    if(document.contains("shared")) {
        const auto& shared=document.at("shared");
        if(!shared.is_object() || shared.size()>max_folders) throw std::runtime_error("Invalid shared asset list");
        for(const auto& [hex,name]:shared.items()) {
            if(hex.size()!=16 || !name.is_string()) throw std::runtime_error("Invalid shared asset entry");
            const auto& text=name.get_ref<const std::string&>();
            if(text.size()<8 || text.size()>256 || !text.starts_with("/Game/")) throw std::runtime_error("Invalid shared asset name");
            uint64_t id=0;
            for(const char c:hex) {
                const int digit=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1;
                if(digit<0) throw std::runtime_error("Shared asset id is not lowercase hex");
                id=id<<4|uint64_t(digit);
            }
            table.shared.push_back({id,text});
        }
        std::sort(table.shared.begin(),table.shared.end(),[](const auto& a,const auto& b) { return a.first<b.first; });
    }
    return table;
}
const std::string* ReplacementTable::shared_asset(uint64_t id) const {
    const auto found=std::lower_bound(shared.begin(),shared.end(),id,[](const auto& entry,uint64_t value) { return entry.first<value; });
    return found!=shared.end() && found->first==id?&found->second:nullptr;
}
const std::string* ReplacementTable::folder(uint64_t id) const {
    const auto found=std::lower_bound(ids.begin(),ids.end(),id,[](const auto& entry,uint64_t value) { return entry.first<value; });
    return found!=ids.end() && found->first==id?&folders[found->second]:nullptr;
}
const std::string* replacement_folder(const std::vector<std::string>& folders,const std::string& object_path) {
    const auto package=package_of(object_path);
    const std::string* best=nullptr;
    for(const auto& folder:folders)
        if(package.starts_with(folder) && (!best || folder.size()>best->size())) best=&folder;
    return best;
}

UtocPackages read_utoc_packages(const fs::path& utoc) {
    std::ifstream file(utoc,std::ios::binary);
    if(!file) throw std::runtime_error("Cannot open the container table");
    std::array<unsigned char,toc_header_bytes> header{};
    file.read(reinterpret_cast<char*>(header.data()),header.size());
    if(!file) throw std::runtime_error("Container table is shorter than its header");
    if(!std::equal(toc_magic.begin(),toc_magic.end(),header.begin())) throw std::runtime_error("Not an IoStore table of contents");
    const auto version=header[16];
    const auto header_size=le32(header.data()+20), chunks=le32(header.data()+24);
    if(version<toc_version_min) throw std::runtime_error("Unsupported container table version "+std::to_string(version));
    if(header_size<toc_header_bytes || header_size>4096) throw std::runtime_error("Unexpected container table header size");
    if(chunks>max_chunks) throw std::runtime_error("Container table exceeds "+std::to_string(max_chunks)+" chunks");
    UtocPackages result; result.chunks=chunks;
    if(!chunks) return result;
    std::vector<unsigned char> table(size_t(chunks)*chunk_id_bytes);
    file.seekg(header_size);
    file.read(reinterpret_cast<char*>(table.data()),std::streamsize(table.size()));
    if(!file) throw std::runtime_error("Container table is truncated");
    for(size_t i=0;i<chunks;++i) {
        const auto* entry=table.data()+i*chunk_id_bytes;
        if(entry[11]==chunk_export_bundle) result.ids.push_back(le64(entry));
    }
    std::sort(result.ids.begin(),result.ids.end());
    result.ids.erase(std::unique(result.ids.begin(),result.ids.end()),result.ids.end());
    return result;
}

bool patch_container(const fs::path& pak) {
    // FString::EndsWith(TEXT("_P.pak")) in the engine ignores case.
    const auto stem=path_utf8(pak.stem());
    return stem.size()>2 && stem[stem.size()-2]=='_' && (stem.back()=='P' || stem.back()=='p');
}

std::vector<ReplacementMod> scan_replacements(const std::vector<fs::path>& paks,
    const ReplacementTable& table,Json* diagnostics) {
    std::vector<ReplacementMod> mods;
    Json report=Json::array(), errors=Json::array(), conflicts=Json::array();
    size_t base=0, pak_only=0;
    for(const auto& pak:paks) {
        if(!patch_container(pak)) { ++base; continue; }
        auto utoc=pak; utoc.replace_extension(".utoc");
        std::error_code error;
        // Cooked packages live in the container on this engine; a pak alone carries none.
        if(!fs::is_regular_file(utoc,error)) { ++pak_only; continue; }
        try {
            const auto packages=read_utoc_packages(utoc);
            std::set<std::string> folders, shared; size_t matched=0;
            for(const auto id:packages.ids) {
                if(const auto* folder=table.folder(id)) { folders.insert(*folder); ++matched; }
                if(const auto* asset=table.shared_asset(id)) shared.insert(*asset);
            }
            if(folders.empty() && shared.empty()) continue;
            ReplacementMod mod;
            mod.stem=path_utf8(pak.stem()); mod.pak=pak;
            mod.folders.assign(folders.begin(),folders.end());
            mod.conflicts.assign(shared.begin(),shared.end());
            mod.packages=packages.ids.size(); mod.matched=matched;
            if(!mod.folders.empty())
                report.push_back({{"path",path_utf8(pak)},{"stem",mod.stem},{"folders",mod.folders},
                                  {"packages",mod.packages},{"matched",matched}});
            if(!mod.conflicts.empty())
                conflicts.push_back({{"path",path_utf8(pak)},{"stem",mod.stem},{"assets",mod.conflicts}});
            mods.push_back(std::move(mod));
        } catch(const std::exception& e) {
            errors.push_back({{"path",path_utf8(pak)},{"reason",e.what()}});
        }
    }
    if(diagnostics) {
        (*diagnostics)["replacements"]=std::move(report);
        (*diagnostics)["replacement_errors"]=std::move(errors);
        (*diagnostics)["replacement_conflicts"]=std::move(conflicts);
        (*diagnostics)["replacement_scan"]={{"containers",paks.size()},{"not_patch",base},{"pak_only",pak_only}};
    }
    return mods;
}

std::string mod_outfit_id(const std::string& stem) {
    std::string id;
    for(unsigned char c:stem) {
        const char l=lower(c);
        id+=(l>='a' && l<='z') || (l>='0' && l<='9') || l=='.' || l=='_' || l=='-'?l:'-';
        if(id.size()>=80) break;
    }
    if(id.empty() || id=="." || id=="..") id="container";
    return std::string(mod_outfit_prefix)+id;
}
std::string mod_display_name(const std::string& stem) {
    std::string name=stem;
    // Containers end in _P by convention (a patch that mounts over the game's own).
    if(name.size()>2 && name[name.size()-2]=='_' && (name.back()=='P' || name.back()=='p')) name.resize(name.size()-2);
    std::string text;
    for(const char c:name) {
        const char out=c=='_' || c=='-'?' ':c;
        if(out==' ' && (text.empty() || text.back()==' ')) continue;
        text+=out;
    }
    while(!text.empty() && text.back()==' ') text.pop_back();
    return text.empty()?stem:text;
}

void rebuild_replacement_outfits(Catalog& catalog) {
    auto& outfits=catalog.outfits;
    std::erase_if(outfits,[](const Outfit& outfit) { return mod_outfit(outfit.id); });
    if(catalog.replacements.empty()) return;
    // Every wearable look by the folder it is drawn from: the shipped roster and the official
    // shells. Pointers into `outfits` stay valid until the new outfits are appended below.
    struct Look { const std::string* folder; const Variant* variant; };
    std::vector<Look> looks;
    for(const auto& outfit:outfits) if(npc_outfit(outfit.id) || outfit.id==original_shells_id)
        for(const auto& variant:outfit.variants)
            if(const auto* folder=replacement_folder(catalog.replacement_folders,variant.mesh)) looks.push_back({folder,&variant});
    std::set<std::string> ids;
    std::vector<Outfit> built;
    for(const auto& mod:catalog.replacements) {
        Outfit outfit;
        outfit.id=mod_outfit_id(mod.stem);
        for(int n=2;!ids.insert(outfit.id).second;++n) outfit.id=mod_outfit_id(mod.stem)+"-"+std::to_string(n);
        outfit.name=mod_display_name(mod.stem);
        outfit.author="Replacement mod";
        outfit.category="Shell";
        outfit.same_skeleton=true;
        std::set<std::string> variant_ids;
        for(const auto& folder:mod.folders) for(const auto& look:looks) {
            if(*look.folder!=folder || !variant_ids.insert(look.variant->id).second) continue;
            Variant variant;
            variant.id=look.variant->id; variant.name=look.variant->name; variant.mesh=look.variant->mesh;
            variant.items.push_back(Item{variant.id,variant.name,variant.mesh,ItemSlot::Body,0,{},{}});
            outfit.variants.push_back(std::move(variant));
        }
        // A folder whose look is not known yet (the official shells arrive once the player
        // exists) leaves nothing to wear; the next rebuild lists it.
        if(outfit.variants.empty()) continue;
        std::string changed;
        constexpr size_t named=4;
        for(size_t i=0;i<outfit.variants.size() && i<named;++i) changed+=(i?", ":"")+outfit.variants[i].name;
        if(outfit.variants.size()>named) changed+=" and "+std::to_string(outfit.variants.size()-named)+" more";
        outfit.description="A replacement mod installed beside the game's containers, "+mod.stem+
            ". It changes "+changed+". Wear that look here; colors and parts stay as the mod's author made them.";
        built.push_back(std::move(outfit));
    }
    for(auto& outfit:built) outfits.push_back(std::move(outfit));
}

void discover_replacements(Catalog& catalog,const fs::path& table_path) {
    catalog.replacements.clear(); catalog.replacement_folders.clear();
    std::error_code error;
    if(!fs::is_regular_file(table_path,error)) {
        catalog.diagnostics["replacement_targets"]="missing";
        rebuild_replacement_outfits(catalog);
        return;
    }
    ReplacementTable table;
    try { table=ReplacementTable::load(table_path); }
    catch(const std::exception& e) {
        catalog.diagnostics["replacement_targets"]=std::string("invalid: ")+e.what();
        rebuild_replacement_outfits(catalog);
        return;
    }
    catalog.diagnostics["replacement_targets"]={{"folders",table.folders.size()},{"packages",table.ids.size()}};
    std::vector<fs::path> ignored;
    for(const auto& file:catalog.diagnostics.value("files",Json::array()))
        if(file.value("status","")=="ignored") {
            const auto text=file.value("path",std::string{});
            ignored.emplace_back(std::u8string(text.begin(),text.end()));
        }
    catalog.replacement_folders=table.folders;
    catalog.replacements=scan_replacements(ignored,table,&catalog.diagnostics);
    rebuild_replacement_outfits(catalog);
}
}
