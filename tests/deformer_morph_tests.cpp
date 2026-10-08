#include "deformer_morph_weights.hpp"
#include <iostream>
#include <limits>
#include <vector>

using css::DeformerMorphWeights;
void expect(bool valid,const char* message) {
    if(!valid) throw std::runtime_error(message);
}
int main() {
    try {
        DeformerMorphWeights binding;
        std::map<std::string,float> graph{{"Glute",0.f},{"Hip",0.f}};
        std::vector<std::pair<std::string,float>> writes;
        auto write=[&](const std::string& name,float value) {
            writes.emplace_back(name,value);
            if(!graph.contains(name)) return false;
            graph[name]=value;return true;
        };
        const std::map<std::string,float> desired{{"Glute",1.f},{"Hip",.5f},{"Jaw",.7f}};
        binding.sync(desired,write);
        expect(graph.at("Glute")==1.f && graph.at("Hip")==.5f,"Initial weights missing");
        expect(writes.size()==3,"Unsupported variable was not probed once");
        binding.sync(desired,write);
        expect(writes.size()==3,"Unchanged weights caused writes");
        binding.sync({{"Glute",.25f},{"Hip",.5f},{"Jaw",.9f}},write);
        expect(writes.size()==4 && graph.at("Glute")==.25f,"Changed weight or negative cache failed");
        binding.sync({{"Glute",.25f}},write);
        expect(graph.at("Hip")==0.f && writes.size()==5,"Removed shape was not reset");
        binding.sync({},write);
        expect(graph.at("Glute")==0.f && writes.size()==6,"Release left a stale correction");
        binding.sync(desired,write);
        binding.forget();
        graph={{"Glute",0.f},{"Hip",0.f},{"Jaw",0.f}};
        binding.sync(desired,write);
        expect(graph.at("Glute")==1.f && graph.at("Jaw")==.7f,"New instance did not replay supported weights");
        const auto before=writes.size();
        bool rejected=false;
        try { binding.sync({{"Glute",.4f},{"Hip",std::numeric_limits<float>::quiet_NaN()}},write); }
        catch(const std::runtime_error&) { rejected=true; }
        expect(rejected && writes.size()==before,"Invalid snapshot partially wrote weights");
        graph.erase("Hip");
        rejected=false;
        try { binding.sync({{"Glute",1.f},{"Hip",.8f},{"Jaw",.7f}},write); }
        catch(const std::runtime_error&) { rejected=true; }
        expect(rejected,"A previously supported variable silently failed");
        graph.emplace("Hip",0.f);
        binding.sync({{"Glute",1.f},{"Hip",.8f},{"Jaw",.7f}},write);
        expect(graph.at("Hip")==.8f,"Failed write was incorrectly cached as successful");
        std::cout<<"Deformer morph lifecycle checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
