#include "devchannel.hpp"

namespace cssx {
DevChannel::DevChannel(fs::path root,std::function<Json(const Json&)> handler):root_(std::move(root)),handler_(std::move(handler)) {
    std::error_code ec;
    enabled_=fs::exists(root_/"dev/enabled.txt",ec);
    if(!enabled_) return;
    fs::create_directories(root_/"runtime",ec);
    // Ignore a stale request left from an earlier process.
    try { if(fs::exists(root_/"runtime/request.json")) last_id_=read_json(root_/"runtime/request.json").value("id",std::string{}); } catch(...) {}
    watch_=FindFirstChangeNotificationW((root_/"runtime").c_str(),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_SIZE);
}
DevChannel::~DevChannel() { if(watch_!=INVALID_HANDLE_VALUE) FindCloseChangeNotification(watch_); }
bool DevChannel::poll() {
    if(!enabled_ || watch_==INVALID_HANDLE_VALUE) return false;
    if(WaitForSingleObject(watch_,0)!=WAIT_OBJECT_0) return false;
    FindNextChangeNotification(watch_);
    Json request;
    try {
        const auto file=root_/"runtime/request.json";
        std::error_code ec; if(!fs::exists(file,ec)) return false;
        request=read_json(file);
    } catch(...) { return false; }   // partially written; the next change event retries
    const auto id=request.value("id",std::string{});
    if(id.empty() || id==last_id_) return false;
    last_id_=id;
    Json response={{"id",id}};
    try { response["result"]=handler_(request); response["ok"]=true; }
    catch(const std::exception& e) { response["ok"]=false; response["error"]=e.what(); }
    try { atomic_json(root_/"runtime/response.json",response,false); } catch(...) {}
    return true;
}
}
