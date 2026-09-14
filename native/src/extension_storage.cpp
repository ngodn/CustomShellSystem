#include "extension_storage.hpp"
#include "extension_data.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif
namespace css::extensions {
namespace {
void identity(const std::string& id) {
    if(!valid_namespace(id)) throw std::runtime_error("Invalid extension storage identity");
}
std::string timestamp() {
    const auto now=std::chrono::system_clock::now();auto seconds=std::chrono::system_clock::to_time_t(now);
    std::tm time{};
#ifdef _WIN32
    gmtime_s(&time,&seconds);
#else
    gmtime_r(&seconds,&time);
#endif
    std::ostringstream out;out<<std::put_time(&time,"%Y-%m-%dT%H:%M:%S")<<'.'<<std::setfill('0')<<std::setw(3)
        <<(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000)<<'Z';return out.str();
}
bool beneath(const fs::path& base,const fs::path& path) {
    auto a=base.begin(),b=path.begin();for(;a!=base.end();++a,++b) if(b==path.end() || *a!=*b) return false;return b!=path.end();
}
void ensure_beneath(const fs::path& base,const fs::path& path) {
    if(!beneath(fs::weakly_canonical(base),fs::weakly_canonical(path))) throw std::runtime_error("Extension storage path escapes its root");
}
void replace(const fs::path& temp,const fs::path& destination) {
#ifdef _WIN32
    if(!MoveFileExW(temp.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot replace extension output");
#else
    fs::rename(temp,destination);
#endif
}
}
void Storage::log(const std::string& id,const std::string& level,const std::string& message,const Json& fields) {
    identity(id);
    if(level!="debug" && level!="info" && level!="warning" && level!="error") throw std::runtime_error("Invalid log severity");
    if(message.size()>16384 || !fields.is_object() || fields.dump().size()>16384) throw std::runtime_error("Log entry exceeds bound");
    if(policy_.max_bytes<128 || policy_.backups<1 || policy_.backups>16) throw std::runtime_error("Invalid log rotation policy");
    auto path=id=="cssx"?root_/"logs/cssx.jsonl":root_/"logs/extensions"/utf8_path(id)/"current.jsonl";
    ensure_beneath(root_,path);
    const auto line=Json{{"time",timestamp()},{"level",level},{"extension",id},{"message",message},{"fields",fields}}.dump()+"\n";
    fs::create_directories(path.parent_path());
    if(fs::exists(path) && fs::file_size(path)>0 && fs::file_size(path)+line.size()>policy_.max_bytes) {
        auto rotated=[&](unsigned index){auto p=path;p+="."+std::to_string(index);ensure_beneath(root_,p);return p;};
        for(unsigned i=policy_.backups;i>1;--i) if(fs::exists(rotated(i-1))) {
            if(fs::exists(rotated(i))) fs::remove(rotated(i));fs::rename(rotated(i-1),rotated(i));
        }
        if(fs::exists(rotated(1))) fs::remove(rotated(1));fs::rename(path,rotated(1));
    }
    std::ofstream file(path,std::ios::binary|std::ios::app);file.write(line.data(),static_cast<std::streamsize>(line.size()));file.flush();
    if(!file) throw std::runtime_error("Cannot append extension log");
}
fs::path Storage::output(const std::string& id,const std::string& filename,const std::string& bytes) {
    identity(id);
    if(filename.empty() || filename.size()>512 || bytes.size()>8*1024*1024 || filename.find_first_of("\\:\0",0,3)!=std::string::npos) throw std::runtime_error("Invalid extension output file");
    auto relative=utf8_path(filename);
    if(relative.is_absolute() || relative.has_root_name()) throw std::runtime_error("Output filename must be relative");
    for(const auto& part:relative) {
        auto name=path_utf8(part);
        if(name==".." || name=="." || name.empty() || name.back()=='.' || name.back()==' ' || name.find_first_of("<>\"|?*")!=std::string::npos) throw std::runtime_error("Invalid output path component");
        for(unsigned char c:name) if(c<32) throw std::runtime_error("Control character in output filename");
        auto stem=name.substr(0,name.find('.'));for(auto& c:stem) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if(stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" || (stem.size()==4 && (stem.starts_with("COM") || stem.starts_with("LPT")) && stem[3]>='0' && stem[3]<='9')) throw std::runtime_error("Reserved Windows output filename");
    }
    auto base=root_/"output/extensions"/utf8_path(id),path=base/relative;
    ensure_beneath(root_,base);ensure_beneath(base,path);
    fs::create_directories(path.parent_path());auto temp=path;temp+=".tmp";ensure_beneath(base,temp);
    { std::ofstream file(temp,std::ios::binary|std::ios::trunc);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));file.flush();if(!file) throw std::runtime_error("Cannot write extension output"); }
    replace(temp,path);return path;
}
}
