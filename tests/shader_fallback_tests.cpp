#include "shader_fallback.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void expect(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
template<class F> bool throws(F&& f) { try { f(); } catch(const std::exception&) { return true; } return false; }
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

    expect(throws([]{ Catalog::parse({{"schema",2}}); }),"A newer schema is rejected");
    expect(throws([&]{ auto bad=document; bad["donors"]["/Game/CSS/x/MI_A.MI_B"]="/Game/y/MI_C.MI_C"; Catalog::parse(bad); }),"Mismatched object names are rejected");
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
