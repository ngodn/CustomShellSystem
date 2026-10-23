// Parse an inline animation-library JSON with the core's own parser (no game needed).
// Build: c++ -std=c++23 -Inative/src -Inative/vendor tools/authoring/shared-assets/check_library.cpp build/host/libcss_data.a -lcrypto
#include "animation_library.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) try {
    std::ifstream in(argv[1]); const auto json=nlohmann::json::parse(in);
    const auto lib=css::AnimationLibrary::parse(json);
    std::cout<<"parsed: faces "<<lib.faces.size()<<", adapters "<<lib.adapters.size()<<"\n";
    for(const auto& f:lib.faces) std::cout<<"  "<<f.id<<" morphs "<<f.morphs.size()<<" expressions "<<f.expressions.size()<<" performances "<<f.performances.size()<<" adapters "<<f.adapters.size()<<"\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<"REJECTED: "<<e.what()<<"\n"; return 1; }
