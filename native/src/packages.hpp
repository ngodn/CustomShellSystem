#pragma once
#include "data.hpp"

namespace css {
struct PackageCatalog { Json catalog; fs::path artwork; };
std::vector<PackageCatalog> package_catalogs(const fs::path& paks,const fs::path& cache);
}
