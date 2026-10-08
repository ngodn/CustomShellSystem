#pragma once
#include "data.hpp"

namespace css {
struct PackageDependency {
    std::string id, contract, name;
};
struct PackageCatalog {
    Json catalog;
    fs::path artwork, source;
    AnimationContainerRevision revision;
    std::string id;
    std::set<std::string> contracts;
    std::vector<PackageDependency> dependencies;
};
std::vector<PackageCatalog> package_catalogs(const fs::path& paks,const fs::path& cache,Json* diagnostics=nullptr);
}
