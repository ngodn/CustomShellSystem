#include "original_shades.hpp"
#include "shade_access.hpp"
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
        Outfit originals; originals.id=original_shells_id; originals.same_skeleton=true;
        Variant base; base.id="genessa"; base.original_shell="genessa"; base.original_shade="Original";
        Variant shade=base; shade.id="genessa.shade.regal"; shade.original_shade="Sapphire Herald";
        shade.shade_tag="Sparta.Item.Skin.Shell.Shared.Regal";
        Variant other=base; other.id="tiel"; other.original_shell="tiel";
        originals.variants={base,shade,other};
        Catalog catalog; catalog.outfits.push_back(originals);
        const std::string shell="CharacterId.Player.Shell.Genessa";
        State state;
        const Json select{{"action","select"},{"outfit",original_shells_id},{"variant",shade.id}};
        const auto before=state.json();
        const auto* selected=shade_command_variant(select,catalog,state,shell);
        check(selected && selected->id==shade.id,"Shade selection resolved incorrectly");
        auto locked=shade_picker_choices(originals,shade,[](const Variant& v){return v.shade_tag.empty();});
        check(locked.size()==2 && locked[1]["label"]=="Sapphire Herald (Locked)","Locked shade hidden or other shell leaked into list");
        auto unknown=shade_picker_choices(originals,shade,[](const Variant&)->std::optional<bool>{return std::nullopt;});
        check(unknown.size()==2,"Unavailable unlock query hid shade choices");
        check(shade_confirmation_needed(selected,false,select),"Locked manual selection skipped confirmation");
        check(!shade_confirmation_needed(&base,false,select),"Default appearance needs unlock confirmation");
        const auto prompt=shade_confirmation(select,*selected,true);
        check(prompt["primary"]=="Wear anyway" && prompt["target"]["allow_locked_shade"]==true,"Confirmation did not explicitly accept appearance use");
        check(state.json()==before,"Preparing or cancelling confirmation changed saved selection");
        check(!shade_confirmation_needed(selected,false,prompt["target"]),"Confirmed locked shade rejected");
        check(!shade_confirmation_needed(selected,true,select),"Normally unlocked shade still prompts");
        auto unlocked=shade_picker_choices(originals,shade,[](const Variant&){return true;});
        check(unlocked[1]["id"]==locked[1]["id"] && unlocked[1]["label"]=="Sapphire Herald","Normal unlock changed shade identity or retained locked label");
        state.selections[shell]={original_shells_id,shade.id,{}};
        save_profile_snapshot(state,"locked-look");
        const auto saved=state.json();
        state=State::parse(saved);
        check(state.selections.at(shell).variant==shade.id,"Reload discarded a confirmed locked appearance");
        const Json load{{"action","load_look"},{"name","locked-look"}};
        selected=shade_command_variant(load,catalog,state,shell);
        check(selected && shade_confirmation_needed(selected,false,load),"Profile recall bypassed locked shade confirmation");
        check(shade_confirmation(load,*selected,true)["primary"]=="Load anyway","Profile confirmation has wrong action label");
        check(state.json()==saved,"Profile confirmation changed state before acceptance");
        check(!shade_confirmation_needed(selected,true,load),"Unlocked profile still requires confirmation");
        const auto restored=restore_profile_snapshot(state,state.presets.at("locked-look"),catalog,shell);
        check(restored.choice && restored.choice->variant==shade.id,"Profile restore replaced shade appearance");
        std::cout<<"Original shade definitions passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
