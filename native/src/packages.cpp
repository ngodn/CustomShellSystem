#include "packages.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <set>
#include <span>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#endif

namespace css {
namespace {
using Bytes=std::vector<unsigned char>;
constexpr std::string_view prefix="MortalShell2/Content/CSS/Packages/";
constexpr size_t facial_resource_limit=16*1024*1024;
PackageCatalog package_document(const Json& manifest,Json catalog,fs::path artwork,
                                const fs::path& source,AnimationContainerRevision revision) {
    PackageCatalog result{std::move(catalog),std::move(artwork),source,std::move(revision),
                          manifest.at("id").get<std::string>(),{}, {}};
    if(manifest.contains("provides")) {
        const auto& contracts=manifest.at("provides");
        if(!contracts.is_array() || contracts.size()>32) throw std::runtime_error("Invalid package contracts");
        for(const auto& item:contracts) {
            const auto contract=item.get<std::string>();
            if(!valid_id(contract) || !result.contracts.insert(contract).second)
                throw std::runtime_error("Invalid or duplicate package contract");
        }
    }
    if(manifest.contains("dependencies")) {
        const auto& dependencies=manifest.at("dependencies");
        if(!dependencies.is_array() || dependencies.size()>32) throw std::runtime_error("Invalid package dependencies");
        std::set<std::pair<std::string,std::string>> identities;
        for(const auto& item:dependencies) {
            if(!item.is_object() || item.size()!=3 || !item.contains("id") ||
               !item.contains("contract") || !item.contains("name"))
                throw std::runtime_error("Invalid package dependency fields");
            PackageDependency dependency{item.at("id").get<std::string>(),item.at("contract").get<std::string>(),
                                         item.at("name").get<std::string>()};
            if(!valid_id(dependency.id) || dependency.id==result.id || !valid_id(dependency.contract) ||
               dependency.name.empty() || dependency.name.size()>96 ||
               std::any_of(dependency.name.begin(),dependency.name.end(),[](unsigned char c){return c<32 || c==127;}) ||
               !identities.emplace(dependency.id,dependency.contract).second)
                throw std::runtime_error("Invalid or duplicate package dependency");
            result.dependencies.push_back(std::move(dependency));
        }
    }
    return result;
}
bool facial_resource(std::string_view name) {
    constexpr std::string_view suffix=".face.json";
    return name.ends_with(suffix) && valid_id(std::string(name.substr(0,name.size()-suffix.size())));
}
struct Reader {
    std::span<const unsigned char> bytes;
    size_t cursor=0;
    auto take(size_t count) {
        if(count>bytes.size()-cursor) throw std::runtime_error("Truncated CSS pak index");
        auto result=bytes.subspan(cursor,count); cursor+=count; return result;
    }
    template<class T> T value() { auto data=take(sizeof(T)); T value{}; std::memcpy(&value,data.data(),sizeof(T)); return value; }
    std::string string() {
        auto count=value<int32_t>();
        if(count<1 || count>2048) throw std::runtime_error("Invalid CSS pak path length");
        auto data=take(static_cast<size_t>(count));
        if(data.back()!=0 || std::find(data.begin(),data.end()-1,0)!=data.end()-1) throw std::runtime_error("Invalid CSS pak path");
        return {reinterpret_cast<const char*>(data.data()),data.size()-1};
    }
};
Bytes hash(std::span<const unsigned char> data,bool sha256) {
    Bytes result(sha256?32:20);
#ifdef _WIN32
    auto status=BCryptHash(sha256?BCRYPT_SHA256_ALG_HANDLE:BCRYPT_SHA1_ALG_HANDLE,nullptr,0,
        const_cast<unsigned char*>(data.data()),static_cast<ULONG>(data.size()),result.data(),static_cast<ULONG>(result.size()));
    if(status<0) throw std::runtime_error("CSS package hash failed");
#else
    unsigned length=0;
    if(!EVP_Digest(data.data(),data.size(),result.data(),&length,sha256?EVP_sha256():EVP_sha1(),nullptr) || length!=result.size())
        throw std::runtime_error("CSS package hash failed");
#endif
    return result;
}
std::string hex(const Bytes& bytes) {
    constexpr char digits[]="0123456789abcdef"; std::string result;
    for(auto byte:bytes) { result+=digits[byte>>4]; result+=digits[byte&15]; }
    return result;
}
Bytes read(std::ifstream& file,uint64_t offset,size_t size,uint64_t limit) {
    if(offset>limit || size>limit-offset) throw std::runtime_error("CSS pak entry exceeds file bounds");
    Bytes result(size); file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(size));
    if(!file) throw std::runtime_error("Cannot read CSS pak");
    return result;
}
struct Entry { uint64_t offset,size; Bytes hash; };
std::map<std::string,Bytes> contents(const fs::path& path) {
    // CSS.Package v1 deliberately uses the standard uncompressed V8B pak index.
    // The game reads it normally; CSS can inspect metadata without engine hooks.
    auto size=fs::file_size(path);
    if(size<221) return {};
    std::ifstream file(path,std::ios::binary);
    auto footer=read(file,size-221,221,size); Reader tail{footer}; tail.take(16);
    if(tail.value<uint8_t>()!=0 || tail.value<uint32_t>()!=0x5A6F12E1 || tail.value<uint32_t>()!=8) return {};
    auto offset=tail.value<uint64_t>(),index_size=tail.value<uint64_t>();
    auto expected=tail.take(20);
    if(index_size>1024*1024) return {};
    auto index=read(file,offset,static_cast<size_t>(index_size),size-221);
    if(hash(index,false)!=Bytes(expected.begin(),expected.end())) throw std::runtime_error("CSS pak index checksum mismatch");
    Reader reader{index};
    if(reader.string()!="../../../") return {};
    auto count=reader.value<uint32_t>();
    if(count>4096) return {};
    std::map<std::string,Entry> entries;
    for(uint32_t i=0;i<count;++i) {
        auto name=reader.string();
        auto entry_offset=reader.value<uint64_t>(),compressed=reader.value<uint64_t>(),uncompressed=reader.value<uint64_t>();
        if(reader.value<uint32_t>()!=0) return {};
        auto expected_hash=reader.take(20);
        auto flags=reader.value<uint8_t>(); reader.value<uint32_t>();
        if(name.starts_with(prefix)) {
            if(flags!=0 || compressed!=uncompressed || compressed>32*1024*1024) throw std::runtime_error("Invalid CSS metadata entry");
            if(!entries.emplace(name,Entry{entry_offset,compressed,Bytes(expected_hash.begin(),expected_hash.end())}).second)
                throw std::runtime_error("Duplicate CSS metadata entry");
        }
    }
    if(reader.cursor!=index.size()) throw std::runtime_error("Unexpected CSS pak index data");
    std::map<std::string,Bytes> result;
    uint64_t total=0, facial_total=0;
    for(auto& [name,entry]:entries) {
        auto leaf=name.substr(name.rfind('/')+1);
        const bool facial=facial_resource(leaf);
        if(!name.ends_with("/manifest.json") && !name.ends_with("/thumbnail.png") && !dye_resource(leaf) && !facial) continue;
        total+=entry.size;
        if(total>260*1024*1024) throw std::runtime_error("CSS metadata resources exceed limit");
        if(name.ends_with("/thumbnail.png") && entry.size>4*1024*1024) throw std::runtime_error("CSS thumbnail exceeds limit");
        if(name.ends_with("/manifest.json") && entry.size>256*1024) throw std::runtime_error("CSS manifest exceeds limit");
        if(facial && (entry.size>facial_resource_limit || (facial_total+=entry.size)>64*1024*1024))
            throw std::runtime_error("CSS facial resources exceed limit");
        auto header=read(file,entry.offset,53,offset); Reader local{header};
        if(local.value<uint64_t>()!=0 || local.value<uint64_t>()!=entry.size || local.value<uint64_t>()!=entry.size || local.value<uint32_t>()!=0)
            throw std::runtime_error("CSS pak entry header mismatch");
        auto h=local.take(20);
        if(Bytes(h.begin(),h.end())!=entry.hash || local.value<uint8_t>()!=0) throw std::runtime_error("CSS pak entry checksum mismatch");
        auto data=read(file,entry.offset+53,static_cast<size_t>(entry.size),offset);
        if(hash(data,false)!=entry.hash) throw std::runtime_error("CSS metadata is corrupt");
        result.emplace(name,std::move(data));
    }
    return result;
}
void attach_facial_resources(Json& catalog,const Json& manifest,const std::map<std::string,Bytes>& entries,
    const std::string& package_id) {
    if(!manifest.contains("facial_resources")) return;
    const auto& resources=manifest.at("facial_resources");
    if(!resources.is_array() || resources.empty() || resources.size()>16)
        throw std::runtime_error("Invalid facial resource list");
    auto& faces=catalog.at("animation_library").at("faces");
    if(!faces.is_array() || faces.empty() || faces.size()>16)
        throw std::runtime_error("Facial resources require declared rigs");
    std::set<std::string> seen;
    for(const auto& resource:resources) {
        if(!resource.is_object() || resource.size()!=4)
            throw std::runtime_error("Invalid facial resource descriptor");
        const auto rig=resource.at("rig").get<std::string>();
        const auto file=resource.at("file").get<std::string>();
        if(!valid_id(rig) || file!=rig+".face.json" || !seen.insert(rig).second ||
           !resource.at("bytes").is_number_unsigned())
            throw std::runtime_error("Invalid or duplicate facial resource identity");
        auto target=faces.end();
        for(auto it=faces.begin();it!=faces.end();++it) if(it->at("id")==rig) {
            if(target!=faces.end()) throw std::runtime_error("Duplicate facial resource rig");
            target=it;
        }
        if(target==faces.end() || target->contains("performances"))
            throw std::runtime_error("Facial resource rig is missing or also contains inline performances");
        const auto found=entries.find(std::string(prefix)+package_id+"/"+file);
        if(found==entries.end()) throw std::runtime_error("Packaged facial resource is missing");
        const auto& bytes=found->second;
        if(bytes.empty() || bytes.size()>facial_resource_limit || resource.at("bytes")!=bytes.size() ||
           resource.at("sha256")!=hex(hash(bytes,true)))
            throw std::runtime_error("Facial resource checksum or size mismatch");
        auto payload=Json::parse(bytes);
        if(!payload.is_object() || payload.size()!=3 || !payload.at("schema").is_number_unsigned() ||
           payload.at("schema")!=1 || payload.at("rig")!=rig ||
           !payload.at("performances").is_array() || payload.at("performances").size()>64)
            throw std::runtime_error("Unsupported facial resource payload");
        (*target)["performances"]=std::move(payload.at("performances"));
    }
}
}
// A package's cache folder: the first 16 hex digits of its manifest hash. Short on purpose.
// Windows refuses paths past 259 characters unless the executable opts in, and a default Steam
// install already puts cache/packages/ 125 characters deep. The earlier <id>/<64-hex hash>/
// layout pushed dye mask temp files of Curvy and Cute (264), BTGG (265) and Beaute Knight
// Proxima (263) past the limit, and the failed write rejected the whole package. Here the worst
// case, a 96-character dye name plus ".tmp", ends at 242. The manifest carries the package id,
// so two packages never share a folder.
static bool cache_folder_name(const std::string& name) {
    return name.size()==16 && std::all_of(name.begin(),name.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
// Folders from the old layout are never read again; drop them so the masks they hold do not
// sit on disk forever. Failures are left for the next scan: this is only a cleanup.
static void prune_legacy_cache(const fs::path& cache) {
    std::error_code error;
    fs::directory_iterator it(cache,error),end;
    if(error) return;
    std::vector<fs::path> legacy;
    for(;it!=end;it.increment(error)) {
        if(error) break;
        auto status=it->symlink_status(error);
        if(error) { error.clear(); continue; }
        if(fs::is_directory(status) && !cache_folder_name(path_utf8(it->path().filename()))) legacy.push_back(it->path());
    }
    for(const auto& path:legacy) fs::remove_all(path,error);
}
// 1.0.6: a new package version gets a new folder, so every update used to leave the old masks
// behind (2.3 GB after a few weeks of Eve and Genessa updates). Remove the folders no installed
// package uses. Only after a scan that saw every folder: a skipped directory could hide a package
// whose cache is still wanted. A pruned package that comes back is simply cached again.
static void prune_unused_cache(const fs::path& cache,const std::set<fs::path>& used) {
    std::error_code error;
    fs::directory_iterator it(cache,error),end;
    if(error) return;
    std::vector<fs::path> unused;
    for(;it!=end;it.increment(error)) {
        if(error) break;
        auto status=it->symlink_status(error);
        if(error) { error.clear(); continue; }
        if(fs::is_directory(status) && cache_folder_name(path_utf8(it->path().filename())) && !used.contains(it->path())) unused.push_back(it->path());
    }
    for(const auto& path:unused) fs::remove_all(path,error);
}
std::vector<PackageCatalog> package_catalogs(const fs::path& paks,const fs::path& cache,Json* diagnostics) {
    std::vector<PackageCatalog> result;
    if(!cache.empty()) prune_legacy_cache(cache);
    Json report={{"pak_files",0},{"files",Json::array()},{"errors",Json::array()}};
    auto path_text=[](const fs::path& path) { auto text=path.generic_u8string(); return std::string(text.begin(),text.end()); };
    auto directory_error=[&](const fs::path& path,const std::string& reason) {
        report["errors"].push_back({{"path",path_text(path)},{"reason",reason}});
    };
    std::vector<fs::path> paths, directories;
    std::set<fs::path> used_cache;
    if(!paks.empty()) directories.push_back(paks);
    // Visit each directory independently so an inaccessible sibling cannot hide
    // healthy packages. Do not follow directory symlinks or junction loops.
    while(!directories.empty() && paths.size()<1024) {
        auto directory=directories.back(); directories.pop_back();
        std::error_code error;
        fs::directory_iterator it(directory,error),end;
        if(error) { directory_error(directory,error.message()); continue; }
        for(;it!=end;it.increment(error)) {
            if(error) { directory_error(directory,error.message()); break; }
            auto path=it->path();
            auto status=it->symlink_status(error);
            if(error) { directory_error(path,error.message()); error.clear(); continue; }
            if(fs::is_directory(status)) directories.push_back(path);
            else if(it->is_regular_file(error)) {
                auto extension=path_utf8(path.extension());
                std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c) { return char(c>='A' && c<='Z'?c+('a'-'A'):c); });
                if(extension==".pak") paths.push_back(path);
            }
            if(error) { directory_error(path,error.message()); error.clear(); }
            if(paths.size()>=1024) { directory_error(paks,"Pak scan reached the 1024-file limit"); break; }
        }
        if(error) directory_error(directory,error.message());
    }
    std::sort(paths.begin(),paths.end());
    report["pak_files"]=paths.size();
    for(const auto& path:paths) {
        Json record={{"path",path_text(path)},{"status","ignored"},{"reason","No supported CSS.Package metadata"}};
        const auto begin=result.size();
        try {
        auto entries=contents(path);
        if(std::count_if(entries.begin(),entries.end(),[](const auto& entry) {
            return entry.first.ends_with("/manifest.json");
        })>1) throw std::runtime_error("CSS package must contain one manifest");
        for(const auto& [name,bytes]:entries) if(name.ends_with("/manifest.json")) {
            auto manifest=Json::parse(bytes);
            if(manifest.at("format")!="CSS.Package" || manifest.at("format_version")!=1 ||
               manifest.at("game")!="MortalShell2" || manifest.at("engine")!="5.6") throw std::runtime_error("Unsupported CSS package format");
            auto id=manifest.at("id").get<std::string>();
            if(!valid_id(id) || name!=std::string(prefix)+id+"/manifest.json") throw std::runtime_error("CSS package identity mismatch");
            AnimationContainerRevision revision;
            for(auto suffix:{".utoc",".ucas"}) {
                auto companion=path; companion.replace_extension(suffix);
                if(!fs::is_regular_file(companion)) throw std::runtime_error(std::string("CSS package companion is missing: ")+suffix);
                const auto& expected=manifest.at("containers").at(suffix);
                if(expected.at("file")!=path_utf8(companion.filename()) || expected.at("bytes")!=fs::file_size(companion))
                    throw std::runtime_error("CSS package companion does not match the manifest");
                // Verify the small table here. Full bulk-data hashing belongs in
                // the offline package verifier, never in a game-thread rescan.
                if(std::string_view(suffix)==".utoc") {
                    auto size=fs::file_size(companion);
                    if(size>16*1024*1024) throw std::runtime_error("CSS container table exceeds limit");
                    std::ifstream toc(companion,std::ios::binary);
                    if(expected.at("sha256")!=hex(hash(read(toc,0,static_cast<size_t>(size),size),true)))
                        throw std::runtime_error("CSS container table checksum mismatch");
                    revision.toc_sha256=expected.at("sha256").get<std::string>();
                }
                else revision.bulk_bytes=fs::file_size(companion);
            }
            const auto kind=manifest.value("kind",std::string("outfit"));
            if(kind=="animation_library") {
                auto catalog=manifest.at("catalog");
                if(catalog.size()!=2 || catalog.at("schema")!=1)
                    throw std::runtime_error("Unsupported animation library catalog schema");
                attach_facial_resources(catalog,manifest,entries,id);
                const auto library=AnimationLibrary::parse(catalog.at("animation_library"));
                if(library.id!=id || library.name!=manifest.at("name").get<std::string>() ||
                   library.author!=manifest.at("author").get<std::string>() ||
                   library.version!=manifest.at("version").get<std::string>())
                    throw std::runtime_error("Animation library package identity mismatch");
                if(manifest.contains("thumbnail") || manifest.contains("resources"))
                    throw std::runtime_error("Animation libraries cannot carry outfit resources");
                result.push_back(package_document(manifest,std::move(catalog),{},path,revision));
                continue;
            }
            if(kind!="outfit") throw std::runtime_error("Unsupported CSS package kind");
            if(manifest.contains("facial_resources")) throw std::runtime_error("Outfits cannot carry facial library resources");
            const auto& outfits=manifest.at("catalog").at("outfits");
            if(!outfits.is_array() || outfits.size()!=1 || outfits[0].at("id")!=id ||
               outfits[0].at("name")!=manifest.at("name") || outfits[0].at("author")!=manifest.at("author") ||
               outfits[0].at("thumbnail")!="thumbnail.png") throw std::runtime_error("CSS package catalog identity mismatch");
            const auto& thumbnail=manifest.at("thumbnail");
            if(thumbnail.at("file")!="thumbnail.png") throw std::runtime_error("Unsupported CSS thumbnail path");
            auto found=entries.find(std::string(prefix)+id+"/thumbnail.png");
            if(found==entries.end()) throw std::runtime_error("CSS package thumbnail is missing");
            const auto& image=found->second;
            const std::array<unsigned char,8> signature{137,80,78,71,13,10,26,10};
            if(image.size()<33 || !std::equal(signature.begin(),signature.end(),image.begin())) throw std::runtime_error("CSS thumbnail is not a PNG");
            auto dimension=[&](size_t p) { return (uint32_t(image[p])<<24)|(uint32_t(image[p+1])<<16)|(uint32_t(image[p+2])<<8)|image[p+3]; };
            auto width=dimension(16),height=dimension(20);
            if(width!=height || width<128 || width>1024 || thumbnail.at("width")!=width || thumbnail.at("height")!=height)
                throw std::runtime_error("Invalid CSS thumbnail dimensions");
            auto sha=hex(hash(image,true));
            if(thumbnail.at("sha256")!=sha) throw std::runtime_error("CSS thumbnail checksum mismatch");
            auto directory=cache/hex(hash(bytes,true)).substr(0,16);
            fs::create_directories(directory);
            auto target=directory/"thumbnail.png";
            bool valid=false;
            if(fs::is_regular_file(target) && fs::file_size(target)==image.size()) {
                std::ifstream cached(target,std::ios::binary);
                valid=hex(hash(read(cached,0,image.size(),image.size()),true))==sha;
            }
            if(!valid) {
                auto temp=target; temp+=".tmp";
                std::ofstream output(temp,std::ios::binary|std::ios::trunc);
                output.write(reinterpret_cast<const char*>(image.data()),static_cast<std::streamsize>(image.size())); output.close();
                if(!output) throw std::runtime_error("Could not cache CSS thumbnail");
                if(fs::exists(target)) fs::remove(target);
                fs::rename(temp,target);
            }
            auto options=ControlSet::parse(outfits[0].contains("customize")?outfits[0].at("customize")
                                                                            :outfits[0].value("colors",Json::object()));
            std::set<std::string> required;
            for(const auto& surface:options.surfaces) for(const auto& [part,file]:surface.layers) required.insert(file);
            for(const auto& variant:outfits[0].at("variants")) if(variant.contains("customize") || variant.contains("colors")) {
                auto variant_options=ControlSet::parse(variant.contains("customize")?variant.at("customize"):variant.at("colors"));
                for(const auto& surface:variant_options.surfaces)
                    for(const auto& [part,file]:surface.layers) required.insert(file);
            }
            auto resources=manifest.value("resources",Json::object());
            if(!resources.is_object() || resources.size()!=required.size()) throw std::runtime_error("Dye resource manifest mismatch");
            for(const auto& name:required) {
                if(!resources.contains(name)) throw std::runtime_error("Missing dye resource description");
                const auto& expected=resources.at(name);
                auto found=entries.find(std::string(prefix)+id+"/"+name);
                if(found==entries.end()) throw std::runtime_error("Missing packaged dye texture");
                const auto& data=found->second;
                if(data.size()<33 || !std::equal(signature.begin(),signature.end(),data.begin()) || std::memcmp(data.data()+12,"IHDR",4) || data[24]!=8 || (data[25]!=4 && data[25]!=6))
                    throw std::runtime_error("Invalid dye texture PNG");
                auto dim=[&](size_t p) { return (uint32_t(data[p])<<24)|(uint32_t(data[p+1])<<16)|(uint32_t(data[p+2])<<8)|data[p+3]; };
                auto w=dim(16),h=dim(20); auto sum=hex(hash(data,true));
                if(w!=h || (w!=1024 && w!=2048 && w!=4096) || expected.at("width")!=w || expected.at("height")!=h || expected.at("bytes")!=data.size() || expected.at("sha256")!=sum)
                    throw std::runtime_error("Dye texture checksum or dimensions mismatch");
                auto path=directory/name;
                bool valid=false;
                if(fs::is_regular_file(path) && fs::file_size(path)==data.size()) {
                    std::ifstream input(path,std::ios::binary);
                    valid=hex(hash(read(input,0,data.size(),data.size()),true))==sum;
                }
                if(!valid) {
                    auto temp=path; temp+=".tmp";
                    std::ofstream output(temp,std::ios::binary|std::ios::trunc);
                    output.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size())); output.close();
                    if(!output) throw std::runtime_error("Cannot cache dye texture");
                    if(fs::exists(path)) fs::remove(path);
                    fs::rename(temp,path);
                }
            }
            result.push_back(package_document(manifest,manifest.at("catalog"),directory,path,revision));
            used_cache.insert(directory);
        }
        if(result.size()>begin) { record["status"]="validated"; record.erase("reason"); }
        } catch(const std::exception& error) {
            result.resize(begin);
            record["status"]="rejected"; record["reason"]=error.what();
        }
        report["files"].push_back(std::move(record));
    }
    if(!cache.empty() && report["errors"].empty()) prune_unused_cache(cache,used_cache);
    if(diagnostics) *diagnostics=std::move(report);
    return result;
}
}
