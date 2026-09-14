#pragma once
#include "api.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace cssx {
using Json = nlohmann::json;
class Client {
    CssxHost host_;
    struct Response {std::string text;bool overflow=false;};
    static void receive(void* context,const char* bytes,size_t count) noexcept {
        auto& response=*static_cast<Response*>(context);
        if(response.overflow || count>1024*1024 || response.text.size()+count>1024*1024) {response.overflow=true;return;}
        try {response.text.append(bytes,count);} catch(...) {response.overflow=true;}
    }
public:
    explicit Client(const CssxHost* host) {
        if(!host || host->abi!=CSSX_ABI || host->size<sizeof(CssxHost) || !host->request) throw std::runtime_error("Incompatible CSSX host");
        host_=*host;
    }
    Json request(const Json& value) const {
        auto bytes=value.dump();Response response;
        if(bytes.size()>1024*1024) throw std::runtime_error("CSSX request exceeds 1 MiB");
        const auto ok=host_.request(host_.context,bytes.c_str(),receive,&response);
        if(response.overflow) throw std::runtime_error("CSSX response exceeds 1 MiB");
        auto result=response.text.empty()?Json():Json::parse(response.text);
        if(!ok) throw std::runtime_error(result.value("error",std::string("CSSX host request failed")));
        return result;
    }
    Json player() const {return request({{"op","player"}});}
    Json get(const Json& object,const std::string& property) const {return request({{"op","get"},{"target",object},{"property",property}});}
    Json set(const Json& object,const std::string& property,const Json& value) const {return request({{"op","set"},{"target",object},{"property",property},{"value",value}});}
    Json call(const Json& object,const std::string& function,const Json& arguments=Json::array()) const {
        auto result=request({{"op","call"},{"target",object},{"function",function},{"args",arguments}});
        return result.is_object() && result.contains("ReturnValue")?result.at("ReturnValue"):result;
    }
    Json find(const std::string& path) const {return request({{"op","find"},{"path",path}});}
    void log(const std::string& message,const std::string& level="info",const Json& fields=Json::object()) const {
        request({{"op","log"},{"level",level},{"message",message},{"fields",fields}});
    }
};
}
