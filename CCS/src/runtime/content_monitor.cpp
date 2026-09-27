#include "content_monitor.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#endif

namespace ccs::runtime {
namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
void checkpoint(std::stop_token stop, Clock::time_point deadline) {
    if (stop.stop_requested()) throw std::runtime_error("Content inspection canceled");
    if (Clock::now() >= deadline) throw std::runtime_error("Content inspection exceeded its deadline");
}
std::string path_text(const fs::path& path) {
    const auto utf8 = path.generic_u8string();
    if (utf8.empty() || utf8.size() > 1024) throw std::runtime_error("Content path exceeds its bound");
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}
bool package(const fs::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".pak" || extension == ".utoc" || extension == ".ucas" || extension == ".sig";
}
ContentFile stamp(const fs::path& file, std::string name) {
    const auto status = fs::symlink_status(file);
    if (!fs::is_regular_file(status)) throw std::runtime_error("Content file is missing, linked or not regular");
    const auto modified = fs::last_write_time(file).time_since_epoch().count();
    if (!std::in_range<int64_t>(modified)) throw std::runtime_error("Content timestamp exceeds its bound");
    return {std::move(name), fs::file_size(file), static_cast<int64_t>(modified)};
}
ContentManifest collect(const fs::path& executable, std::stop_token stop, Clock::time_point deadline) {
    checkpoint(stop, deadline);
    if (!executable.is_absolute()) throw std::runtime_error("Game executable path must be absolute");
    const auto normalized = executable.lexically_normal();
    ContentManifest result;
    result.executable = path_text(normalized);
    result.files.push_back(stamp(normalized, "$executable"));
    fs::path packages;
    auto parent = normalized.parent_path();
    for (unsigned depth = 0; depth < ContentMonitor::depth_limit && !parent.empty(); ++depth) {
        checkpoint(stop, deadline);
        const auto candidate = parent / "Content/Paks";
        std::error_code error;
        const auto status = fs::symlink_status(candidate, error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw fs::filesystem_error("Cannot inspect game content directory", candidate, error);
        if (!error && fs::is_symlink(status)) throw std::runtime_error("Linked package roots require explicit mount validation");
        if (!error && fs::is_directory(status)) { packages = candidate; break; }
        const auto next = parent.parent_path();
        if (next == parent) break;
        parent = next;
    }
    if (packages.empty()) throw std::runtime_error("No game Content/Paks directory found near executable");
    result.packages = path_text(packages);
    size_t inspected{};
    for (fs::recursive_directory_iterator it(packages), end; it != end; ++it) {
        checkpoint(stop, deadline);
        if (++inspected > ContentMonitor::entry_limit || it.depth() >= static_cast<int>(ContentMonitor::depth_limit))
            throw std::runtime_error("Game content tree exceeds inspection bounds");
        const auto status = it->symlink_status();
        if (fs::is_symlink(status)) throw std::runtime_error("Linked package entries require explicit mount validation");
        if (!package(it->path())) continue;
        result.files.push_back(stamp(it->path(), path_text(it->path().lexically_relative(packages))));
    }
    if (result.files.size() == 1) throw std::runtime_error("Game package directory contains no supported containers");
    const auto registry = packages.parent_path() / "AssetRegistry.bin";
    std::error_code error;
    const auto registry_status = fs::symlink_status(registry, error);
    if (error && error != std::errc::no_such_file_or_directory)
        throw fs::filesystem_error("Cannot inspect loose Asset Registry", registry, error);
    if (!error && fs::exists(registry_status)) result.files.push_back(stamp(registry, "$loose_asset_registry"));
    std::sort(result.files.begin(), result.files.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    checkpoint(stop, deadline);
    return result;
}
}
ContentManifest read_content_manifest(const std::filesystem::path& executable, std::stop_token stop) {
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    const auto first = collect(executable, stop, deadline);
    const auto second = collect(executable, stop, deadline);
    if (first != second) throw std::runtime_error("Game files changed during content inspection");
    return second;
}

ContentMonitor::ContentMonitor(std::filesystem::path executable)
    : executable_(std::move(executable)), worker_([this](std::stop_token stop) {
        try { run(stop); } catch (...) { stopped_.store(true, std::memory_order_release); }
    }) {}
ContentMonitor::~ContentMonitor() { close(); }
bool ContentMonitor::request() {
    std::unique_lock lock(gate_, std::try_to_lock);
    if (!lock || closed_.load(std::memory_order_acquire) || stopped() || pending_ || busy_) return false;
    pending_ = true; ready_.notify_one(); return true;
}
std::shared_ptr<const ContentObservation> ContentMonitor::poll() {
    std::unique_lock lock(gate_, std::try_to_lock);
    return lock ? published_ : nullptr;
}
void ContentMonitor::close() {
    closed_.store(true, std::memory_order_release);
    worker_.request_stop(); ready_.notify_one();
}
void ContentMonitor::run(std::stop_token stop) {
    bool background_priority{};
#ifdef _WIN32
    background_priority = SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN) != 0;
#endif
    std::shared_ptr<const ContentManifest> baseline;
    uint64_t sequence{};
    bool restart_required{};
    while (!stop.stop_requested()) {
        {
            std::unique_lock lock(gate_);
            ready_.wait(lock, stop, [&] { return pending_ || closed_.load(std::memory_order_acquire); });
            if (closed_.load(std::memory_order_acquire) || stop.stop_requested()) break;
            pending_ = false; busy_ = true;
        }
        const auto began = Clock::now();
        auto result = std::make_shared<ContentObservation>();
        if (sequence == std::numeric_limits<uint64_t>::max()) break;
        result->sequence = ++sequence;
        result->background_priority = background_priority;
        try {
            result->manifest = std::make_shared<ContentManifest>(read_content_manifest(executable_, stop));
            if (!baseline) baseline = result->manifest;
            else if (*baseline != *result->manifest) restart_required = true;
            result->available = true;
        } catch (const std::exception& error) {
            result->error = error.what();
            if (result->error.size() > 512) result->error.resize(512);
        } catch (...) { result->error = "Unknown content inspection failure"; }
        result->restart_required = restart_required;
        result->elapsed_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - began).count());
        std::shared_ptr<const ContentObservation> previous;
        {
            std::lock_guard lock(gate_);
            previous = std::move(published_);
            published_ = std::move(result); busy_ = false;
        }
    }
    stopped_.store(true, std::memory_order_release);
}
}
