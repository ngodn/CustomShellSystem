#pragma once
#include "data.hpp"

namespace css {
struct PackageCatalog { Json catalog; fs::path artwork; fs::path source; };
std::vector<PackageCatalog> package_catalogs(const fs::path& paks,const fs::path& cache,Json* diagnostics=nullptr);
}
