#include "data.hpp"
#include "startup.hpp"
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
        auto project=root/"game/MortalShell2";
        fs::create_directories(project/"Content/Paks");
        const auto executable=project/"Binaries/Win64/MortalShell2-Win64-Shipping.exe";
        expect(wardrobe_packages(executable)==project/"Content/Paks","Game-relative package discovery failed without a Mods folder");
        fs::create_directories(root/"custom-loader/Mods/CustomShellSystem");
        expect(wardrobe_packages(executable)==project/"Content/Paks","Custom UE4SS directory changed game asset discovery");
        for(const auto* platform:{"Win64","WinGDK","CustomPlatform"})
            expect(wardrobe_packages(project/"Binaries"/platform/"Game.exe")==project/"Content/Paks","Executable lookup assumes a storefront platform folder");
        auto relocated=root/"game/SeparateAssets/Content";
        fs::create_directories(relocated/"Paks");
        expect(wardrobe_packages(executable,fs::absolute(relocated))==fs::absolute(relocated)/"Paks","Engine content directory must take precedence over guessed layout");
        expect(wardrobe_packages(executable,root/"missing")==project/"Content/Paks","Missing engine content path prevented executable fallback");
        expect(wardrobe_packages(executable,"relative/Content")==project/"Content/Paks","Unresolved engine path used process working directory");
        expect(wardrobe_startup_message(0).find("No CSS outfit packages")!=std::string::npos,"Empty catalog startup hides missing packages");
        expect(wardrobe_startup_message(3).find("Choose an appearance")!=std::string::npos,"Fresh install suggests CSS must be enabled before selecting");
        fs::remove_all(root/"game"); fs::remove_all(root/"custom-loader");
        // The same package discovery/cache path used by Core::load_catalog.
        if(argc==3) {
            auto catalog=Catalog::load(root/"catalog",argv[2],root/"cache/packages");
            expect(!catalog.outfits.empty(),"Installed package fixture discovery failed");
            for(const auto& outfit:catalog.outfits) {
                expect(fs::is_regular_file(outfit.thumbnail),"Package thumbnail was not extracted");
                expect(!outfit.variants.empty(),"Discovered outfit has no variants");
                for(const auto& variant:outfit.variants)
                    for(const auto& surface:outfit.controls_for(variant.id).surfaces)
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
        auto unicode_file=root/fs::path(u8"游戏/服装")/"state/state.json";
        expect(path_utf8(unicode_file).find("/")!=std::string::npos,"Unicode path could not become a UI cache key");
        auto unicode_state=load_state(unicode_file);
        unicode_state.enabled=true;
        atomic_json(unicode_file,unicode_state.json());
        expect(load_state(unicode_file).json()==unicode_state.json(),"Unicode state path lost preferences");
        { std::ofstream bad(unicode_file); bad<<"{broken"; }
        expect(load_state(unicode_file,&recovered).json()==State{}.json() && recovered,"Unicode backup recovery failed");
        std::cout<<checks<<" startup checks passed\n";
        if(!extracted) fs::remove_all(root);
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<"\nEvidence: "<<root<<'\n'; return 1; }
}
