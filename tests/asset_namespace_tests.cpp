#include "asset_namespace.hpp"
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void expect(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
}

int main() try {
    using namespace css;
    expect(playback_adapter_asset("/Game/CSS/eins0fx/EveSB/Animations/Adapters/ABP_Eve.ABP_Eve_C"),"Author/project adapter accepted");
    expect(playback_adapter_asset("/Game/CSS/CSS_EvePeony_eins0fx_P/Animations/Adapters/ABP_Eve.ABP_Eve_C"),"Stem-namespaced adapter accepted");
    expect(playback_adapter_asset("/Game/CSS/Locomotion/Adapters/ABP_X.ABP_X_C"),"Legacy shared adapter accepted");
    expect(!playback_adapter_asset("/Game/CSS/CSS_EvePeony_eins0fx_P/Animations/Adapters/"),"Empty adapter name rejected");
    expect(!playback_adapter_asset("/Game/CSS/CSS_EvePeony_eins0fx_P/ABP_Secondary.ABP_Secondary_C"),"Non-adapter folder rejected");
    expect(!playback_adapter_asset("/Game/Sparta/Animations/Adapters/ABP_X.ABP_X_C"),"Game content rejected");
    expect(!playback_adapter_asset("/Game/CSS/bad-name/Animations/Adapters/ABP_X.ABP_X_C"),"Invalid namespace rejected");
    expect(facial_adapter_asset("/Game/CSS/CSS_EvePeony_eins0fx_P/PeonyV2/Meshes/SK_PeonyV2_Face.SK_PeonyV2_Face"),"Stem-namespaced face accepted");
    std::cout<<"asset namespace tests passed ("<<checks<<" checks)\n";
    return 0;
} catch(const std::exception& error) {
    std::cerr<<"asset namespace test failed: "<<error.what()<<'\n';
    return 1;
}
