#pragma once
#include "core_types.hpp"
#include <cssx/client.hpp>
#include <map>

namespace cine {
using Json = nlohmann::json;
inline constexpr const char* core_table_path = "/Game/CSSX/CINE/Core/DT_CINE_Core.DT_CINE_Core";

// Startup/take-boundary parsing. No reflected calls or JSON access in the frame consumers.
const Json& default_content_rows();
const Json& content_schema();
void validate_content_row(const std::string& name, const Json& row);
CoreContent parse_content(const Json& rows);
struct ContentOverride {
    CoreContent value;
    std::vector<std::string> problems;
};
ContentOverride resolve_content(const Json& core, const std::map<std::string, Json>& rows);
const CoreContent& content();
void reset_content();
void install_content(CoreContent value);
std::string annotation(const std::string& pattern, size_t count = 0, const std::string& group = {});

// One bridge request per advance. Publication is atomic after every row validates.
class ContentLoader {
public:
    bool advance(const cssx::Client& host);
    bool done() const { return done_; }
    const std::string& problem() const { return problem_; }
    const Json& rows() const { return core_rows_; }
private:
    Json table_, rows_ = Json::object(), core_rows_ = default_content_rows();
    std::vector<std::string> names_;
    size_t next_ = 0;
    bool listed_ = false, done_ = false;
    std::string problem_;
};
}
