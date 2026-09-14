#pragma once
#include "cssx/api.h"
#include "extension_data.hpp"
#include <windows.h>
namespace css {
class ExtensionClient {
    HMODULE module_=nullptr;
    const CssxRuntime* api_=nullptr;
    void* instance_=nullptr;
    CssxHost host_{};
    static void sink(void* output,const char* data,size_t size) {
        auto& bytes=*static_cast<std::string*>(output);if(size<=1024*1024 && bytes.size()+size<=1024*1024) bytes.append(data,size);
    }
public:
    ~ExtensionClient() { if(instance_ && api_ && api_->stop(instance_)) {api_->destroy(instance_);instance_=nullptr;} if(!instance_ && module_) FreeLibrary(module_); }
    void start(const fs::path& root,const CssxHost& host) {
        if(instance_) return;
        host_=host;
        auto file=root/"cores/cssx_core.dll";
        if(fs::exists(root/"cssx.json")) file=extensions::contained_file(root/"cores",read_json(root/"cssx.json").at("file").get<std::string>());
        module_=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!module_) throw std::runtime_error("CSSX core could not load (Windows error "+std::to_string(GetLastError())+")");
        auto get=reinterpret_cast<CssxGetRuntime>(GetProcAddress(module_,"cssx_get_runtime"));
        if(!get || !(api_=get()) || api_->abi!=CSSX_ABI || api_->size<sizeof(CssxRuntime)) throw std::runtime_error("CSSX core ABI mismatch");
        instance_=api_->create(&host_,root.c_str());if(!instance_) throw std::runtime_error("CSSX initialization failed");
    }
    Json request(const Json& value) {
        if(!instance_) throw std::runtime_error("CSSX core is unavailable");
        const auto bytes=value.dump();std::string out;
        const int ok=api_->request(instance_,bytes.c_str(),sink,&out);
        auto result=Json::parse(out);if(!ok) throw std::runtime_error(result.value("error",std::string("CSSX request failed")));return result;
    }
    void tick(double delta) {if(instance_ && !api_->tick(instance_,delta)) throw std::runtime_error("CSSX runtime tick failed");}
    bool stop() {if(!instance_) return true;if(!api_->stop(instance_)) return false;api_->destroy(instance_);instance_=nullptr;return true;}
    bool ready() const {return instance_!=nullptr;}
};
}
