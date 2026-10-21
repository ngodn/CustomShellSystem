#include "original_shades.hpp"
#include <iostream>
#include <stdexcept>
using namespace css;
static void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        const auto old=original_shade_definition("Sparta.Item.Skin.Shell.Shared.Alternative");
        check(old && old->suffix=="AltSkin" && old->fallback_label=="Alternative","Existing alternative shade IDs changed");
        const auto obsidian=original_shade_definition("Sparta.Item.Skin.Shell.Shared.Obsidian");
        check(obsidian && obsidian->suffix=="Obsidian","Obsidian shade missing");
        const auto regal=original_shade_definition("Sparta.Item.Skin.Shell.Shared.Regal");
        check(regal && regal->suffix=="Regal","October update Regal shades are rejected");
        const auto future=original_shade_definition("Sparta.Item.Skin.Shell.Shared.Autumn_2027");
        check(future && future->suffix=="Autumn_2027","New native shade names require another allowlist update");
        check(!original_shade_definition("None"),"Invalid tag accepted");
        check(!original_shade_definition("Sparta.Item.Skin.Shell.Shared."),"Empty shade accepted");
        check(!original_shade_definition("Sparta.Item.Skin.Shell.Shared.Regal/Other"),"Shade asset-path separator accepted");
        check(!original_shade_definition("Sparta.Item.Skin.Shell.Unrelated.Regal"),"Unrelated tag namespace accepted");
        std::cout<<"Original shade definitions passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
