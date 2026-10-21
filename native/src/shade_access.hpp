#pragma once
#include "data.hpp"

namespace css {
template<class UnlockQuery>
Json shade_picker_choices(const Outfit& originals,const Variant& current,UnlockQuery unlock_query) {
    Json choices=Json::array();
    for(const auto& shade:originals.variants) {
        if(shade.original_shell!=current.original_shell) continue;
        const std::optional<bool> unlocked=unlock_query(shade);
        const auto suffix=!unlocked?" (Unlock status unknown)":*unlocked?"":" (Locked)";
        choices.push_back({{"id",originals.id+"/"+shade.id},{"label",shade.original_shade+suffix}});
    }
    return choices;
}
inline const Variant* shade_command_variant(const Json& command,const Catalog& catalog,
                                            const State& state,const std::string& shell) {
    const auto action=command.value("action",std::string{});
    if(action=="select" || action=="wear")
        return catalog.find(command.at("outfit").get<std::string>(),command.at("variant").get<std::string>());
    if(action=="load_look" || action=="load_profile") {
        const auto selection=profile_selection(state.presets.at(command.at("name").get<std::string>()),catalog,state,shell);
        if(selection.choice) return catalog.find(selection.choice->outfit,selection.choice->variant);
    }
    return nullptr;
}
inline bool shade_confirmation_needed(const Variant* variant,bool unlocked,const Json& command) {
    return variant && !variant->shade_tag.empty() && !unlocked && !command.value("allow_locked_shade",false);
}
inline Json shade_confirmation(Json command,const Variant& variant,bool status_known) {
    command["allow_locked_shade"]=true;
    const bool profile=command.value("action",std::string{}).starts_with("load_");
    const auto message=status_known
        ?variant.original_shade+" is still locked in this save. "
        :"CSS could not check whether "+variant.original_shade+" is unlocked. ";
    return {{"action","ui_confirm"},{"title","Use a locked shade?"},
            {"primary",profile?"Load anyway":"Wear anyway"},
            {"message",message+"CSS can show its appearance without unlocking it or changing quest progress. We encourage earning the shade through normal play."},
            {"target",std::move(command)}};
}
}
