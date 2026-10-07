#include "packages.hpp"
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    using namespace css;
    if(argc!=2) return 2;
    try {
        const fs::path root=argv[1];
        std::ifstream stream(root/"expected.json");
        const auto cases=Json::parse(stream);
        size_t checks=0;
        for(const auto& item:cases) {
            const auto directory=root/item.at("name").get<std::string>();
            Json diagnostics;
            const auto packages=package_catalogs(directory,{},&diagnostics);
            if(packages.size()!=item.at("packages").get<size_t>())
                throw std::runtime_error(item.at("name").get<std::string>()+": "+diagnostics.dump());
            ++checks;
            if(item.contains("catalog")) {
                if(packages.front().catalog!=item.at("catalog"))
                    throw std::runtime_error("Resource resolution changed the catalog");
                const auto library=AnimationLibrary::parse(packages.front().catalog.at("animation_library"));
                if(library.faces.size()!=1 || library.faces.front().performances.size()!=1)
                    throw std::runtime_error("Missing parsed facial performance");
                const auto catalog=Catalog::load(directory/"empty",directory,{});
                if(catalog.animation_libraries.size()!=1 ||
                   catalog.animation_libraries[0].faces[0].performances[0].key_count()!=
                   library.faces[0].performances[0].key_count())
                    throw std::runtime_error("Catalog consumers lost facial data");
                checks+=3;
            } else if(diagnostics["files"][0]["status"]!="rejected") {
                throw std::runtime_error("Invalid resource was not diagnosed");
            } else {
                const auto fallback=root/(item.at("name").get<std::string>()+"-fallback");
                fs::create_directories(fallback);
                atomic_json(fallback/"outfit.css.json",{{"schema",1},{"outfits",{
                    {{"id","test.outfit"},{"name","Existing outfit"},{"shells",{"CharacterId.Player.Shell.Genessa"}},
                     {"variants",{{{"id","one"},{"name","One"},{"mesh","/Game/Test/Body.Body"}}}}}
                }}},false);
                const auto catalog=Catalog::load(fallback,directory,{});
                if(catalog.outfits.size()!=1 || !catalog.animation_libraries.empty())
                    throw std::runtime_error("Rejected facial resources hid a healthy outfit");
                ++checks;
            }
        }
        std::cout<<"Facial resource package checks passed: "<<checks<<'\n';
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
