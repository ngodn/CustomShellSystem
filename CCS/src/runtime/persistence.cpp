#include "persistence.hpp"
#include "common.hpp"
#include "settings.hpp"
#include "storage.hpp"
#include <limits>
#include <stdexcept>

namespace ccs::runtime {
Persistence::Persistence(std::filesystem::path root)
    : root_(std::move(root)), worker_([this] { run(); }) {}

Persistence::~Persistence() { close(); }

void Persistence::close() {
    { std::lock_guard lock(gate_); closed_ = true; }
    ready_.notify_one();
}

std::optional<uint64_t> Persistence::submit(FileRequest request) {
    if (request.payload.size() > payload_limit) return std::nullopt;
    if (request.replace_existing && request.operation != FileOperation::SavePreset) return std::nullopt;
    switch (request.operation) {
        case FileOperation::LoadPreset:
        case FileOperation::SavePreset:
        case FileOperation::DeletePreset:
            if (!valid_preset_name(request.name)) return std::nullopt;
            break;
        case FileOperation::ListPresets:
        case FileOperation::SaveSettings:
            if (!request.name.empty()) return std::nullopt;
            break;
        default: return std::nullopt;
    }
    const bool writing = request.operation == FileOperation::SavePreset || request.operation == FileOperation::SaveSettings;
    if (writing == request.payload.empty()) return std::nullopt;
    std::lock_guard lock(gate_);
    if (closed_ || next_id_ == std::numeric_limits<uint64_t>::max()) return std::nullopt;
    for (auto& entry : entries_) {
        if (entry.state != State::Free) continue;
        entry.request = std::move(request);
        entry.id = next_id_++;
        entry.state = State::Queued;
        ready_.notify_one();
        return entry.id;
    }
    return std::nullopt;
}

std::optional<FileResult> Persistence::poll() {
    std::lock_guard lock(gate_);
    Entry* completed{};
    for (auto& entry : entries_)
        if (entry.state == State::Complete && (!completed || entry.id < completed->id)) completed = &entry;
    if (!completed) return std::nullopt;
    auto result = std::move(completed->result);
    *completed = Entry{};
    return result;
}

void Persistence::run() {
    while (true) {
        Entry* next{};
        FileRequest request;
        uint64_t id{};
        {
            std::unique_lock lock(gate_);
            ready_.wait(lock, [&] {
                next = nullptr;
                for (auto& entry : entries_)
                    if (entry.state == State::Queued && (!next || entry.id < next->id)) next = &entry;
                return next || closed_;
            });
            if (!next) return;
            next->state = State::Running;
            id = next->id;
            request = std::move(next->request);
        }
        auto result = execute(id, request);
        {
            std::lock_guard lock(gate_);
            next->result = std::move(result);
            next->state = State::Complete;
        }
    }
}

FileResult Persistence::execute(uint64_t id, const FileRequest& request) {
    FileResult result;
    result.id = id; result.operation = request.operation; result.name = request.name;
    try {
        if (request.operation == FileOperation::SaveSettings) {
            Settings settings(root_ / "settings.json");
            settings.from_json(nlohmann::json::parse(request.payload));
            result.success = settings.save();
        } else {
            Storage storage(root_ / "presets");
            switch (request.operation) {
                case FileOperation::ListPresets:
                    result.success = storage.list_presets(result.names);
                    break;
                case FileOperation::LoadPreset:
                    result.preset = storage.load_preset(request.name);
                    result.success = result.preset.has_value();
                    break;
                case FileOperation::SavePreset: {
                    auto preset = Storage::json_to_preset(nlohmann::json::parse(request.payload));
                    if (preset && preset->name == request.name) {
                        const auto written = storage.save_preset(*preset, request.replace_existing);
                        result.success = written == FileWriteResult::Success;
                        result.exists = written == FileWriteResult::Exists;
                    }
                    break;
                }
                case FileOperation::DeletePreset:
                    result.success = storage.delete_preset(request.name);
                    break;
                default: throw std::runtime_error("Unknown persistence operation");
            }
        }
        if (!result.success) result.error = result.exists ? "A preset with this name already exists" : "File operation failed or data was rejected";
    } catch (const std::exception& error) {
        result.success = false;
        result.error = error.what();
        if (result.error.size() > 512) result.error.resize(512);
    } catch (...) {
        result.success = false;
        result.error = "Unknown file operation failure";
    }
    return result;
}
}
