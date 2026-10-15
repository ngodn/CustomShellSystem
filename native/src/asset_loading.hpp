#pragma once
#include <stdexcept>

namespace css {
class AssetNotReady : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
}
