#include "shader_fallback.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
int checks=0;
void expect(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
template<class F> bool throws(F&& f) { try { f(); } catch(const std::exception&) { return true; } return false; }

struct ModeChange {
    using Mode=css::shader_fallback::Mode;
    struct Component { std::vector<int> overrides{8,9}; };

    css::shader_fallback::ModeState state;
    Component* component=nullptr;
    std::vector<int> expected{8,9};
    int resets=0, resolutions=0, reads=0;

    explicit ModeChange(Mode mode=Mode::automatic)
        : state{mode,css::shader_fallback::active(mode,false,true)} {}

    bool turn_off(bool expires_on_reset=false) {
        return state.set_mode("off",false,true,[]{ return false; },[&]{
            ++resets;
            if(expires_on_reset) component=nullptr;
            if(component) component->overrides={17,31};
        },[&]{
            css::shader_fallback::snapshot_materials(expected,[&]{
                expect(resets==1,"Resolve the tracked component after resetting controls");
                ++resolutions;
                return component;
            },[&](Component* resolved,auto& slots){
                ++reads;
                expect(resolved!=nullptr,"The override reader received a null component");
                slots.insert(slots.end(),resolved->overrides.begin(),resolved->overrides.end());
            });
        });
    }
};

void test_mode_change() {
    using css::shader_fallback::Mode;
    ModeChange stock;
    expect(stock.state.active,"Auto is active on an unverified build with donors");
    expect(stock.turn_off(),"Auto to Off changes the effective fallback mode");
    expect(stock.state.mode==Mode::off && !stock.state.active && !stock.state.outfit_stale,
           "Off is stored and fallback is inactive on the stock mesh");
    expect(stock.expected.empty(),"A missing component clears the old snapshot");
    expect(stock.resets==1 && stock.resolutions==1 && stock.reads==0,
           "A missing component is resolved once without reading overrides");

    ModeChange::Component component;
    ModeChange expired;
    expired.component=&component;
    expect(expired.turn_off(true),"A component expiring during reset still allows the mode change");
    expect(expired.expected.empty() && expired.resets==1 && expired.resolutions==1 && expired.reads==0,
           "A component expiring during reset leaves an empty snapshot without reading overrides");

    ModeChange live;
    live.component=&component;
    expect(live.turn_off(),"A live appearance also changes the effective fallback mode");
    expect(live.expected==std::vector<int>{17,31},"The snapshot preserves the order of restored overrides");
    expect(live.resets==1 && live.resolutions==1 && live.reads==1,
           "A live component is resolved and read once");

    ModeChange already_off{Mode::off};
    already_off.component=&component;
    expect(!already_off.turn_off(),"Off to Off leaves the effective fallback mode unchanged");
    expect(already_off.expected==std::vector<int>{8,9} && already_off.resets==0 &&
           already_off.resolutions==0 && already_off.reads==0,
           "An unchanged mode leaves the old snapshot and controls alone");
}
}

int main() try {
    using namespace css::shader_fallback;
    const nlohmann::json document={
        {"schema",1},
        {"verified_builds",{{{"steam_build","26147827"},{"pak_tables",{{"global.utoc",842},{"pakchunk0-Windows.utoc",9425336},{"pakchunk11-Windows.utoc",3767005}}}}}},
        {"donors",{
            {"/Game/CSS/eins0fx/EveSB/Body/MI_Genessa_body.MI_Genessa_body","/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_body.MI_Genessa_body"},
            {"/Game/CSS/eins0fx/EveSB/Outfit/MI_ShellKeeper_Hair_01.MI_ShellKeeper_Hair_01","/Game/Sparta/Characters/NPCs/ShellKeeper/Art/Mesh/Textures/MI_ShellKeeper_Hair_01.MI_ShellKeeper_Hair_01"}}}};
    const auto catalog=Catalog::parse(document);
    expect(catalog.size()==2,"Both donors load");
    expect(catalog.donor("/Game/CSS/eins0fx/EveSB/Body/MI_Genessa_body.MI_Genessa_body")!=nullptr,"A listed material finds its donor");
    expect(*catalog.donor("/Game/CSS/eins0fx/EveSB/Body/MI_Genessa_body.MI_Genessa_body")=="/Game/MS1/Characters/NPC/Genessa/Materials/MI_Genessa_body.MI_Genessa_body","The donor path is exact");
    expect(catalog.donor("/Game/CSS/eins0fx/EveSB/Body/MI_Genessa_body")==nullptr,"A package path without the object name is not a match");
    expect(catalog.donor("/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Textures/MI_LadyKnight_1001.MI_LadyKnight_1001")==nullptr,"Game materials are never listed");

    PakTables same{{"global.utoc",842},{"pakchunk0-Windows.utoc",9425336},{"pakchunk11-Windows.utoc",3767005}};
    expect(catalog.verified_build(same)==std::optional<std::string>("26147827"),"The verified build is recognised");
    auto older=same; older["pakchunk0-Windows.utoc"]=9422835;
    expect(!catalog.verified_build(older),"A different table size is another build");
    auto extra=same; extra["pakchunk12-Windows.utoc"]=1;
    expect(!catalog.verified_build(extra),"An added chunk is another build");
    auto fewer=same; fewer.erase("pakchunk11-Windows.utoc");
    expect(!catalog.verified_build(fewer),"A missing chunk is another build");
    expect(!catalog.verified_build({}),"An unreadable folder is never verified");

    expect(is_game_table("pakchunk0-Windows.utoc") && is_game_table("pakchunk11-Windows.utoc") && is_game_table("global.utoc"),"Game tables are recognised");
    expect(!is_game_table("pakchunk0-Windows.ucas") && !is_game_table("pakchunk-Windows.utoc") && !is_game_table("pakchunkX-Windows.utoc"),"Other files are ignored");
    expect(!is_game_table("CSS_EveStellarBlade_eins0fx_P.utoc") && !is_game_table("pakchunk0-Windows_P.utoc"),"Mod containers are ignored");

    expect(active(Mode::automatic,false,true),"Auto rebuilds on an unverified game");
    expect(!active(Mode::automatic,true,true),"Auto leaves a verified game alone");
    expect(active(Mode::always,true,true),"Always rebuilds even on a verified game");
    expect(!active(Mode::off,false,true),"Off never rebuilds");
    expect(!active(Mode::always,false,false),"Without a donor catalog nothing is rebuilt");
    expect(parse_mode("auto")==Mode::automatic && parse_mode("always")==Mode::always && parse_mode("off")==Mode::off,"Modes parse");
    expect(!parse_mode("on") && !parse_mode("") && !parse_mode("AUTO"),"Unknown modes are rejected");
    expect(std::string(mode_name(Mode::always))=="always","Modes round-trip");
    test_mode_change();

    // An old outfit version known to lack its shaders on a verified game still gets the fallback.
    expect(!catalog.stale("eins0fx.seduxtress","1.5.0"),"Without a stale list no outfit is stale");
    auto with_stale=document; with_stale["stale_outfits"]={{"eins0fx.seduxtress",{"1.4.0","1.5.0"}},{"eins0fx.unholygenessa",{"0.3.0"}}};
    const auto staled=Catalog::parse(with_stale);
    expect(staled.stale("eins0fx.seduxtress","1.5.0") && staled.stale("eins0fx.unholygenessa","0.3.0"),"Listed versions are stale");
    expect(!staled.stale("eins0fx.seduxtress","1.5.3") && !staled.stale("eins0fx.commanderwhite","0.1.0"),"Other versions and outfits are not");
    expect(active_for_outfit(Mode::automatic,true,true,true),"Auto rebuilds a stale outfit on a verified game");
    expect(!active_for_outfit(Mode::automatic,true,true,false),"Auto leaves a current outfit alone on a verified game");
    expect(active_for_outfit(Mode::automatic,false,true,false),"Auto still rebuilds everything on an unverified game");
    expect(!active_for_outfit(Mode::off,true,true,true),"Off never rebuilds, even a stale outfit");
    expect(!active_for_outfit(Mode::automatic,true,false,true),"Without donors a stale outfit is not rebuilt");
    for(const auto& invalid:{nlohmann::json::array(),nlohmann::json({{"eins0fx.seduxtress","1.5.0"}}),
                             nlohmann::json({{"eins0fx.seduxtress",nlohmann::json::array()}}),
                             nlohmann::json({{"eins0fx.seduxtress",{""}}}),nlohmann::json({{"",{"1.5.0"}}}),
                             nlohmann::json({{"eins0fx.seduxtress",{1}}})}) {
        expect(throws([&]{ auto bad=document; bad["stale_outfits"]=invalid; Catalog::parse(bad); }),"Malformed stale lists are rejected");
    }

    expect(throws([]{ Catalog::parse({{"schema",2}}); }),"A newer schema is rejected");
    auto renamed=document;
    renamed["donors"]["/Game/CSS/x/PackageHash.MI_B"]="/Game/y/MI_C.MI_C";
    const auto renamed_catalog=Catalog::parse(renamed);
    expect(renamed_catalog.donor("/Game/CSS/x/PackageHash.MI_B")!=nullptr,"A relocated material uses its export name");
    expect(renamed_catalog.donor("/Game/CSS/x/PackageHash.PackageHash")==nullptr,"The package leaf does not replace the export name");
    expect(throws([&]{ auto bad=document; bad["donors"]["/Game/CSS/x/.MI_B"]="/Game/y/MI_C.MI_C"; Catalog::parse(bad); }),"Missing package names are rejected");
    expect(throws([&]{ auto bad=document; bad["donors"]["/Engine/x/MI_A.MI_A"]="/Game/y/MI_C.MI_C"; Catalog::parse(bad); }),"Only game content paths are accepted");
    expect(throws([&]{ auto bad=document; bad["donors"]["/Game/x/MI_A.MI_A"]="/Game/x/MI_A.MI_A"; Catalog::parse(bad); }),"A material cannot be its own donor");
    expect(throws([&]{ auto bad=document; bad["verified_builds"][0]["pak_tables"]["evil.utoc"]=1; Catalog::parse(bad); }),"Unknown table names are rejected");
    expect(throws([&]{ auto bad=document; bad["verified_builds"][0]["pak_tables"]["global.utoc"]=-1; Catalog::parse(bad); }),"Negative sizes are rejected");

    // The shipped catalog must parse and every pair must be valid.
    if(const char* shipped=std::getenv("CSS_SHADER_DONORS"); shipped && *shipped) {
        std::ifstream input(shipped);
        expect(bool(input),"The shipped donor catalog opens");
        const auto real=Catalog::parse(nlohmann::json::parse(input));
        expect(real.size()>0,"The shipped donor catalog lists materials");
    }
    std::cout<<"shader fallback tests passed ("<<checks<<" checks)\n";
    return 0;
} catch(const std::exception& error) {
    std::cerr<<"shader fallback test failed: "<<error.what()<<'\n';
    return 1;
}
