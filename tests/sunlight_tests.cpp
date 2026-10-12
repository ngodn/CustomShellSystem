#include "sunlight_shadows.hpp"
#include "data.hpp"
#include <iostream>

using css::Json;
static void expect(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
struct World {
    Json pawn={{"$object",1}}, light={{"$object",2},{"class","Class /Script/Engine.DirectionalLightComponent"}};
    float angle=4;
    bool alive=true,visible=true,menu=false,fail_restore=false,bad_signature=false;
    unsigned calls=0,writes=0;
    Json query(const Json& q) {
        ++calls;
        const auto op=q.at("op").get<std::string>();
        if(op=="player") return {{"pawn",pawn}};
        if(op=="valid") return alive;
        if(op=="find") return {{"$object",3}};
        if(op=="describe") return {{"NewValue",{{"out",false},{"return",false},{"size",bad_signature?8:4}}}};
        if(op=="get") {
            const auto p=q.at("property").get<std::string>();
            if(p=="LightSourceAngle") return angle;
            if(p=="IsMenu") return menu;
            if(p=="DirectionalLight") return alive?light:Json();
            if(p=="bVisible") return visible;
            if(p=="bHiddenInGame") return false;
            if(p=="CastShadows") return true;
        }
        if(op=="call") {
            if(q.at("function")=="GetAllActorsOfClass") return {{"OutActors",Json::array({Json{{"$object",4}}})}};
            const float value=q.at("args").at("NewValue").get<float>();
            if(fail_restore && value==4) throw std::runtime_error("restoration unavailable");
            angle=value; ++writes; return Json::object();
        }
        throw std::runtime_error("Unexpected query: "+q.dump());
    }
    auto bridge() { return [this](const Json& q){return query(q);}; }
};
int main() {
    try {
        {
            World w; css::SunlightShadows s;
            s.tick(false,0,w.bridge()); expect(w.calls==0,"Default must not query or change the engine");
            s.tick(true,0,w.bridge()); expect(w.angle<.536f && w.writes==1,"Reduction not applied");
            s.tick(true,2000,w.bridge()); expect(w.writes==1,"Repeated tick recaptured or rewrote the light");
            s.tick(false,2001,w.bridge()); expect(w.angle==4,"Disable failed to restore game angle");
            s.restore(w.bridge()); expect(w.writes==2,"Repeated restoration wrote again");
        }
        {
            World w; css::SunlightShadows s; s.tick(true,0,w.bridge());
            w.angle=2; s.tick(true,2000,w.bridge()); s.tick(true,4000,w.bridge());
            expect(w.angle==2 && w.writes==1,"Overwrote a later lighting change");
            expect(s.diagnostics().at("relinquished_lights")==1,"Later writer ownership not retained");
            s.restore(w.bridge()); expect(w.angle==2,"Unload overwrote a later writer");
            s.tick(true,6000,w.bridge()); s.restore(w.bridge()); expect(w.angle==2,"Reselect lost new original");
        }
        {
            World w; css::SunlightShadows s; s.tick(true,0,w.bridge());
            w.alive=false; s.tick(true,2000,w.bridge()); s.restore(w.bridge());
            expect(w.writes==1,"Wrote to an expired light");
        }
        {
            World w; css::SunlightShadows s; s.tick(true,0,w.bridge());
            w.pawn=nullptr; s.tick(true,2000,w.bridge()); expect(w.angle==4,"Leaving world retained lighting override");
            w.pawn={{"$object",5}}; s.tick(true,4000,w.bridge()); s.restore(w.bridge());
            expect(w.angle==4 && w.writes==4,"New pawn did not recover independent ownership");
        }
        {
            World w; css::SunlightShadows s; s.tick(true,0,w.bridge()); w.fail_restore=true;
            bool failed=false; try {s.restore(w.bridge());} catch(...) {failed=true;}
            expect(failed && s.diagnostics().at("modified_lights")==1,"Failed restore lost original");
            w.fail_restore=false; s.restore(w.bridge()); expect(w.angle==4,"Retry did not restore original");
        }
        {
            World w; css::SunlightShadows s;
            w.menu=true; s.tick(true,0,w.bridge()); expect(w.writes==0,"Modified menu lighting");
            w.menu=false; w.visible=false; s.tick(true,2000,w.bridge()); expect(w.writes==0,"Modified inactive light");
            w.visible=true; w.angle=.2f; s.tick(true,4000,w.bridge()); expect(w.writes==0,"Softened an already sharp light");
            w.angle=4; w.bad_signature=true;
            bool failed=false; try {s.tick(true,6000,w.bridge());} catch(...) {failed=true;}
            expect(failed && w.writes==0,"Called incompatible setter");
            w.bad_signature=false; s.tick(true,8000,w.bridge());
            w.visible=false; s.tick(true,10000,w.bridge()); expect(w.angle==4,"Deactivated light did not restore");
        }
        {
            css::State s; expect(s.sunlight_shadows=="default","New state must use game lighting");
            s.sunlight_shadows="reduced";
            expect(css::State::parse(s.json()).sunlight_shadows=="reduced","Mode did not survive save/reload");
            auto j=s.json(); j.erase("sunlight_shadows");
            expect(css::State::parse(j).sunlight_shadows=="default","Old save must retain game lighting");
            for(const Json bad:{Json("unknown"),Json(5),Json(nullptr),Json::array()}) {
                j["sunlight_shadows"]=bad;
                expect(css::State::parse(j).sunlight_shadows=="default","Invalid mode must retain game lighting");
            }
        }
        std::cout << "Sunlight ownership, lifecycle, restoration and persistence passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
