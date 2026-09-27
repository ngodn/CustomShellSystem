#pragma once
#include "ccs_abi.h"
#include "settings.hpp"
#include "storage.hpp"
#include "writer.hpp"
#include "moveset_manager.hpp"
#include "menu.hpp"
#include <memory>
#include <filesystem>

namespace ccs {

class Core {
public:
    explicit Core(const CcsLoaderContext* loader);
    ~Core();

    void tick(const CcsPlayerContext* player_ctx, double delta_seconds);
    void on_hotkey(uint32_t key_code);
    void shutdown();
    const char* get_status_json();

private:
    std::filesystem::path root_dir_;
    std::unique_ptr<runtime::Writer> writer_;
    std::unique_ptr<runtime::Settings> settings_;
    std::unique_ptr<runtime::Storage> storage_;
    std::unique_ptr<MovesetManager> movesets_;
    std::unique_ptr<Menu> menu_;

    std::string status_json_;
    bool initialized_{false};
};

} // namespace ccs
