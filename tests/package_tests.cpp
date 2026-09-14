#include "packages.hpp"
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    using namespace css;
    if(argc!=2) { std::cerr<<"package_tests PACKAGE_DIRECTORY\n"; return 2; }
    auto root=fs::temp_directory_path()/"css-package-integration";
    if(fs::exists(root)) { std::cerr<<"Remove stale test directory first: "<<root<<'\n'; return 2; }
    fs::create_directories(root/"paks");
    try {
        for(const auto& file:fs::directory_iterator(argv[1])) {
            if(file.path().extension()==".pak") fs::copy_file(file.path(),root/"paks"/file.path().filename());
            else fs::create_symlink(fs::absolute(file.path()),root/"paks"/file.path().filename());
        }
        auto packages=package_catalogs(root/"paks",root/"cache");
        if(packages.size()!=1 || !fs::exists(packages[0].artwork/"thumbnail.png")) throw std::runtime_error("Package metadata or thumbnail missing");
        fs::create_directories(root/"catalog");
        auto catalog=Catalog::load(root/"catalog",root/"paks",root/"cache");
        if(catalog.outfits.size()!=1 || catalog.outfits[0].thumbnail.empty()) throw std::runtime_error("Catalog ignored package thumbnail");
        auto cached=packages[0].artwork/"thumbnail.png";
        auto timestamp=fs::last_write_time(cached);
        package_catalogs(root/"paks",root/"cache");
        if(timestamp!=fs::last_write_time(cached)) throw std::runtime_error("Unchanged cache was rewritten");
        std::ofstream(cached,std::ios::binary|std::ios::trunc)<<"corrupt";
        package_catalogs(root/"paks",root/"cache");
        if(fs::file_size(cached)==7) throw std::runtime_error("Corrupt thumbnail cache was not repaired");
        std::set<std::string> masks;
        for(const auto& variant:catalog.outfits[0].variants)
            for(const auto& surface:catalog.outfits[0].colors_for(variant.id).surfaces)
                for(const auto& [part,file]:surface.layers) masks.insert(file);
        for(const auto& file:masks) {
            auto texture=packages[0].artwork/file;
            if(!fs::is_regular_file(texture)) throw std::runtime_error("Packaged color mask was not cached");
        }
        if(!masks.empty()) {
            auto texture=packages[0].artwork/(*masks.rbegin());
            auto bytes=fs::file_size(texture);
            std::ofstream(texture,std::ios::binary|std::ios::trunc)<<"corrupt";
            package_catalogs(root/"paks",root/"cache");
            if(fs::file_size(texture)!=bytes) throw std::runtime_error("Corrupt color mask cache was not repaired");
        }
        fs::path pak;
        for(const auto& file:fs::directory_iterator(root/"paks")) if(file.path().extension()==".pak") pak=file.path();
        std::fstream edit(pak,std::ios::in|std::ios::out|std::ios::binary);
        edit.seekp(-180,std::ios::end); edit.put('\xff'); edit.close();
        Json diagnostics;
        if(!package_catalogs(root/"paks",root/"cache",&diagnostics).empty() || diagnostics["files"][0]["status"]!="rejected")
            throw std::runtime_error("Corrupt pak index was accepted or not reported");
        fs::remove_all(root);
        std::cout<<"Package catalog, thumbnail, cache reuse, repair and corruption checks passed\n";
    } catch(const std::exception& error) { fs::remove_all(root); std::cerr<<error.what()<<'\n'; return 1; }
}
