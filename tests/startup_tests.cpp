#include "data.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace css;
static unsigned checks;
static void expect(bool value,const char* message) { ++checks; if(!value) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    const bool extracted=argc>=2;
    auto root=extracted?fs::path(argv[1]):fs::temp_directory_path()/("css-fresh-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto file=root/"state/state.json";
        expect(!fs::exists(file),"Test must start without user state");
        expect(!fs::exists(root/"catalog"),"Test must start without developer catalog");
        bool recovered=true;
        auto state=load_state(file,&recovered);
        expect(!recovered,"Fresh state was reported as recovered");
        expect(state.json()==State{}.json(),"Fresh state contains non-default settings");
        expect(read_json(file)==state.json(),"Default state file not generated");
        expect(!fs::exists(file.string()+".bak"),"Fresh install fabricated a previous save");
        expect(Catalog::load(root/"catalog").outfits.empty(),"No-package startup failed");
        // The same package discovery/cache path used by Core::load_catalog.
        if(argc==3) {
            auto catalog=Catalog::load(root/"catalog",argv[2],root/"cache/packages");
            expect(!catalog.outfits.empty(),"Installed package fixture discovery failed");
            for(const auto& outfit:catalog.outfits) {
                expect(fs::is_regular_file(outfit.thumbnail),"Package thumbnail was not extracted");
                expect(!outfit.variants.empty(),"Discovered outfit has no variants");
                for(const auto& variant:outfit.variants)
                    for(const auto& surface:outfit.colors_for(variant.id).surfaces)
                        for(const auto& [part,texture]:surface.layers)
                            expect(fs::is_regular_file(outfit.resources/texture),"Variant color resource was not extracted");
            }
            expect(Catalog::load(root/"catalog",argv[2],root/"cache/packages").outfits.size()==catalog.outfits.size(),"Cache reuse changed catalog");
            fs::remove_all(root/"cache");
            expect(Catalog::load(root/"catalog",argv[2],root/"cache/packages").outfits.size()==catalog.outfits.size(),"Cache regeneration failed");
        }
        atomic_json(root/"runtime/loader.json",{{"abi",1}},false);
        atomic_json(root/"runtime/status.json",{{"schema",1},{"enabled",false}},false);
        expect(read_json(root/"runtime/status.json").at("enabled")==false,"Runtime folder/file creation failed");
        auto original=state.json();
        state.enabled=true; state.favorites.insert("test.outfit");
        atomic_json(file,state.json());
        expect(read_json(file.string()+".bak")==original,"First settings change lost default backup");
        expect(load_state(file).json()==state.json(),"Saved preferences did not survive reload");
        // Leave a valid non-default backup, then damage the primary.
        atomic_json(file,state.json());
        { std::ofstream bad(file); bad<<"{broken"; }
        auto restored=load_state(file,&recovered);
        expect(recovered && restored.json()==state.json(),"Corrupt-primary recovery lost preferences");
        expect(read_json(file)==state.json(),"Recovery did not repair primary state");
        expect(read_json(file.string()+".bak")==state.json(),"Recovery overwrote the good backup");
        bool archived=false;
        for(const auto& entry:fs::directory_iterator(file.parent_path())) if(entry.path().filename().string().starts_with("state.json.corrupt-")) archived=true;
        expect(archived,"Corrupt bytes were not preserved");
        fs::remove(file);
        expect(load_state(file,&recovered).json()==state.json() && recovered,"Missing-primary recovery lost backup");
        fs::remove(file); fs::remove(file.string()+".bak");
        expect(load_state(file).json()==State{}.json(),"Deleted state was not regenerated with clean defaults");
        std::cout<<checks<<" startup checks passed\n";
        if(!extracted) fs::remove_all(root);
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\nEvidence: "<<root<<'\n'; return 1; }
}
