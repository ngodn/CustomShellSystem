#include "data.hpp"
#include <chrono>
#include <iostream>

using namespace css;

int main(int argc,char** argv) {
    if(argc!=3) { std::cerr<<"package_dependency_tests CASE_DIRECTORY EXPECTATIONS_JSON\n"; return 2; }
    const auto temporary=fs::temp_directory_path()/(
        "css-dependencies-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto expectations=read_json(argv[2]);
        size_t passed=0;
        for(const auto& test:expectations) {
            const auto name=test.at("name").get<std::string>();
            if(!valid_id(name)) throw std::runtime_error("Invalid test case directory");
            const auto catalog=Catalog::load(temporary/"catalog",fs::path(argv[1])/name,temporary/"cache");
            std::set<std::string> outfits,libraries;
            for(const auto& outfit:catalog.outfits) outfits.insert(outfit.id);
            for(const auto& library:catalog.animation_libraries) libraries.insert(library.id);
            auto fail=[&](const char* message) {
                std::cerr<<catalog.diagnostics.dump(2)<<'\n';
                throw std::runtime_error(name+": "+message);
            };
            if(outfits!=test.at("outfits").get<std::set<std::string>>() ||
               libraries!=test.at("libraries").get<std::set<std::string>>())
                fail("Wrong available outfits or libraries");
            if(catalog.outfit_revisions.size()!=outfits.size()) fail("Rejected outfit left a revision binding");
            if(test.contains("face_matches")) {
                size_t matches=0;
                for(const auto& outfit:catalog.outfits) for(const auto& variant:outfit.variants)
                    if(catalog.facial_match(outfit.id,variant.id).adapter) ++matches;
                if(matches!=test.at("face_matches").get<size_t>()) fail("Unexpected facial revision coverage");
            }
            for(const auto* key:{"loaded","rejected"})
                if(catalog.diagnostics.at(key)!=test.at(key)) fail("Wrong package status count");
            for(const auto& expected:test.value("reasons",Json::array())) {
                bool found=false;
                for(const auto& file:catalog.diagnostics.at("files"))
                    if(file.value("reason",std::string{}).find(expected.get<std::string>())!=std::string::npos)
                        found=true;
                if(!found) fail("Missing actionable diagnostic");
            }
            if(test.contains("empty_message") &&
               catalog.empty_message().find(test.at("empty_message").get<std::string>())==std::string::npos)
                fail("Missing dependency was not explained to the player");
            ++passed;
        }
        fs::remove_all(temporary);
        std::cout<<passed<<" package dependency scenarios passed\n";
    } catch(const std::exception& error) {
        fs::remove_all(temporary);
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
