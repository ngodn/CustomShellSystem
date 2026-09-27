#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace ccs::runtime {
struct ContentFile {
    std::string path;
    uintmax_t bytes{};
    int64_t modified{};
    bool operator==(const ContentFile&) const = default;
};
struct ContentManifest {
    std::string executable, packages;
    std::vector<ContentFile> files;
    bool operator==(const ContentManifest&) const = default;
};

// Disk metadata is a change detector, not proof of mounted content or native compatibility.
ContentManifest read_content_manifest(const std::filesystem::path& executable, std::stop_token stop = {});

struct ContentObservation {
    uint64_t sequence{}, elapsed_us{};
    bool available{}, restart_required{};
    bool background_priority{};
    std::string error;
    std::shared_ptr<const ContentManifest> manifest;
};

class ContentMonitor {
public:
    static constexpr size_t entry_limit = 4096;
    static constexpr unsigned depth_limit = 8;
    explicit ContentMonitor(std::filesystem::path executable);
    ~ContentMonitor();
    ContentMonitor(const ContentMonitor&) = delete;
    ContentMonitor& operator=(const ContentMonitor&) = delete;
    bool request();
    std::shared_ptr<const ContentObservation> poll();
    void close();
    bool stopped() const { return stopped_.load(std::memory_order_acquire); }
private:
    void run(std::stop_token stop);
    std::filesystem::path executable_;
    std::mutex gate_;
    std::condition_variable_any ready_;
    bool pending_{}, busy_{};
    std::atomic<bool> closed_{};
    std::shared_ptr<const ContentObservation> published_;
    std::atomic<bool> stopped_{};
    std::jthread worker_;
};
}
