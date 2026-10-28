#include "render.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <future>
#include <limits>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cine::video {
namespace {
using Clock = std::chrono::steady_clock;
std::wstring widen(std::string_view s) {
#ifdef _WIN32
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8 path");
    std::wstring out(size_t(n), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), out.data(), n))
        throw std::runtime_error("Cannot convert path");
    return out;
#else
    // Paths in the portable tests are ASCII. Windows performs strict UTF-8 conversion.
    return std::wstring(s.begin(), s.end());
#endif
}
std::string utf8(const fs::path& p) {
    const auto s = p.generic_u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}
std::wstring wide_path(const fs::path& p) { return widen(utf8(p)); }
void append(std::vector<std::wstring>& a, const std::vector<std::wstring>& b) { a.insert(a.end(), b.begin(), b.end()); }
bool object(const Json& j) { return j.is_object() && j.contains("$object") && j.at("$object").is_number_unsigned(); }
void require_object(const Json& j, const char* name) { if (!object(j)) throw std::runtime_error(std::string("Unavailable: ") + name); }
bool token(std::string_view s) {
    return !s.empty() && s.size() <= 96 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
unsigned frame_count(const Request& r) {
    if (r.settings.fps != 24 && r.settings.fps != 30 && r.settings.fps != 60) throw std::runtime_error("Frame rate must be 24, 30 or 60");
    if (!std::isfinite(r.seconds) || r.seconds <= 0 || r.seconds > 3600) throw std::runtime_error("Take duration must be between 0 and 3600 seconds");
    return unsigned(std::ceil(r.seconds * r.settings.fps));
}
std::vector<std::wstring> prelude(const fs::path& exe, std::string_view encoder) {
    std::vector<std::wstring> a{wide_path(exe), L"-hide_banner", L"-nostdin", L"-y", L"-loglevel", L"warning"};
    if (encoder == "h264_vulkan") append(a, {L"-init_hw_device", L"vulkan=cine", L"-filter_hw_device", L"cine"});
    return a;
}
std::wstring filter(std::string_view encoder) {
    return encoder == "h264_vulkan" ? L"pad=ceil(iw/2)*2:ceil(ih/2)*2,format=nv12,hwupload" : L"pad=ceil(iw/2)*2:ceil(ih/2)*2,format=yuv420p";
}
bool writer_closed(const fs::path& path) {
#ifdef _WIN32
    // Deny write sharing: completion includes the UE writer closing its handle.
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    CloseHandle(file);
#else
    (void)path;
#endif
    return true;
}
uint32_t be32(const unsigned char* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint32_t le32(const unsigned char* p) { return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }
uint16_t le16(const unsigned char* p) { return uint16_t(unsigned(p[1]) << 8 | p[0]); }
}

std::wstring quote_argument(std::wstring_view value) {
    if (value.find(L'\0') != value.npos) throw std::runtime_error("NUL in process argument");
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'"') out.append(slashes * 2 + 1, L'\\');
        else out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}
std::wstring command_line(const std::vector<std::wstring>& args) {
    if (args.empty()) throw std::runtime_error("Empty command");
    std::wstring out;
    for (const auto& arg : args) { if (!out.empty()) out += L' '; out += quote_argument(arg); }
    if (out.size() >= 32767) throw std::runtime_error("Process command exceeds Windows limit");
    return out;
}
std::vector<std::string> encoder_order(bool wine) {
    if (wine) return {"h264_vulkan", "h264_nvenc", "libopenh264"};
    return {"h264_nvenc", "h264_amf", "h264_qsv", "h264_mf", "h264_vulkan", "libopenh264"};
}
bool is_wine() noexcept {
#ifdef _WIN32
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version");
#else
    return false;
#endif
}
std::string frame_name(unsigned index) {
    if (index > 999999) throw std::runtime_error("Frame number exceeds six digits");
    std::array<char, 16> out{};
    std::snprintf(out.data(), out.size(), "%06u.png", index);
    return out.data();
}
std::string screenshot_command(const fs::path& file) {
    auto path = utf8(file);
    if (path.empty() || path.find_first_of("\"\r\n|") != path.npos || path.find('\0') != path.npos)
        throw std::runtime_error("Unsafe screenshot filename");
    return "Shot filename=\"" + path + "\" -nosuffix";
}
std::vector<std::wstring> encoder_options(std::string_view encoder, Quality quality) {
    const unsigned level = static_cast<unsigned>(quality);
    if (level > 2) throw std::runtime_error("Invalid quality");
    const auto qp = std::to_wstring(18 + level * 5);
    std::vector<std::wstring> a{L"-c:v", widen(encoder)};
    if (encoder == "h264_nvenc") append(a, {L"-rc", L"vbr", L"-cq", qp, L"-b:v", L"0"});
    else if (encoder == "h264_amf") append(a, {L"-rc", L"cqp", L"-qp_i", qp, L"-qp_p", qp});
    else if (encoder == "h264_qsv") append(a, {L"-global_quality", qp});
    else if (encoder == "h264_vulkan") append(a, {L"-rc_mode", L"cqp", L"-qp", qp});
    else if (encoder == "h264_mf" || encoder == "libopenh264") append(a, {L"-b:v", level == 0 ? L"32M" : level == 1 ? L"16M" : L"8M"});
    else throw std::runtime_error("Unknown video encoder");
    return a;
}
std::vector<std::wstring> probe_arguments(const fs::path& exe, std::string_view encoder, Quality quality) {
    auto a = prelude(exe, encoder);
    append(a, {L"-f", L"lavfi", L"-i", L"testsrc2=size=1920x1080:rate=30", L"-frames:v", L"1", L"-vf", filter(encoder)});
    append(a, encoder_options(encoder, quality));
    append(a, {L"-an", L"-f", L"null", L"-"});
    return a;
}
std::vector<std::wstring> encode_arguments(const Request& r, const fs::path& folder, std::string_view encoder, unsigned frames) {
    if (!frames || frames > 216000) throw std::runtime_error("Invalid frame count");
    if (r.settings.fps != 24 && r.settings.fps != 30 && r.settings.fps != 60) throw std::runtime_error("Invalid encode frame rate");
    auto a = prelude(r.cine_folder / "ffmpeg/ffmpeg.exe", encoder);
    std::wstring pattern;
    for (wchar_t c : wide_path(folder / "frames")) { pattern += c; if (c == L'%') pattern += L'%'; }
    pattern += L"/%06d.png";
    append(a, {L"-framerate", std::to_wstring(r.settings.fps), L"-start_number", L"0", L"-i", pattern});
    if (!r.sound.empty()) append(a, {L"-i", wide_path(folder / "music.wav")});
    append(a, {L"-map", L"0:v:0", L"-frames:v", std::to_wstring(frames), L"-vf", filter(encoder)});
    append(a, encoder_options(encoder, r.settings.quality));
    const auto duration = std::to_wstring(double(frames) / r.settings.fps);
    if (!r.sound.empty()) append(a, {L"-map", L"1:a:0", L"-c:a", L"aac", L"-b:a", L"192k", L"-af", L"apad,atrim=duration=" + duration});
    else a.push_back(L"-an");
    append(a, {L"-t", duration, L"-movflags", L"+faststart", L"-progress", L"pipe:1", L"-nostats", wide_path(folder / "video.partial.mp4")});
    return a;
}
void ProgressParser::feed(std::string_view bytes) {
    for (char c : bytes) {
        if (c != '\n') {
            if (pending.size() >= 4096) throw std::runtime_error("Encoder progress line too long");
            pending += c;
            continue;
        }
        if (!pending.empty() && pending.back() == '\r') pending.pop_back();
        if (pending.starts_with("frame=")) {
            std::string_view v(pending.data() + 6, pending.size() - 6);
            while (!v.empty() && v.front() == ' ') v.remove_prefix(1);
            unsigned n = 0;
            auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
            if (ec == std::errc{} && end == v.data() + v.size()) frames = std::max(frames, n);
        }
        if (pending == "progress=end") ended = true;
        pending.clear();
    }
}
bool complete_png(const fs::path& path, unsigned* width, unsigned* height) {
    if (!writer_closed(path)) return false;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto length = in.tellg();
    if (length < 57) return false;
    in.seekg(0);
    std::array<unsigned char, 24> header{};
    in.read(reinterpret_cast<char*>(header.data()), header.size());
    static constexpr std::array<unsigned char, 8> signature{137,80,78,71,13,10,26,10};
    if (!in || !std::equal(signature.begin(), signature.end(), header.begin()) || be32(header.data()+8) != 13 || std::memcmp(header.data()+12, "IHDR", 4)) return false;
    const unsigned w = be32(header.data()+16), h = be32(header.data()+20);
    if (!w || !h || w > 32768 || h > 32768) return false;
    std::array<unsigned char, 12> end{};
    in.seekg(-12, std::ios::end); in.read(reinterpret_cast<char*>(end.data()), end.size());
    static constexpr std::array<unsigned char, 12> iend{0,0,0,0,73,69,78,68,174,66,96,130};
    if (!in || end != iend) return false;
    if (width) *width = w;
    if (height) *height = h;
    return true;
}
bool complete_wav(const fs::path& path, double* seconds) {
    if (!writer_closed(path)) return false;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto length = in.tellg();
    if (length < 44) return false;
    const auto size = uint64_t(length);
    in.seekg(0);
    std::array<unsigned char, 12> header{};
    in.read(reinterpret_cast<char*>(header.data()), header.size());
    if (!in || std::memcmp(header.data(), "RIFF", 4) || std::memcmp(header.data()+8, "WAVE", 4) || uint64_t(le32(header.data()+4))+8 != size) return false;
    uint32_t rate = 0, data = 0;
    uint16_t align = 0;
    for (uint64_t pos = 12; pos + 8 <= size;) {
        in.seekg(std::streamoff(pos));
        std::array<unsigned char, 24> chunk{};
        in.read(reinterpret_cast<char*>(chunk.data()), 8);
        const auto n = le32(chunk.data()+4);
        if (!in || pos + 8 + n > size) return false;
        if (!std::memcmp(chunk.data(), "fmt ", 4)) {
            if (n < 16) return false;
            in.read(reinterpret_cast<char*>(chunk.data()+8), 16);
            const auto channels = le16(chunk.data()+10), bits = le16(chunk.data()+22), block = le16(chunk.data()+20);
            rate = le32(chunk.data()+12);
            // The frame is channels x 16 bits. UE 5.6's submix WAV writer puts the bytes per sample
            // (2) in the block-align field of a stereo file; its byte rate and data are right, so
            // the frame is computed, and either block-align value is accepted.
            align = uint16_t(channels * 2);
            if (!in || le16(chunk.data()+8) != 1 || !channels || channels > 8 || bits != 16 || (block != align && block != 2) || !rate || le32(chunk.data()+16) != rate * align) return false;
        } else if (!std::memcmp(chunk.data(), "data", 4)) data = n;
        pos += 8 + uint64_t(n) + (n & 1);
    }
    if (!rate || !align || !data || data % align) return false;
    if (seconds) *seconds = double(data) / rate / align;
    return true;
}

namespace {
#ifdef _WIN32
class Handle {
    HANDLE value_ = nullptr;
public:
    explicit Handle(HANDLE h = nullptr) : value_(h) {}
    ~Handle() { if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const { return value_; }
    void reset(HANDLE h = nullptr) { if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); value_ = h; }
};
void win_check(BOOL result, const char* operation) {
    if (!result) throw std::runtime_error(std::string(operation) + " failed (Windows " + std::to_string(GetLastError()) + ")");
}
struct ProcessResult { DWORD code; unsigned frames; bool ended; };
ProcessResult run_process(const std::vector<std::wstring>& args, const fs::path& log,
                          std::atomic<bool>& cancel, std::atomic<unsigned>& progress, double timeout) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle err(CreateFileW(log.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (err.get() == INVALID_HANDLE_VALUE) win_check(FALSE, "Open render.log");
    Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (input.get() == INVALID_HANDLE_VALUE) win_check(FALSE, "Open NUL");
    HANDLE raw_read = nullptr, raw_write = nullptr;
    win_check(CreatePipe(&raw_read, &raw_write, &security, 0), "Create progress pipe");
    Handle read(raw_read), write(raw_write);
    win_check(SetHandleInformation(read.get(), HANDLE_FLAG_INHERIT, 0), "Protect progress pipe");
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> storage(bytes);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    win_check(InitializeProcThreadAttributeList(attributes, 1, 0, &bytes), "Initialize child handle list");
    struct AttributeGuard { LPPROC_THREAD_ATTRIBUTE_LIST list; ~AttributeGuard() { DeleteProcThreadAttributeList(list); } } guard{attributes};
    HANDLE inherited[]{input.get(), write.get(), err.get()};
    win_check(UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr), "Set child handle list");
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.get(); startup.StartupInfo.hStdOutput = write.get(); startup.StartupInfo.hStdError = err.get();
    startup.lpAttributeList = attributes;
    auto command = command_line(args);
    PROCESS_INFORMATION process{};
    win_check(CreateProcessW(args.front().c_str(), command.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS | EXTENDED_STARTUPINFO_PRESENT,
                            nullptr, nullptr, &startup.StartupInfo, &process), "Start ffmpeg");
    Handle child(process.hProcess), thread(process.hThread);
    write.reset();
    ProgressParser parser;
    const auto start = Clock::now();
    bool terminated = false;
    try {
        for (;;) {
            if (!terminated && (cancel.load() || std::chrono::duration<double>(Clock::now()-start).count() > timeout)) {
                win_check(TerminateProcess(child.get(), 1), "Cancel ffmpeg");
                terminated = true;
            }
            // Drain a bounded amount before polling process status, even if a child floods stdout.
            for (unsigned n = 0; n < 16; ++n) {
                DWORD available = 0;
                if (!PeekNamedPipe(read.get(), nullptr, 0, nullptr, &available, nullptr)) {
                    if (GetLastError() == ERROR_BROKEN_PIPE) break;
                    win_check(FALSE, "Read encoder progress");
                }
                if (!available) break;
                std::array<char, 4096> buffer{}; DWORD received = 0;
                win_check(ReadFile(read.get(), buffer.data(), std::min<DWORD>(available, DWORD(buffer.size())), &received, nullptr), "Read encoder progress");
                parser.feed(std::string_view(buffer.data(), received));
                progress.store(parser.frames);
            }
            const auto wait = WaitForSingleObject(child.get(), 10);
            if (wait == WAIT_FAILED) win_check(FALSE, "Poll ffmpeg");
            if (wait == WAIT_OBJECT_0) {
                // One final drain catches the progress=end emitted just before exit.
                DWORD available = 0;
                while (PeekNamedPipe(read.get(), nullptr, 0, nullptr, &available, nullptr) && available) {
                    std::array<char, 4096> buffer{}; DWORD received = 0;
                    win_check(ReadFile(read.get(), buffer.data(), std::min<DWORD>(available, DWORD(buffer.size())), &received, nullptr), "Drain encoder progress");
                    parser.feed(std::string_view(buffer.data(), received));
                }
                DWORD code = 0; win_check(GetExitCodeProcess(child.get(), &code), "Get encoder exit status");
                progress.store(parser.frames);
                if (terminated) throw std::runtime_error(cancel.load() ? "Render cancelled" : "Encoder timed out");
                return {code, parser.frames, parser.ended};
            }
        }
    } catch (...) {
        // This handle refers only to the child we created. Never use process-name termination.
        if (WaitForSingleObject(child.get(), 0) == WAIT_TIMEOUT) {
            TerminateProcess(child.get(), 1);
            WaitForSingleObject(child.get(), INFINITE); // Worker only; stop() keeps the DLL loaded.
        }
        throw;
    }
}
#endif
class AsyncIO final : public IO {
    std::future<Result> result_;
    std::atomic<bool> cancelled_{false};
    std::atomic<unsigned> progress_{0};
    void check_cancel() const { if (cancelled_.load()) throw std::runtime_error("Render cancelled"); }
    template<class F> void wait_file(F predicate, const char* what) {
        const auto deadline = Clock::now() + std::chrono::seconds(60);
        for (;;) {
            check_cancel();
            if (predicate()) return;
            if (Clock::now() >= deadline) throw std::runtime_error(std::string(what) + " missing, incomplete or unsupported after 60 seconds");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    Result execute(const Job& j) {
        check_cancel();
        if (j.kind == JobKind::Prepare) {
#ifndef _WIN32
            throw std::runtime_error("Video encoding requires Windows or Proton");
#else
            const auto exe = j.request.cine_folder / "ffmpeg/ffmpeg.exe";
            if (!fs::is_regular_file(exe)) throw std::runtime_error("Missing CINE ffmpeg/ffmpeg.exe");
            const auto now = std::time(nullptr); std::tm date{};
            if (localtime_s(&date, &now)) throw std::runtime_error("Cannot name render folder");
            std::array<char, 32> stamp{};
            if (!std::strftime(stamp.data(), stamp.size(), "%Y%m%d-%H%M%S", &date)) throw std::runtime_error("Cannot format render date");
            const auto root = j.request.cine_folder / "renders";
            fs::create_directories(root);
            auto folder = root / (j.request.preset + "-" + stamp.data());
            if (!fs::create_directory(folder)) throw std::runtime_error("Render folder already exists. Try again in a second");
            fs::create_directory(folder / "frames");
            if (fs::space(folder).available < 256ull * 1024 * 1024) throw std::runtime_error("Less than 256 MiB free in render folder");
            if (!j.encoder.empty()) return {true, {}, j.encoder, folder};
            for (const auto& encoder : encoder_order(is_wine())) {
                check_cancel();
                std::ofstream(folder / "render.log", std::ios::app) << "Probing " << encoder << '\n';
                try {
                    auto r = run_process(probe_arguments(exe, encoder, j.request.settings.quality), folder / "render.log", cancelled_, progress_, 30);
                    if (r.code == 0) return {true, {}, encoder, folder};
                } catch (const std::exception& e) {
                    check_cancel();
                    std::ofstream(folder / "render.log", std::ios::app) << e.what() << '\n';
                }
            }
            throw std::runtime_error("No H.264 encoder passed its probe. See " + utf8(folder / "render.log"));
#endif
        }
        if (j.kind == JobKind::Frames) {
            unsigned first_w = 0, first_h = 0;
            for (unsigned i = 0; i < j.frames; ++i) {
                unsigned w = 0, h = 0;
                wait_file([&] { return complete_png(j.folder / "frames" / frame_name(i), &w, &h); }, "PNG frame");
                if (!i) { first_w = w; first_h = h; }
                if (w != first_w || h != first_h) throw std::runtime_error("Window size changed during frame capture");
            }
        } else if (j.kind == JobKind::Audio) {
            double seconds = 0;
            wait_file([&] { return complete_wav(j.folder / "music.wav", &seconds); }, "Recorded PCM WAV");
            if (seconds + 0.1 < double(j.frames) / j.request.settings.fps)
                throw std::runtime_error("Recorded music is shorter than the take");
        } else if (j.kind == JobKind::Encode) {
#ifndef _WIN32
            throw std::runtime_error("Video encoding requires Windows or Proton");
#else
            const auto r = run_process(encode_arguments(j.request, j.folder, j.encoder, j.frames), j.folder / "render.log", cancelled_, progress_, 86400);
            check_cancel();
            if (r.code || !r.ended || r.frames != j.frames) throw std::runtime_error("Encoder failed or did not encode every frame. See render.log");
            if (fs::file_size(j.folder / "video.partial.mp4") < 32) throw std::runtime_error("Encoder output is empty");
            fs::rename(j.folder / "video.partial.mp4", j.folder / "video.mp4");
#endif
        }
        return {};
    }
public:
    ~AsyncIO() override { cancel(); if (result_.valid()) result_.wait(); }
    void launch(Job j) override {
        if (busy()) throw std::runtime_error("Render worker is still busy");
        cancelled_.store(false); progress_.store(0);
        result_ = std::async(std::launch::async, [this, j = std::move(j)] {
            try { return execute(j); }
            catch (const std::exception& e) { return Result{false, e.what(), {}, {}}; }
            catch (...) { return Result{false, "Unknown render worker failure", {}, {}}; }
        });
    }
    std::optional<Result> poll() override {
        if (!result_.valid() || result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
        return result_.get();
    }
    void cancel() noexcept override { cancelled_.store(true); }
    bool busy() const noexcept override { return result_.valid(); }
    unsigned progress() const noexcept override { return progress_.load(); }
};
}
std::unique_ptr<IO> make_io() { return std::make_unique<AsyncIO>(); }

Renderer::Renderer(cssx::Client& client, Take take, std::unique_ptr<IO> io)
    : client_(client), take_(std::move(take)), io_(std::move(io)) {
    if (!io_) throw std::runtime_error("Render IO is required");
}
Renderer::~Renderer() {
    // The owner must honour stop()==false before destroying or unloading the extension.
    stop();
}
bool Renderer::active() const noexcept {
    return status_.state == State::Probing || status_.state == State::Frames ||
           status_.state == State::Audio || status_.state == State::Encoding;
}
Json Renderer::cdo(const char* name) {
    auto result = client_.request({{"op", "class_default"}, {"class", name}});
    require_object(result, name);
    return result;
}
void Renderer::resolve() {
    system_ = cdo("KismetSystemLibrary"); gameplay_ = cdo("GameplayStatics");
    auto instance = client_.call(gameplay_, "GetGameInstance", Json::array({request_.world}));
    require_object(instance, "game instance");
    engine_ = client_.call(system_, "GetOuterObject", Json::array({instance}));
    require_object(engine_, "game engine");
    saved_rate_ = client_.get(engine_, "FixedFrameRate");
    saved_fixed_ = client_.get(engine_, "bUseFixedFrameRate");
    if (!saved_fixed_.is_boolean() || !saved_rate_.is_number() || !std::isfinite(saved_rate_.get<double>()) || saved_rate_.get<double>() <= 0)
        throw std::runtime_error("Unexpected engine frame-rate properties");
    auto settings = client_.get(engine_, "GameUserSettings");
    require_object(settings, "game user settings");
    if (client_.call(settings, "IsHDREnabled").get<bool>()) throw std::runtime_error("PNG rendering requires HDR to be off");
    if (!request_.sound.empty()) resolve_audio();
}
void Renderer::resolve_audio() {
    mixer_ = cdo("AudioMixerBlueprintLibrary");
    submix_ = client_.request({{"op", "load"}, {"path", request_.submix}});
    sound_ = client_.request({{"op", "load"}, {"path", request_.sound}});
    require_object(submix_, "CINE recording submix"); require_object(sound_, "scene SoundWave");
    if (!client_.get(sound_, "bEnableSubmixSends").get<bool>()) throw std::runtime_error("Scene sound disables submix sends");
    if (client_.get(submix_, "bAutoDisable").get<bool>()) throw std::runtime_error("CINE recording submix must have Auto Disable off");
}

bool Renderer::start(Request request) noexcept {
    if (active() || status_.restoring || io_->busy() || audio_flush_pending_) return false;
    try {
        status_ = {};
        if (!take_.begin || !take_.frame || !take_.end) throw std::runtime_error("Render take callbacks are missing");
        if (request.settings.size != Size::Window) throw std::runtime_error("Only game-window size supports one Shot per frame");
        if (static_cast<unsigned>(request.settings.quality) > 2) throw std::runtime_error("Invalid render quality");
        if (!token(request.preset)) throw std::runtime_error("Invalid preset name for render folder");
        if (!request.cine_folder.is_absolute()) throw std::runtime_error("CINE folder must be absolute");
        screenshot_command(request.cine_folder / "renders/check.png");
        if (!std::isfinite(request.audio_offset) || request.audio_offset < 0 || request.audio_offset > 86400)
            throw std::runtime_error("Invalid music start offset");
        status_.total = frame_count(request);
        require_object(request.world, "render world"); require_object(request.pawn, "render pawn");
        request_ = std::move(request);
        frames_started_ = frames_ended_ = audio_ended_ = frames_verified_ = false;
        have_serial_ = have_time_ = false;
        component_ = nullptr;
        resolve();
        const auto exe = request_.cine_folder / "ffmpeg/ffmpeg.exe";
        if (exe != cached_exe_) cached_encoder_.clear();
        status_.encoder = cached_encoder_;
        status_.state = State::Probing; status_.line = "Checking video encoder";
        launch(JobKind::Prepare);
        return true;
    } catch (const std::exception& e) { fail(e.what()); }
    catch (...) { fail("Unknown render start failure"); }
    return false;
}
void Renderer::launch(JobKind kind) {
    io_->launch(Job{kind, request_, status_.folder, status_.encoder, status_.total});
    job_pending_ = true;
}
void Renderer::fixed(bool enabled) {
    if (enabled) {
        rate_dirty_ = true;
        client_.set(engine_, "FixedFrameRate", request_.settings.fps);
        if (std::abs(client_.get(engine_, "FixedFrameRate").get<double>() - request_.settings.fps) > 0.001)
            throw std::runtime_error("Engine refused fixed frame rate");
    }
    fixed_dirty_ = true;
    client_.set(engine_, "bUseFixedFrameRate", enabled);
    if (client_.get(engine_, "bUseFixedFrameRate") != Json(enabled)) throw std::runtime_error("Engine refused fixed timestep setting");
}
bool Renderer::restore_engine() noexcept {
    if (!rate_dirty_ && !fixed_dirty_) return true;
    try {
        if (!client_.request({{"op", "valid"}, {"target", engine_}}).get<bool>()) {
            rate_dirty_ = fixed_dirty_ = false;
            return true; // The owner was destroyed. Never write a replacement object.
        }
    } catch (...) { return false; }
    if (rate_dirty_) try {
        client_.set(engine_, "FixedFrameRate", saved_rate_);
        if (client_.get(engine_, "FixedFrameRate") == saved_rate_) rate_dirty_ = false;
    } catch (...) {}
    if (fixed_dirty_) try {
        client_.set(engine_, "bUseFixedFrameRate", saved_fixed_);
        if (client_.get(engine_, "bUseFixedFrameRate") == saved_fixed_) fixed_dirty_ = false;
    } catch (...) {}
    return !rate_dirty_ && !fixed_dirty_;
}
void Renderer::finish_audio() {
    if (object(component_)) {
        if (client_.request({{"op", "valid"}, {"target", component_}}).get<bool>()) {
            client_.call(component_, "Stop");
            if (send_dirty_) { client_.call(component_, "SetSubmixSend", Json::array({submix_, 0.0})); send_dirty_ = false; }
            client_.call(component_, "K2_DestroyComponent", Json::array({component_}));
        }
        component_ = nullptr; send_dirty_ = false;
    }
    if (recording_) {
        if (client_.request({{"op", "valid"}, {"target", request_.world}}).get<bool>()) {
            client_.call(mixer_, "StopRecordingOutput", Json::array({request_.world, 1, "music", utf8(status_.folder), submix_, nullptr}));
            audio_flush_pending_ = true;
        }
        recording_ = false;
    }
    if (submix_dirty_) {
        if (client_.request({{"op", "valid"}, {"target", request_.world}}).get<bool>())
            client_.call(submix_, "SetSubmixOutputVolume", Json::array({request_.world, 1.0}));
        submix_dirty_ = false;
    }
}
bool Renderer::restore() noexcept {
    bool ok = true;
    try { finish_audio(); } catch (...) { ok = false; }
    if (take_dirty_) try { take_.end(); take_dirty_ = false; } catch (...) { ok = false; }
    if (!restore_engine()) ok = false;
    status_.restoring = !ok;
    return ok;
}
void Renderer::begin_frames() {
    fixed(true);
    take_dirty_ = true;
    take_.begin(Pass::Frames);
    launch(JobKind::Frames);
    status_.state = State::Frames;
    status_.line = "Rendering frames";
    frames_started_ = true;
}
void Renderer::begin_audio(double now) {
    if (request_.sound.empty()) { begin_encode(); return; }
    // Bridge handles are weak. Reload assets after the potentially long frame pass.
    resolve_audio();
    fixed(false); // The audio pass is real time, even if the player's original setting was fixed.
    take_dirty_ = true; take_.begin(Pass::Audio);
    auto music = client_.request({{"op", "load"}, {"path", request_.music_class}});
    require_object(music, "game music sound class");
    component_ = client_.call(gameplay_, "CreateSound2D", Json::array({request_.world, sound_, 1.0, 1.0, request_.audio_offset, nullptr, false, false}));
    require_object(component_, "render audio component");
    client_.set(component_, "SoundClassOverride", music);
    client_.set(component_, "bIsUISound", true);
    submix_dirty_ = true;
    client_.call(submix_, "SetSubmixOutputVolume", Json::array({request_.world, 0.0}));
    send_dirty_ = true;
    client_.call(component_, "SetSubmixSend", Json::array({submix_, 1.0}));
    recording_ = true;
    client_.call(mixer_, "StartRecordingOutput", Json::array({request_.world, double(status_.total) / request_.settings.fps + 1.0, submix_}));
    client_.call(component_, "Play", Json::array({request_.audio_offset}));
    audio_start_ = now;
    status_.state = State::Audio; status_.line = "Recording scene music";
}
void Renderer::begin_encode() {
    if (!restore()) throw std::runtime_error("Render settings could not be restored");
    status_.state = State::Encoding; status_.line = "Encoding video";
    launch(JobKind::Encode);
}
void Renderer::handle_result(const Result& result, double now) {
    if (!result.ok) {
        if (status_.state == State::Probing || status_.state == State::Encoding) cached_encoder_.clear();
        throw std::runtime_error(result.error);
    }
    if (status_.state == State::Probing) {
        if (result.encoder.empty() || result.folder.empty()) throw std::runtime_error("Encoder probe returned incomplete results");
        status_.folder = result.folder; status_.encoder = result.encoder;
        cached_encoder_ = result.encoder; cached_exe_ = request_.cine_folder / "ffmpeg/ffmpeg.exe";
        begin_frames();
    } else if (status_.state == State::Frames) {
        frames_verified_ = true;
        if (frames_ended_) begin_audio(now);
    } else if (status_.state == State::Audio) {
        audio_flush_pending_ = false;
        begin_encode();
    } else if (status_.state == State::Encoding) {
        status_.state = State::Done; status_.line = "Video saved";
    }
}
void Renderer::tick(uint64_t serial, double real_seconds, const Json& world, const Json& pawn) noexcept {
    if (!active()) {
        if (status_.restoring || io_->busy() || audio_flush_pending_) stop();
        return;
    }
    try {
        if (!std::isfinite(real_seconds) || (have_time_ && real_seconds < last_time_)) throw std::runtime_error("Render clock is not monotonic");
        last_time_ = real_seconds; have_time_ = true;
        if (world != request_.world || pawn != request_.pawn) throw std::runtime_error("World or player changed during render");
        if (have_serial_ && serial <= last_serial_) {
            if (serial == last_serial_) return;
            throw std::runtime_error("Render frame serial moved backwards");
        }
        if (status_.state == State::Frames && frames_started_ && !frames_ended_ && have_serial_ && serial != last_serial_ + 1)
            throw std::runtime_error("Render callback missed an engine frame");
        last_serial_ = serial; have_serial_ = true;
        if (job_pending_) {
            if (auto result = io_->poll()) { job_pending_ = false; handle_result(*result, real_seconds); }
        }
        if (status_.state == State::Frames) {
            if (status_.frames < status_.total) {
                take_.frame(status_.frames, double(status_.frames) / request_.settings.fps);
                client_.call(system_, "ExecuteConsoleCommand", Json::array({request_.world, screenshot_command(status_.folder / "frames" / frame_name(status_.frames)), nullptr}));
                ++status_.frames;
            } else if (!frames_ended_) {
                take_.end(); take_dirty_ = false;
                if (!restore_engine()) throw std::runtime_error("Fixed timestep restoration failed");
                frames_ended_ = true; status_.line = "Checking captured frames";
                if (frames_verified_) begin_audio(real_seconds);
            }
        } else if (status_.state == State::Audio && !audio_ended_ && real_seconds - audio_start_ >= double(status_.total) / request_.settings.fps) {
            finish_audio();
            take_.end(); take_dirty_ = false;
            if (!restore_engine()) throw std::runtime_error("Audio timestep restoration failed");
            audio_ended_ = true; status_.line = "Saving scene music";
            launch(JobKind::Audio);
        } else if (status_.state == State::Encoding) status_.encoded = std::min(status_.total, io_->progress());
    } catch (const std::exception& e) { fail(e.what()); }
    catch (...) { fail("Unknown render tick failure"); }
}
void Renderer::fail(const std::string& reason) noexcept {
    io_->cancel();
    try { status_.state = State::Failed; status_.line = "Render failed"; status_.error = reason; } catch (...) {}
    restore();
}
void Renderer::cancel() noexcept {
    if (active()) fail("Render cancelled");
}
bool Renderer::stop() noexcept {
    if (active()) cancel();
    bool ok = restore();
    try {
        if (auto result = io_->poll()) {
            job_pending_ = false;
            if (flush_job_) {
                flush_job_ = false; audio_flush_pending_ = false;
                if (!result->ok) { status_.error += "; " + result->error; }
            }
        }
        if (audio_flush_pending_ && !io_->busy()) {
            // Do not cancel this drain: StopRecordingOutput owns an asynchronous UE writer.
            io_->launch(Job{JobKind::Audio, request_, status_.folder, status_.encoder, 0});
            job_pending_ = true; flush_job_ = true;
        }
    } catch (...) { ok = false; }
    return ok && !io_->busy() && !audio_flush_pending_;
}
}
