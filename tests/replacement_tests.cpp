// Use Non-CSS Mod (1.0.0-beta.6): the container table reader, the folder table, the scan and
// the outfit synthesis, on synthetic containers so nothing here needs the game.
//
//   css_replacement_tests                      run the checks
//   css_replacement_tests <table.json> <utoc>  print the folders a real container touches
#include "data.hpp"
#include "replacements.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
using namespace css;
static unsigned checks;
static void expect(bool value,const char* label) { ++checks; if(!value) throw std::runtime_error(label); }
static void rejects(const std::function<void()>& action,const char* label) {
    bool rejected=false;
    try { action(); } catch(const std::exception&) { rejected=true; }
    expect(rejected,label);
}
// A table of contents with the given chunks: (package id, chunk type).
static void write_utoc(const fs::path& path,const std::vector<std::pair<uint64_t,uint8_t>>& chunks,uint8_t version=8,uint32_t header_size=144) {
    std::vector<unsigned char> bytes(144,0);
    std::memcpy(bytes.data(),"-==--==--==--==-",16);
    bytes[16]=version;
    auto put32=[&](size_t at,uint32_t value) { for(int i=0;i<4;++i) bytes[at+i]=(unsigned char)(value>>(8*i)); };
    put32(20,header_size); put32(24,uint32_t(chunks.size()));
    bytes.resize(header_size,0);
    for(const auto& [id,type]:chunks) {
        unsigned char entry[12]{};
        for(int i=0;i<8;++i) entry[i]=(unsigned char)(id>>(8*i));
        entry[11]=type;
        bytes.insert(bytes.end(),entry,entry+12);
    }
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
}
static std::string hex16(uint64_t id) { char text[17]; std::snprintf(text,sizeof text,"%016llx",(unsigned long long)id); return text; }
static Outfit roster(const std::string& id,const std::vector<std::pair<std::string,std::string>>& looks) {
    Outfit outfit; outfit.id=id; outfit.name=id; outfit.same_skeleton=true;
    for(const auto& [variant_id,mesh]:looks) { Variant v; v.id=variant_id; v.name="Look "+variant_id; v.mesh=mesh; outfit.variants.push_back(v); }
    return outfit;
}
int main(int argc,char** argv) {
    if(argc>=3) {
        try {
            const auto table=ReplacementTable::load(argv[1]);
            for(int i=2;i<argc;++i) {
                const auto packages=read_utoc_packages(argv[i]);
                std::set<std::string> folders; size_t matched=0;
                for(const auto id:packages.ids) if(const auto* folder=table.folder(id)) { folders.insert(*folder); ++matched; }
                std::cout<<argv[i]<<": "<<packages.chunks<<" chunks, "<<packages.ids.size()<<" packages, "<<matched<<" in a listed folder\n";
                for(const auto& folder:folders) std::cout<<"  "<<folder<<'\n';
            }
            return 0;
        } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
    }
    auto root=fs::temp_directory_path()/("css-replacements-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(root/"catalog"); fs::create_directories(root/"Paks/~mods/alpha"); fs::create_directories(root/"cache");
        const std::string alpha="/Game/Sparta/Characters/Enemies/Alpha/", beta="/Game/Sparta/Characters/Enemies/Beta/",
                          gamma="/Game/Sparta/Characters/Shells/Gamma/", family="/Game/Sparta/Characters/Enemies/Family/Sub/";
        const uint64_t a1=0x1111'2222'3333'4444, a2=0xaaaa'bbbb'cccc'dddd, b1=0x0000'0000'0000'0001, g1=0xffff'ffff'ffff'fff0, f1=0x0102'0304'0506'0708, none=0x7777'7777'7777'7777;
        // The table: four folders, ids as the tool writes them.
        const uint64_t skel=0x5e1e'0000'0000'0001;
        Json table_document={{"schema",1},{"folders",{{alpha,hex16(a1)+hex16(a2)},{beta,hex16(b1)},{gamma,hex16(g1)},{family,hex16(f1)}}},
                             {"shared",{{hex16(skel),"/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton"}}}};
        { std::ofstream(root/"catalog/replacement-targets.json")<<table_document.dump(1); }
        const auto table=ReplacementTable::load(root/"catalog/replacement-targets.json");
        expect(table.folders.size()==4 && table.ids.size()==5,"Table did not load every folder and id");
        expect(table.shared.size()==1 && table.shared_asset(skel) && table.shared_asset(skel)->ends_with("SKEL_Human_Skeleton") && !table.shared_asset(a1),"Shared assets did not load");
        rejects([&]{ Json bad=table_document; bad["shared"]["nothex"]="/Game/X"; std::ofstream(root/"bad.json")<<bad.dump(); ReplacementTable::load(root/"bad.json"); },"Bad shared id accepted");
        expect(table.folder(a1) && *table.folder(a1)==alpha && table.folder(a2) && *table.folder(a2)==alpha,"Alpha ids did not resolve");
        expect(table.folder(b1) && *table.folder(b1)==beta && !table.folder(none),"Beta or unknown id resolved wrongly");
        expect(replacement_folder(table.folders,alpha+"Art/Mesh/SK_A.SK_A") && *replacement_folder(table.folders,alpha+"Art/Mesh/SK_A.SK_A")==alpha,"Mesh under Alpha did not map to its folder");
        expect(!replacement_folder(table.folders,"/Game/Sparta/Characters/Enemies/AlphaTwo/SK.SK"),"Folder prefix matched a longer sibling name");
        expect(!replacement_folder(table.folders,"/Game/Sparta/Weapons/SK.SK"),"A weapon mapped to a character folder");
        {   // Longest prefix wins when a family folder and its parent are both listed.
            std::vector<std::string> nested{"/Game/Sparta/Characters/Enemies/Family/",family};
            expect(*replacement_folder(nested,family+"Mesh/SK_F.SK_F")==family,"Longest folder prefix did not win");
        }
        rejects([&]{ Json bad=table_document; bad["folders"][alpha]="0123"; std::ofstream(root/"bad.json")<<bad.dump(); ReplacementTable::load(root/"bad.json"); },"Short id accepted");
        rejects([&]{ Json bad=table_document; bad["folders"][alpha]=hex16(a1)+"ZZZZZZZZZZZZZZZZ"; std::ofstream(root/"bad.json")<<bad.dump(); ReplacementTable::load(root/"bad.json"); },"Non-hex id accepted");
        rejects([&]{ Json bad=table_document; bad["folders"][beta]=hex16(a1); std::ofstream(root/"bad.json")<<bad.dump(); ReplacementTable::load(root/"bad.json"); },"Duplicate id accepted");
        rejects([&]{ Json bad=table_document; bad["folders"]["Enemies/NoRoot/"]=hex16(a1); std::ofstream(root/"bad.json")<<bad.dump(); ReplacementTable::load(root/"bad.json"); },"Folder outside /Game/ accepted");
        rejects([&]{ Json bad=table_document; bad["schema"]=2; std::ofstream(root/"bad.json")<<bad.dump(); ReplacementTable::load(root/"bad.json"); },"Unknown schema accepted");
        // The container table reader.
        write_utoc(root/"one.utoc",{{a1,1},{a1,2},{none,1},{b1,9},{b1,1},{a2,6}});
        const auto one=read_utoc_packages(root/"one.utoc");
        expect(one.chunks==6 && one.ids==std::vector<uint64_t>{b1,a1,none},"Export bundle ids were not read, sorted and unique");
        write_utoc(root/"wide.utoc",{{a1,1}},8,200);
        expect(read_utoc_packages(root/"wide.utoc").ids==std::vector<uint64_t>{a1},"Header size was not honored");
        write_utoc(root/"empty.utoc",{});
        expect(read_utoc_packages(root/"empty.utoc").ids.empty(),"Empty table did not read as no packages");
        rejects([&]{ std::ofstream(root/"magic.utoc",std::ios::binary)<<std::string(200,'x'); read_utoc_packages(root/"magic.utoc"); },"Wrong magic accepted");
        rejects([&]{ std::ofstream(root/"short.utoc",std::ios::binary)<<"-==--==--==--==-"; read_utoc_packages(root/"short.utoc"); },"Short file accepted");
        rejects([&]{ write_utoc(root/"old.utoc",{{a1,1}},2); read_utoc_packages(root/"old.utoc"); },"Pre-UE5 version accepted");
        rejects([&]{
            std::vector<unsigned char> bytes(144,0); std::memcpy(bytes.data(),"-==--==--==--==-",16); bytes[16]=8; bytes[20]=144; bytes[24]=0xff; bytes[25]=0xff; bytes[26]=0x01;
            std::ofstream(root/"huge.utoc",std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),144); read_utoc_packages(root/"huge.utoc"); },"Oversized chunk count accepted");
        rejects([&]{ write_utoc(root/"cut.utoc",{{a1,1},{a2,1}}); fs::resize_file(root/"cut.utoc",144+12+3); read_utoc_packages(root/"cut.utoc"); },"Truncated chunk table accepted");
        rejects([&]{ read_utoc_packages(root/"absent.utoc"); },"Missing file accepted");
        // Only a patch container (the engine's *_P rule) can override base content.
        const auto paks=root/"Paks";
        expect(patch_container(paks/"~mods/Mod_P.pak") && patch_container(paks/"pakchunk99-Windows_P.pak") && patch_container(paks/"x_p.PAK"),"Patch container not recognized");
        expect(!patch_container(paks/"pakchunk0-Windows.pak") && !patch_container(paks/"global.pak") && !patch_container(paks/"~mods/Mod.pak") &&
               !patch_container(paks/"_P.pak") && !patch_container(paks/"ModP.pak"),"Base content mistaken for a patch");
        // Names.
        expect(mod_outfit_id("ProximaFitShape_P")=="css.mod.proximafitshape_p","Outfit id did not lowercase the stem");
        expect(mod_outfit_id("Weird Name (v2)!")=="css.mod.weird-name--v2--","Outfit id kept characters outside the id set");
        expect(valid_id(mod_outfit_id(std::string(200,'a'))),"Long stem produced an invalid id");
        expect(mod_display_name("ProximaFitShape_P")=="ProximaFitShape" && mod_display_name("Aemeath_Harbinger_P")=="Aemeath Harbinger" &&
               mod_display_name("__x__")=="x" && mod_display_name("_P")=="P" && mod_display_name("___")=="___","Display name did not strip the suffix and separators");
        // The scan: a mod over Alpha and Beta, a game container with the same ids, a mod with
        // nothing in a character folder, a pak without a container, and a damaged table.
        write_utoc(paks/"~mods/alpha/AlphaBeta_P.utoc",{{a1,1},{b1,1},{none,1},{a2,2}});
        write_utoc(paks/"pakchunk0-Windows.utoc",{{a1,1},{b1,1}});
        write_utoc(paks/"~mods/Weapon_P.utoc",{{none,1}});
        write_utoc(paks/"~mods/GammaTwice_P.utoc",{{g1,1}});
        write_utoc(paks/"~mods/GammaAgain_P.utoc",{{g1,1},{f1,1}});
        write_utoc(paks/"~mods/NoSuffix.utoc",{{a1,1}});
        write_utoc(paks/"~mods/Skeleton_P.utoc",{{skel,1},{none,1}});
        write_utoc(paks/"~mods/BetaSkel_P.utoc",{{b1,1},{skel,1}});
        std::ofstream(paks/"~mods/Broken_P.utoc")<<"junk";
        for(const auto* name:{"~mods/alpha/AlphaBeta_P.pak","pakchunk0-Windows.pak","~mods/Weapon_P.pak","~mods/PakOnly_P.pak","~mods/Broken_P.pak","~mods/GammaTwice_P.pak","~mods/GammaAgain_P.pak","~mods/NoSuffix.pak","~mods/Skeleton_P.pak","~mods/BetaSkel_P.pak"})
            std::ofstream(paks/name)<<"not a CSS package";
        std::vector<fs::path> ignored;
        for(const auto* name:{"pakchunk0-Windows.pak","~mods/BetaSkel_P.pak","~mods/Broken_P.pak","~mods/GammaAgain_P.pak","~mods/GammaTwice_P.pak","~mods/NoSuffix.pak","~mods/PakOnly_P.pak","~mods/Skeleton_P.pak","~mods/Weapon_P.pak","~mods/alpha/AlphaBeta_P.pak"}) ignored.push_back(paks/name);
        Json diagnostics;
        const auto mods=scan_replacements(ignored,table,&diagnostics);
        expect(mods.size()==5,"Scan did not list the three folder mods and the two shared-asset ones");
        expect(mods[0].stem=="BetaSkel_P" && mods[0].folders==std::vector<std::string>{beta} && mods[0].conflicts.size()==1 && mods[0].conflicts[0].ends_with("SKEL_Human_Skeleton"),"BetaSkel scan record is wrong");
        expect(mods[1].stem=="GammaAgain_P" && mods[1].folders==std::vector<std::string>{family,gamma} && mods[1].packages==2 && mods[1].matched==2 && mods[1].conflicts.empty(),"GammaAgain scan record is wrong");
        expect(mods[2].stem=="GammaTwice_P" && mods[2].folders==std::vector<std::string>{gamma},"GammaTwice scan record is wrong");
        expect(mods[3].stem=="Skeleton_P" && mods[3].folders.empty() && mods[3].conflicts.size()==1,"Skeleton-only mod was not recorded as a conflict");
        expect(mods[4].stem=="AlphaBeta_P" && mods[4].folders==std::vector<std::string>{alpha,beta} && mods[4].packages==3 && mods[4].matched==2,"AlphaBeta scan record is wrong");
        expect(diagnostics["replacement_scan"]["not_patch"]==2 && diagnostics["replacement_scan"]["pak_only"]==1 && diagnostics["replacement_scan"]["containers"]==10,"Scan counters are wrong");
        expect(diagnostics["replacement_conflicts"].size()==2 && diagnostics["replacement_conflicts"][1]["stem"]=="Skeleton_P","Conflict diagnostics are wrong");
        expect(diagnostics["replacement_errors"].size()==1 && diagnostics["replacement_errors"][0]["path"].get<std::string>().ends_with("Broken_P.pak"),"Damaged container was not reported");
        expect(diagnostics["replacements"].size()==4,"Scan diagnostics did not list the folder mods");
        // Outfit synthesis from the roster and the official shells.
        Catalog catalog;
        catalog.replacements=mods; catalog.replacement_folders=table.folders;
        catalog.outfits.push_back(roster("css.npc.enemies",{{"sk_a",alpha+"Art/Mesh/SK_A.SK_A"},{"sk_b",beta+"SK_B.SK_B"},{"sk_f",family+"SK_F.SK_F"},{"sk_x","/Game/Sparta/Characters/Enemies/Other/SK_X.SK_X"}}));
        rebuild_replacement_outfits(catalog);
        auto find=[&](const std::string& id)->const Outfit* { for(const auto& o:catalog.outfits) if(o.id==id) return &o; return nullptr; };
        const auto* alphabeta=find("css.mod.alphabeta_p");
        expect(alphabeta && alphabeta->variants.size()==2 && alphabeta->variants[0].id=="sk_a" && alphabeta->variants[1].id=="sk_b","AlphaBeta outfit did not get one variant per look");
        expect(alphabeta->same_skeleton && alphabeta->name=="AlphaBeta" && alphabeta->variants[0].items.size()==1 && alphabeta->variants[0].items[0].mesh==alphabeta->variants[0].mesh,"AlphaBeta outfit shape is wrong");
        expect(alphabeta->description.find("Look sk_a, Look sk_b")!=std::string::npos,"Description does not name the looks");
        expect(find("css.mod.gammaagain_p") && find("css.mod.gammaagain_p")->variants.size()==1 && find("css.mod.gammaagain_p")->variants[0].id=="sk_f","Family folder look was not listed");
        expect(!find("css.mod.gammatwice_p"),"A mod over a folder with no known look was listed");
        expect(catalog.compatible("css.mod.alphabeta_p","CharacterId.Player.Shell.Genessa") && catalog.compatible("css.mod.alphabeta_p","CharacterId.Player.Darkform.Genessa"),"Mod outfit is not wearable on a shell");
        expect(catalog.display_order("",{}).empty(),"Mod outfits leaked into the Custom Shells order");
        // The official shells arrive later: a rebuild adds Gamma without duplicating the rest.
        auto originals=roster(original_shells_id,{{"gamma",gamma+"Art/Mesh/SK_G.SK_G"}});
        catalog.outfits.push_back(originals);
        rebuild_replacement_outfits(catalog);
        expect(!find("css.mod.skeleton_p") && find("css.mod.betaskel_p") && find("css.mod.betaskel_p")->variants.size()==1,"Conflict-only mod got an outfit, or a conflicting folder mod lost its look");
        size_t mod_outfits=0; for(const auto& o:catalog.outfits) if(mod_outfit(o.id)) ++mod_outfits;
        expect(mod_outfits==4 && find("css.mod.gammatwice_p") && find("css.mod.gammatwice_p")->variants.size()==1 && find("css.mod.gammatwice_p")->variants[0].id=="gamma","Rebuild after the official shells did not add the Gamma mod once");
        expect(find("css.mod.gammaagain_p")->variants.size()==2,"Rebuild did not extend GammaAgain with the official look");
        // Stable ids survive a rebuild, and two containers with one stem get distinct ids.
        catalog.replacements.push_back(mods[4]);
        rebuild_replacement_outfits(catalog);
        expect(find("css.mod.alphabeta_p") && find("css.mod.alphabeta_p-2"),"Duplicate stems did not get distinct ids");
        catalog.replacements.clear();
        rebuild_replacement_outfits(catalog);
        for(const auto& o:catalog.outfits) expect(!mod_outfit(o.id),"Mod outfits survived an empty replacement list");
        // End to end through Catalog::load: the roster document, the table and the paks folder.
        Json roster_document={{"schema",1},{"outfits",Json::array({{{"id","css.npc.enemies"},{"name","Enemies"},{"shells",Json::array({"CharacterId.Player.Shell.Genessa"})},
            {"compatibility","same_skeleton"},{"variants",Json::array({{{"id","sk_a"},{"name","Look A"},{"mesh",alpha+"Art/Mesh/SK_A.SK_A"}}})}}})}};
        { std::ofstream(root/"catalog/enemies.css.json")<<roster_document.dump(1); }
        auto loaded=Catalog::load(root/"catalog",paks,root/"cache");
        expect(loaded.replacements.size()==5 && loaded.replacement_folders.size()==4,"Catalog::load did not run the replacement scan");
        bool listed=false; for(const auto& o:loaded.outfits) if(o.id=="css.mod.alphabeta_p" && o.variants.size()==1 && o.variants[0].name=="Look A") listed=true;
        expect(listed,"Catalog::load did not synthesize the mod outfit");
        expect(loaded.diagnostics["replacement_targets"]["folders"]==4 && loaded.diagnostics["ignored"]==10,"Catalog diagnostics lack the table or the ignored count");
        // A catalog document may not claim the reserved prefix.
        Json claim=roster_document; claim["outfits"][0]["id"]="css.mod.fake";
        { std::ofstream(root/"catalog/enemies.css.json")<<claim.dump(1); }
        rejects([&]{ Catalog::load(root/"catalog",paks,root/"cache"); },"Catalog document claimed the css.mod. prefix");
        { std::ofstream(root/"catalog/enemies.css.json")<<roster_document.dump(1); }
        // No table: the catalog still loads and says why the row is empty.
        fs::remove(root/"catalog/replacement-targets.json");
        auto untabled=Catalog::load(root/"catalog",paks,root/"cache");
        expect(untabled.replacements.empty() && untabled.diagnostics["replacement_targets"]=="missing" && untabled.outfits.size()==1,"Missing table was not tolerated");
        { std::ofstream(root/"catalog/replacement-targets.json")<<"{not json"; }
        auto broken=Catalog::load(root/"catalog",paks,root/"cache");
        expect(broken.replacements.empty() && broken.diagnostics["replacement_targets"].get<std::string>().starts_with("invalid:") && broken.outfits.size()==1,"Broken table was not tolerated");
        fs::remove_all(root);
        std::cout<<"Replacement mod checks passed ("<<checks<<")\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; fs::remove_all(root); return 1; }
}
