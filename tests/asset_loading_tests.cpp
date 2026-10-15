#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "asset_loading.hpp"
using css::AssetNotReady;

// Engine boundary fixture. The production load function is included below.
enum EObjectFlags { RF_NeedLoad=1, RF_NeedPostLoad=2, RF_NeedPostLoadSubobjects=4,
    RF_WillBeLoaded=8, RF_BeginDestroyed=16, RF_FinishDestroyed=32 };
struct UObject {
    unsigned flags=0;
    bool unreachable=false;
    bool HasAnyFlags(EObjectFlags mask) const { return (flags & mask)!=0; }
    bool IsUnreachable() const { return unreachable; }
};
struct Guard { UObject* value=nullptr; UObject* get() const { return value; }
    void capture(UObject* object) { value=object; } };
static std::unordered_map<std::string,Guard> s_asset_cache;
static UObject *found=nullptr, *loaded=nullptr;
static int blocking_calls=0;
namespace UObjectGlobals {
template<class T> T StaticFindObject(void*,void*,const wchar_t*) { return found; }
}
static std::wstring wide(const std::string& value) { return {value.begin(),value.end()}; }
static UObject* find(const wchar_t*) { static UObject kismet; return &kismet; }
struct FString { explicit FString(const wchar_t*) {} };
struct Call {
    std::wstring name;
    Call(UObject*,const wchar_t* function,int):name(function) {}
    template<class T> void set(const wchar_t*,const T&) {}
    void copy(const wchar_t*,const Call&,const wchar_t*) {}
    void run() { if(name==L"LoadAsset_Blocking") ++blocking_calls; }
    template<class T> T get() { return loaded; }
};
static std::string asset_evidence(UObject*) { return "fixture object"; }
#include "asset_loading.inl"

static void expect(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
static bool rejected() {
    try { return load("/Fixture.Asset")==nullptr; }
    catch(const std::runtime_error&) { return true; }
}
int main() {
    try {
        UObject object;
        found=&object;
        expect(load("/Fixture.Asset")==&object && blocking_calls==0,"Ready find should not block");
        for(auto flag:{RF_NeedLoad,RF_NeedPostLoad,RF_NeedPostLoadSubobjects,RF_WillBeLoaded,
                       RF_BeginDestroyed,RF_FinishDestroyed}) {
            object.flags=flag;
            expect(rejected(),"Cached unfinished/destroying asset escaped the loader");
            if(flag<=RF_WillBeLoaded) {
                bool retryable=false;
                try { load("/Fixture.Asset",false); }
                catch(const AssetNotReady&) { retryable=true; }
                expect(retryable,"Pending optional asset became a permanent missing asset");
            }
            s_asset_cache.clear();
            expect(rejected(),"Found unfinished/destroying asset escaped the loader");
            object.flags=0;
            expect(load("/Fixture.Asset")==&object,"A completed asset did not recover");
        }
        object.unreachable=true;
        expect(rejected(),"Unreachable cached asset escaped the loader");
        s_asset_cache.clear();
        expect(rejected(),"Unreachable found asset escaped the loader");
        object.unreachable=false;
        found=nullptr;loaded=&object;object.flags=RF_NeedPostLoad;
        expect(rejected(),"Blocking loader result skipped readiness validation");
        expect(s_asset_cache.empty(),"Incomplete load result entered the cache");
        object.flags=0;
        expect(load("/Fixture.Asset")==&object,"Blocking load did not recover");
        s_asset_cache.clear();loaded=nullptr;
        expect(load("/Missing.Asset",false)==nullptr,"Optional missing asset must return null");
        bool missing=false;
        try { load("/Missing.Asset"); } catch(const std::runtime_error&) { missing=true; }
        expect(missing,"Required missing asset must report failure");
        std::cout<<"Asset loading readiness and recovery checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
