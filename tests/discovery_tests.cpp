#include "data.hpp"
#include "startup.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace css;
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    auto root=fs::temp_directory_path()/("css-discovery-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto paks=root/"Game/Content/Paks";
    try {
        fs::create_directories(paks/"~mods/healthy");
        fs::path healthy;
        for(const auto& file:fs::directory_iterator(argv[1])) if(file.path().extension()==".pak" || file.path().extension()==".utoc" || file.path().extension()==".ucas") {
            auto target=paks/"~mods/healthy"/file.path().filename();
            fs::copy_file(file.path(),target);
            if(file.path().extension()==".pak") healthy=target;
        }
        auto load=[&] { return Catalog::load(root/"catalog",wardrobe_packages(root/"Game/Binaries/Win64/Game.exe"),root/"cache"); };
        if(load().outfits.size()!=1) throw std::runtime_error("Baseline fixture did not load");
        auto bad=paks/"~mods/damaged_P.pak"; fs::copy_file(healthy,bad);
        { std::fstream out(bad,std::ios::binary|std::ios::in|std::ios::out); out.seekp(-180,std::ios::end); out.put('\xff'); }
        auto damaged=load();
        if(damaged.outfits.size()!=1) throw std::runtime_error("Damaged pak hid a healthy outfit");
        if(damaged.diagnostics.at("rejected")!=1 || damaged.diagnostics.at("loaded")!=1)
            throw std::runtime_error("Damaged pak did not retain its failure diagnostic");
        auto record=damaged.diagnostics["files"][0];
        if(record.at("reason")!="CSS pak index checksum mismatch") throw std::runtime_error("Wrong rejection reason");
        fs::remove(bad);
        fs::create_directories(paks/"~mods/duplicate");
        for(const auto& file:fs::directory_iterator(paks/"~mods/healthy")) fs::copy_file(file.path(),paks/"~mods/duplicate"/file.path().filename());
        auto duplicate=load();
        if(duplicate.outfits.size()!=1 || duplicate.diagnostics.at("rejected")!=1)
            throw std::runtime_error("Duplicate outfit blocked catalog or was not reported");
        for(const auto& file:fs::directory_iterator(paks/"~mods/duplicate")) if(file.path().extension()==".utoc") fs::remove(file.path());
        auto incomplete=load();
        if(incomplete.outfits.size()!=1 || incomplete.diagnostics.at("rejected")!=1)
            throw std::runtime_error("Incomplete package hid a healthy outfit");
        fs::remove_all(paks/"~mods/duplicate");
        auto blocked=root/"blocked-cache";
        std::ofstream(blocked)<<"Not a directory";
        auto uncached=Catalog::load(root/"catalog",paks,blocked);
        if(!uncached.outfits.empty() || uncached.diagnostics.at("rejected")!=1 || uncached.empty_message().find("CSS.log")==std::string::npos)
            throw std::runtime_error("Cache failure was accepted or hidden");
        std::ofstream(paks/"ordinary_P.pak")<<"Not a CSS package";
        if(load().outfits.size()!=1 || load().diagnostics.at("ignored")!=1)
            throw std::runtime_error("Non-CSS pak interfered with catalog");
        for(const auto& file:fs::directory_iterator(paks/"~mods/healthy")) fs::rename(file.path(),paks/file.path().filename());
        fs::remove_all(paks/"~mods");
        if(load().outfits.size()!=1) throw std::runtime_error("Pak at Content/Paks root was not discovered");
        auto renamed=paks/healthy.filename(); auto upper=renamed; upper.replace_extension(".PAK"); fs::rename(renamed,upper);
        if(load().outfits.size()!=1) throw std::runtime_error("Uppercase pak extension hid outfit");
        auto missing=Catalog::load(root/"catalog",root/"missing",root/"cache");
        if(missing.diagnostics["errors"].size()!=1 || missing.empty_message().find("CSS.log")==std::string::npos)
            throw std::runtime_error("Missing package folder was not diagnosed");
        fs::remove_all(root); std::cout<<"Package failure isolation and Content/Paks discovery passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; fs::remove_all(root); return 1; }
}
