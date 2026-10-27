#pragma once
#include <cssx/client.hpp>
#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace cine::video {
using Json = cssx::Json;
namespace fs = std::filesystem;
enum class State { Idle, Probing, Frames, Audio, Encoding, Done, Failed };
enum class Size { Window, HD1080, QHD1440, UHD2160 };
enum class Quality { High, Medium, Small };
enum class Pass { Frames, Audio };

struct Settings {
    Size size = Size::Window;
    unsigned fps = 30;
    Quality quality = Quality::High;
};
struct Request {
    Settings settings;
    fs::path cine_folder;
    std::string preset;
    double seconds = 0;
    Json world, pawn;
    // Empty sound means a silent take. Paths must name mounted objects.
    std::string sound, music_class;
    double audio_offset = 0;
    std::string submix = "/Game/CSSX/CINE/Core/Audio/SM_CINE_Render.SM_CINE_Render";
};
struct Take {
    // All callbacks run on the game thread. Both passes suppress the director's music.
    std::function<void(Pass)> begin;
    // Apply the pose BEFORE requesting its screenshot. Called once per engine frame.
    std::function<void(unsigned, double)> frame;
    // Rewind/restore the take. Must be idempotent, including after a failed begin.
    std::function<void()> end;
};
struct Status {
    State state = State::Idle;
    std::string line = "Ready", error, encoder;
    fs::path folder;
    unsigned frames = 0, total = 0, encoded = 0;
    bool restoring = false;
};

std::wstring quote_argument(std::wstring_view value);
std::wstring command_line(const std::vector<std::wstring>& args);
std::vector<std::string> encoder_order(bool wine);
bool is_wine() noexcept;
std::string frame_name(unsigned index);
std::string screenshot_command(const fs::path& file);
std::vector<std::wstring> encoder_options(std::string_view encoder, Quality quality);
std::vector<std::wstring> probe_arguments(const fs::path& exe, std::string_view encoder, Quality quality);
std::vector<std::wstring> encode_arguments(const Request&, const fs::path& folder,
                                         std::string_view encoder, unsigned frames);
struct ProgressParser {
    unsigned frames = 0;
    bool ended = false;
    std::string pending;
    void feed(std::string_view bytes);
};
// Disk validators run only on the worker. A nonempty or stable-size file is insufficient.
bool complete_png(const fs::path& path, unsigned* width = nullptr, unsigned* height = nullptr);
bool complete_wav(const fs::path& path, double* seconds = nullptr);

enum class JobKind { Prepare, Frames, Audio, Encode };
struct Job {
    JobKind kind = JobKind::Prepare;
    Request request;
    fs::path folder;
    std::string encoder;
    unsigned frames = 0;
};
struct Result {
    bool ok = true;
    std::string error, encoder;
    fs::path folder;
};
class IO {
public:
    virtual ~IO() = default;
    virtual void launch(Job job) = 0;
    virtual std::optional<Result> poll() = 0;
    virtual void cancel() noexcept = 0;
    virtual bool busy() const noexcept = 0;
    virtual unsigned progress() const noexcept = 0;
};
std::unique_ptr<IO> make_io();

class Renderer {
public:
    explicit Renderer(cssx::Client& client, Take take, std::unique_ptr<IO> io = make_io());
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool start(Request request) noexcept;
    // Use the per-engine-frame callback, NOT the coalesced 10 Hz extension tick.
    // Supply a monotonic frame serial, steady-clock seconds, and current identities.
    void tick(uint64_t serial, double real_seconds, const Json& world, const Json& pawn) noexcept;
    void cancel() noexcept;
    // Return false to CSSX stop() until restoration and worker teardown finish.
    bool stop() noexcept;
    const Status& status() const noexcept { return status_; }
    bool active() const noexcept;
private:
    cssx::Client& client_;
    Take take_;
    std::unique_ptr<IO> io_;
    Request request_;
    Status status_;
    Json engine_, system_, gameplay_, mixer_, sound_, submix_, component_;
    Json saved_rate_, saved_fixed_;
    bool rate_dirty_ = false, fixed_dirty_ = false, take_dirty_ = false;
    bool recording_ = false, send_dirty_ = false, submix_dirty_ = false;
    bool frames_started_ = false, frames_ended_ = false, audio_ended_ = false;
    bool frames_verified_ = false, audio_flush_pending_ = false, flush_job_ = false;
    bool have_serial_ = false, have_time_ = false;
    uint64_t last_serial_ = 0;
    double last_time_ = 0, audio_start_ = 0;
    std::string cached_encoder_;
    fs::path cached_exe_;
    bool job_pending_ = false;
    Json cdo(const char* name);
    void resolve();
    void resolve_audio();
    void fixed(bool enabled);
    bool restore_engine() noexcept;
    bool restore() noexcept;
    void finish_audio();
    void begin_frames();
    void begin_audio(double now);
    void begin_encode();
    void fail(const std::string& reason) noexcept;
    void launch(JobKind kind);
    void handle_result(const Result& result, double now);
};
}
