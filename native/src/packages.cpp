#include "packages.hpp"
#include <array>
#include <cstring>
#include <fstream>
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
    uint64_t total=0;
    for(auto& [name,entry]:entries) {
        auto leaf=name.substr(name.rfind('/')+1);
        if(!name.ends_with("/manifest.json") && !name.ends_with("/thumbnail.png") && !dye_resource(leaf)) continue;
        total+=entry.size;
        if(total>260*1024*1024) throw std::runtime_error("CSS metadata resources exceed limit");
        if(name.ends_with("/thumbnail.png") && entry.size>4*1024*1024) throw std::runtime_error("CSS thumbnail exceeds limit");
        if(name.ends_with("/manifest.json") && entry.size>256*1024) throw std::runtime_error("CSS manifest exceeds limit");
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
}
std::vector<PackageCatalog> package_catalogs(const fs::path& paks,const fs::path& cache,Json* diagnostics) {
    std::vector<PackageCatalog> result;
    Json report={{"pak_files",0},{"files",Json::array()},{"errors",Json::array()}};
    auto path_text=[](const fs::path& path) { auto text=path.generic_u8string(); return std::string(text.begin(),text.end()); };
    auto directory_error=[&](const fs::path& path,const std::string& reason) {
        report["errors"].push_back({{"path",path_text(path)},{"reason",reason}});
    };
    std::vector<fs::path> paths, directories;
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
        for(const auto& [name,bytes]:entries) if(name.ends_with("/manifest.json")) {
            auto manifest=Json::parse(bytes);
            if(manifest.at("format")!="CSS.Package" || manifest.at("format_version")!=1 ||
               manifest.at("game")!="MortalShell2" || manifest.at("engine")!="5.6") throw std::runtime_error("Unsupported CSS package format");
            auto id=manifest.at("id").get<std::string>();
            if(!valid_id(id) || name!=std::string(prefix)+id+"/manifest.json") throw std::runtime_error("CSS package identity mismatch");
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
                }
            }
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
            auto directory=cache/id/hex(hash(bytes,true));
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
            result.push_back({manifest.at("catalog"),directory,path});
        }
        if(result.size()>begin) { record["status"]="validated"; record.erase("reason"); }
        } catch(const std::exception& error) {
            result.resize(begin);
            record["status"]="rejected"; record["reason"]=error.what();
        }
        report["files"].push_back(std::move(record));
    }
    if(diagnostics) *diagnostics=std::move(report);
    return result;
}
}
