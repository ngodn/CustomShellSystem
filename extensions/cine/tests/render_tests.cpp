#include "render.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

using namespace cine::video;
namespace {
unsigned checks = 0;
void expect(bool value, const std::string& message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F f, const std::string& message) {
    bool threw = false; try { f(); } catch (...) { threw = true; }
    expect(threw, message);
}
Json obj(uint64_t id) { return {{"$object", id}}; }
struct Host {
    CssxHost api{CSSX_ABI, sizeof(CssxHost), this, request};
    double rate = 48;
    bool fixed = false, hdr = false, valid_engine = true, valid_world = true;
    unsigned calls = 0, shots = 0, plays = 0, starts = 0, stops = 0, destroyed = 0, fail_at = 0;
    double submix_volume = 1;
    std::string fail_key;
    bool fail_once = true;
    std::vector<std::string> trace;
    static int request(void* ctx, const char* input, CssxSink sink, void* output) {
        try {
            auto text = static_cast<Host*>(ctx)->handle(Json::parse(input)).dump();
            sink(output, text.data(), text.size()); return 1;
        } catch (const std::exception& e) {
            auto text = Json{{"error", e.what()}}.dump(); sink(output, text.data(), text.size()); return 0;
        }
    }
    Json handle(const Json& j) {
        ++calls;
        auto op = j.at("op").get<std::string>();
        auto key = op + ":" + j.value("property", j.value("function", j.value("class", std::string{})));
        trace.push_back(key);
        if (calls == fail_at) throw std::runtime_error("injected bridge call " + std::to_string(calls));
        if (key == fail_key) { if (fail_once) fail_key.clear(); throw std::runtime_error("injected " + key); }
        if (op == "class_default") {
            const auto name = j.at("class").get<std::string>();
            if (name == "KismetSystemLibrary") return obj(10);
            if (name == "GameplayStatics") return obj(11);
            if (name == "AudioMixerBlueprintLibrary") return obj(12);
            throw std::runtime_error("unknown CDO");
        }
        if (op == "load") {
            auto path = j.at("path").get<std::string>();
            if (path.find("SM_CINE") != path.npos) return obj(20);
            if (path == "/Music") return obj(22);
            if (path == "/Sound") return obj(21);
            throw std::runtime_error("unknown load");
        }
        if (op == "valid") {
            if (j.at("target") == obj(1)) return valid_engine;
            if (j.at("target") == obj(2)) return valid_world;
            return true;
        }
        if (op == "get") {
            auto property = j.at("property").get<std::string>();
            if (property == "FixedFrameRate") return rate;
            if (property == "bUseFixedFrameRate") return fixed;
            if (property == "GameUserSettings") return obj(4);
            if (property == "bEnableSubmixSends") return true;
            if (property == "bAutoDisable") return false;
        }
        if (op == "set") {
            auto property = j.at("property").get<std::string>();
            if (property == "FixedFrameRate") rate = j.at("value").get<double>();
            else if (property == "bUseFixedFrameRate") fixed = j.at("value").get<bool>();
            else if (property != "bIsUISound" && property != "SoundClassOverride") throw std::runtime_error("unexpected set");
            return j.at("value");
        }
        if (op == "call") {
            auto fn = j.at("function").get<std::string>();
            auto a = j.at("args");
            if (fn == "GetGameInstance") { expect(a == Json::array({obj(2)}), "world to GetGameInstance"); return obj(3); }
            if (fn == "GetOuterObject") { expect(a == Json::array({obj(3)}), "game instance outer"); return obj(1); }
            if (fn == "IsHDREnabled") return hdr;
            if (fn == "ExecuteConsoleCommand") {
                expect(a.at(1).get<std::string>().ends_with("-nosuffix"), "screenshots disable suffix");
                expect(a.at(2).is_null(), "default controller console route"); ++shots;
            } else if (fn == "CreateSound2D") { expect(a.size() == 8 && !a.at(7).get<bool>(), "owned audio component"); return obj(23); }
            else if (fn == "SetSubmixOutputVolume") submix_volume = a.at(1).get<double>();
            else if (fn == "SetSubmixSend") expect(a.at(0) == obj(20), "dedicated submix send");
            else if (fn == "StartRecordingOutput") { expect(a.at(2) == obj(20), "record dedicated submix"); ++starts; }
            else if (fn == "StopRecordingOutput") { expect(a.size() == 6 && a.at(1) == 1 && a.at(4) == obj(20), "WAV export signature"); ++stops; }
            else if (fn == "Play") ++plays;
            else if (fn == "K2_DestroyComponent") ++destroyed;
            else if (fn != "Stop") throw std::runtime_error("unexpected call " + fn);
            return nullptr;
        }
        throw std::runtime_error("unexpected request " + key);
    }
};
struct FakeIO final : IO {
    std::optional<Result> ready;
    bool pending = false, cancelled = false;
    unsigned launches = 0, encoded = 0;
    Job job;
    void launch(Job value) override { expect(!pending, "one worker job at a time"); job = std::move(value); pending = true; cancelled = false; ++launches; }
    std::optional<Result> poll() override {
        if (!ready) return {};
        pending = false;
        auto result = ready; ready.reset(); return result;
    }
    void cancel() noexcept override { cancelled = true; }
    bool busy() const noexcept override { return pending; }
    unsigned progress() const noexcept override { return encoded; }
    void done(bool success = true) { ready = Result{success, success ? "" : "worker failure", "libopenh264", fs::path("/cine/renders/test-20261027-000000")}; }
};
struct Fixture {
    Host host;
    cssx::Client client{&host.api};
    FakeIO* io = nullptr;
    unsigned begins = 0, ends = 0, applied = 0;
    bool fail_begin = false, fail_end = false, fail_frame = false;
    Pass pass = Pass::Frames;
    std::vector<double> times;
    std::unique_ptr<Renderer> render;
    Request request;
    uint64_t serial = 0;
    double time = 100;
    Fixture(bool music = false) {
        auto worker = std::make_unique<FakeIO>(); io = worker.get();
        Take take;
        take.begin = [this](Pass p) { ++begins; pass = p; if (fail_begin) throw std::runtime_error("begin failure"); };
        take.frame = [this](unsigned i, double t) { expect(i == applied, "frame index order"); ++applied; times.push_back(t); if (fail_frame) throw std::runtime_error("pose failure"); };
        take.end = [this] { ++ends; if (fail_end) throw std::runtime_error("end failure"); };
        render = std::make_unique<Renderer>(client, std::move(take), std::move(worker));
        request.cine_folder = "/cine"; request.preset = "test"; request.seconds = 0.1;
        request.world = obj(2); request.pawn = obj(5);
        if (music) { request.sound = "/Sound"; request.music_class = "/Music"; }
    }
    void tick() { render->tick(++serial, time += 1.0 / 30, request.world, request.pawn); }
    void start_frames() { expect(render->start(request), "start accepted"); io->done(); tick(); }
    void finish_frames() { while (render->status().frames < render->status().total) tick(); tick(); io->done(); tick(); }
    void drain() {
        for (unsigned i = 0; i < 4; ++i) { if (io->busy()) io->done(); if (render->stop()) return; }
    }
    ~Fixture() { fail_end = false; host.fail_key.clear(); drain(); }
};
void bridge_failure_tests() {
    unsigned baseline_calls = 0;
    for (unsigned failure = 0; failure <= baseline_calls; ++failure) {
        Fixture f(true); f.host.fail_at = failure;
        f.render->start(f.request);
        for (unsigned n = 0; n < 50 && f.render->active(); ++n) {
            if (f.io->busy() && (f.io->job.kind != JobKind::Frames || f.render->status().frames == f.render->status().total)) f.io->done();
            f.tick();
        }
        if (failure == 0) {
            expect(f.render->status().state == State::Done, "baseline audio render completes");
            baseline_calls = f.host.calls;
        } else expect(f.render->status().state == State::Failed, "bridge failure " + std::to_string(failure) + " is visible");
        f.drain();
        expect(f.host.rate == 48 && !f.host.fixed && f.host.submix_volume == 1, "restore after bridge call " + std::to_string(failure));
        expect(f.render->stop(), "unload after bridge failure retry");
    }
}
void state_tests() {
    {
        Fixture f;
        f.tick(); expect(f.host.calls == 0 && f.io->launches == 0, "idle does no bridge or worker work");
        f.start_frames(); expect(f.host.fixed && f.host.rate == 30 && f.host.shots == 1, "fixed frame zero");
        f.render->tick(f.serial, f.time, f.request.world, f.request.pawn);
        expect(f.host.shots == 1, "duplicate callback does not capture twice");
        expect(!f.render->start(f.request), "reject overlapping render");
        f.finish_frames();
        expect(f.render->status().state == State::Encoding && !f.host.fixed && f.host.rate == 48, "restore before encode");
        expect(f.times == std::vector<double>({0, 1.0/30, 2.0/30}), "fixed timeline samples");
        f.io->encoded = 2; f.tick(); expect(f.render->status().encoded == 2, "encoding progress");
        f.io->done(); f.tick(); expect(f.render->status().state == State::Done, "complete render");
        const auto calls = f.host.calls; f.tick(); expect(f.host.calls == calls, "done makes no engine calls");
        f.applied = 0; expect(f.render->start(f.request), "restart after completion");
        expect(f.io->job.encoder == "libopenh264", "session winner reused");
    }
    {
        Fixture f(true); f.host.fixed = true; f.start_frames();
        expect(f.host.plays == 0 && f.host.starts == 0, "silent frame pass");
        f.finish_frames();
        expect(f.render->status().state == State::Audio && f.pass == Pass::Audio && !f.host.fixed, "real-time replay");
        expect(f.host.plays == 1 && f.host.starts == 1 && f.host.submix_volume == 0, "isolated recording with muted return");
        f.time += 1; f.tick();
        expect(f.host.stops == 1 && f.host.destroyed == 1 && f.host.submix_volume == 1 && f.host.fixed && f.host.rate == 48, "audio teardown restores originals");
        expect(f.io->job.kind == JobKind::Audio, "wait for WAV worker");
        f.io->done(); f.tick(); expect(f.render->status().state == State::Encoding, "WAV complete before mux");
    }
    for (const auto key : {"set:FixedFrameRate", "set:bUseFixedFrameRate", "call:ExecuteConsoleCommand"}) {
        Fixture f; f.host.fail_key = key; f.start_frames();
        expect(f.render->status().state == State::Failed && !f.host.fixed && f.host.rate == 48, std::string("rollback on ") + key);
    }
    for (const auto key : {"call:CreateSound2D", "set:SoundClassOverride", "set:bIsUISound", "call:SetSubmixOutputVolume", "call:SetSubmixSend", "call:StartRecordingOutput", "call:Play"}) {
        Fixture f(true); f.start_frames(); f.host.fail_key = key; f.finish_frames();
        expect(f.render->status().state == State::Failed && !f.host.fixed && f.host.rate == 48, std::string("audio rollback on ") + key);
        f.drain(); expect(f.host.submix_volume == 1, "submix restored on audio failure");
    }
    {
        Fixture f; f.fail_begin = true; f.start_frames();
        expect(f.render->status().state == State::Failed && f.ends == 1 && f.host.rate == 48, "partial begin unwound");
    }
    {
        Fixture f; f.fail_frame = true; f.start_frames();
        expect(f.render->status().state == State::Failed && f.host.shots == 0 && !f.host.fixed, "pose failure before capture");
    }
    {
        Fixture f; f.start_frames(); f.host.fail_key = "set:FixedFrameRate"; f.host.fail_once = false;
        f.render->cancel(); expect(f.render->status().restoring && !f.render->stop(), "refused restore prevents unload");
        expect(!f.host.fixed, "restore other field despite rate failure");
        f.host.fail_key.clear(); f.io->done(); expect(f.render->stop() && f.host.rate == 48, "restore retries exact original");
    }
    {
        Fixture f; f.start_frames(); f.fail_end = true; f.render->cancel();
        expect(f.render->status().restoring && !f.host.fixed, "take restore failure does not skip engine restore");
        f.fail_end = false; f.drain(); expect(!f.render->status().restoring, "take restore retried");
    }
    {
        Fixture f; f.start_frames();
        f.render->tick(++f.serial, f.time, obj(99), f.request.pawn);
        expect(f.render->status().state == State::Failed && f.host.rate == 48, "world change restores persistent engine");
    }
    {
        Fixture f; f.start_frames(); f.host.valid_engine = false;
        f.render->cancel(); f.io->done(); expect(f.render->stop(), "destroyed engine releases ownership without stale writes");
    }
    {
        Fixture f; f.start_frames(); f.render->tick(f.serial + 2, f.time, f.request.world, f.request.pawn);
        expect(f.render->status().state == State::Failed, "missed frame fails visibly");
    }
    {
        Fixture f; f.start_frames(); f.render->tick(++f.serial, f.time - 1, f.request.world, f.request.pawn);
        expect(f.render->status().state == State::Failed, "bad clock fails");
    }
    {
        Fixture f; f.host.hdr = true;
        expect(!f.render->start(f.request) && f.io->launches == 0 && !f.host.fixed, "HDR refused before mutation");
    }
    for (auto state : {State::Probing, State::Frames, State::Audio, State::Encoding}) {
        Fixture f(state == State::Audio);
        expect(f.render->start(f.request), "cancel fixture start");
        if (state != State::Probing) { f.io->done(); f.tick(); }
        if (state == State::Audio || state == State::Encoding) f.finish_frames();
        expect(f.render->status().state == state, "cancel reaches requested state");
        f.render->cancel();
        expect(f.render->status().state == State::Failed && f.host.rate == 48 && !f.host.fixed, "cancel restores from each state");
        f.drain(); expect(f.render->stop(), "cancel teardown completes");
    }
    {
        Fixture f; f.start_frames(); f.io->done(false); f.tick();
        expect(f.render->status().state == State::Failed && f.host.rate == 48, "disk failure restores");
    }
    {
        Fixture f; f.start_frames(); f.finish_frames(); f.io->done(false); f.tick();
        expect(f.render->status().state == State::Failed, "encoder failure reported");
        f.drain(); f.applied = 0; expect(f.render->start(f.request), "retry failed encoding");
        expect(f.io->job.encoder.empty(), "failed winner evicted");
    }
    {
        Fixture f; f.request.settings.size = Size::HD1080; expect(!f.render->start(f.request), "unsupported size refused");
        f.request.settings.size = Size::Window; f.request.settings.fps = 25; expect(!f.render->start(f.request), "unsupported fps refused");
        f.request.settings.fps = 24; f.request.preset = "../evil"; expect(!f.render->start(f.request), "path traversal refused");
        f.request.preset = "test"; f.request.seconds = std::numeric_limits<double>::quiet_NaN(); expect(!f.render->start(f.request), "NaN duration refused");
    }
}
std::vector<std::wstring> decode_windows(const std::wstring& command) {
    // Independent CRT argument decoder for quote/escape round trips.
    std::vector<std::wstring> args;
    size_t i = 0;
    while (i < command.size()) {
        while (i < command.size() && command[i] == L' ') ++i;
        if (i == command.size()) break;
        std::wstring arg; bool quoted = false;
        while (i < command.size() && (quoted || command[i] != L' ')) {
            size_t n = 0; while (i < command.size() && command[i] == L'\\') { ++n; ++i; }
            if (i < command.size() && command[i] == L'"') {
                arg.append(n/2, L'\\');
                if (n%2) arg += L'"'; else quoted = !quoted;
                ++i;
            } else {
                arg.append(n, L'\\');
                if (i < command.size() && (quoted || command[i] != L' ')) arg += command[i++];
            }
        }
        args.push_back(arg);
    }
    return args;
}
void pure_tests() {
    const std::vector<std::wstring> args{L"C:\\Program Files\\CINE\\ffmpeg.exe", L"", L"hello world", L"tail\\", L"a\\\"b", L"two\\\\\"quotes", L"%PATH% & | < > ^", L"unicode \u2665"};
    expect(decode_windows(command_line(args)) == args, "Windows argument round trip");
    rejects([] { quote_argument(std::wstring(L"x\0y", 3)); }, "embedded NUL refused");
    rejects([] { command_line({std::wstring(33000, L'x')}); }, "long command refused");
    expect(encoder_order(true) == std::vector<std::string>({"h264_vulkan", "h264_nvenc", "libopenh264"}), "Wine order");
    expect(encoder_order(false) == std::vector<std::string>({"h264_nvenc", "h264_amf", "h264_qsv", "h264_mf", "h264_vulkan", "libopenh264"}), "Windows order");
#ifndef _WIN32
    expect(!is_wine(), "portable Wine detection stub");
#endif
    expect(frame_name(0) == "000000.png" && frame_name(99) == "000099.png" && frame_name(999999) == "999999.png", "frame names");
    rejects([] { frame_name(1000000); }, "frame overflow refused");
    expect(screenshot_command("/CINE space/000000.png") == "Shot filename=\"/CINE space/000000.png\" -nosuffix", "UE quoting");
    rejects([] { screenshot_command("/CINE/x\"|quit"); }, "console injection refused");
    for (const auto& encoder : encoder_order(false)) {
        for (auto quality : {Quality::High, Quality::Medium, Quality::Small}) {
            auto probe = probe_arguments("/cine/ffmpeg.exe", encoder, quality);
            expect(decode_windows(command_line(probe)) == probe, "probe argv round trip");
            expect(std::find(probe.begin(), probe.end(), L"1") != probe.end(), "one frame probe");
            Request r; r.cine_folder = "/cine"; r.sound = "/Sound";
            auto enc = encode_arguments(r, "/cine/renders/test", encoder, 90);
            expect(decode_windows(command_line(enc)) == enc, "encode argv round trip");
            expect(std::find(enc.begin(), enc.end(), L"aac") != enc.end(), "native AAC");
            expect(std::find(enc.begin(), enc.end(), L"+faststart") != enc.end(), "MP4 faststart");
            expect(enc.back().ends_with(L"video.partial.mp4"), "unpublished partial output");
        }
    }
    {
        Request r; r.cine_folder = "/cine";
        auto args = encode_arguments(r, "/100% cinema/clip", "libopenh264", 6);
        expect(std::find(args.begin(), args.end(), L"/100%% cinema/clip/frames/%06d.png") != args.end(), "literal percent in image sequence folder");
        r.settings.fps = 0;
        rejects([&] { encode_arguments(r, "/cine", "libopenh264", 6); }, "zero encode frame rate refused");
    }
    ProgressParser parser;
    parser.feed("frame=1\nout_time_us=123\nfra"); parser.feed("me= 40\r\nprogress=en"); parser.feed("d\nframe=-2\nframe=5\n");
    expect(parser.frames == 40 && parser.ended, "chunked progress and monotonic count");
    rejects([&] { parser.feed(std::string(4097, 'x')); }, "bounded progress line");
}
void file_tests(const fs::path& root) {
    fs::create_directories(root);
    const auto data = root / ("files-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    expect(fs::create_directory(data), "exclusive test folder");
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{data};
    const auto png = data / "frame.png", wav = data / "music.wav";
    expect(!complete_png(png) && !complete_wav(wav), "missing files incomplete");
    const std::array<unsigned char, 69> image{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,2,0,0,0,144,119,83,222,0,0,0,12,73,68,65,84,8,215,99,248,207,192,0,0,3,1,1,0,24,221,141,176,0,0,0,0,73,69,78,68,174,66,96,130};
    for (size_t n = 0; n < image.size(); ++n) {
        { std::ofstream out(png, std::ios::binary); out.write(reinterpret_cast<const char*>(image.data()), std::streamsize(n)); }
        expect(!complete_png(png), "truncated PNG refused");
    }
    { std::ofstream out(png, std::ios::binary); out.write(reinterpret_cast<const char*>(image.data()), image.size()); }
    unsigned w = 0, h = 0;
    expect(complete_png(png, &w, &h) && w == 1 && h == 1, "PNG complete dimensions");
    std::string bytes = "RIFF";
    auto u16 = [&](uint16_t n) { bytes += char(n); bytes += char(n >> 8); };
    auto u32 = [&](uint32_t n) { for (unsigned shift = 0; shift < 32; shift += 8) bytes += char(n >> shift); };
    u32(40); bytes += "WAVEfmt "; u32(16); u16(1); u16(2); u32(48000); u32(192000); u16(4); u16(16); bytes += "data"; u32(4); u32(0);
    for (size_t n = 0; n < bytes.size(); ++n) {
        { std::ofstream out(wav, std::ios::binary); out.write(bytes.data(), std::streamsize(n)); }
        expect(!complete_wav(wav), "truncated WAV refused");
    }
    { std::ofstream out(wav, std::ios::binary); out.write(bytes.data(), std::streamsize(bytes.size())); }
    double seconds = 0;
    expect(complete_wav(wav, &seconds) && std::abs(seconds - 1.0/48000) < 1e-10, "PCM WAV complete duration");
    {   // UE 5.6's submix writer: stereo 16-bit with block align 2 (bytes per sample), byte rate right.
        std::string ue = bytes; ue[32] = char(2);
        { std::ofstream out(wav, std::ios::binary); out.write(ue.data(), std::streamsize(ue.size())); }
        expect(complete_wav(wav, &seconds) && std::abs(seconds - 1.0/48000) < 1e-10, "UE block-align quirk accepted");
        ue = bytes; ue[32] = char(3);
        { std::ofstream out(wav, std::ios::binary); out.write(ue.data(), std::streamsize(ue.size())); }
        expect(!complete_wav(wav), "other block-align values refused");
        { std::ofstream out(wav, std::ios::binary); out.write(bytes.data(), std::streamsize(bytes.size())); }   // the worker tests read this file
    }
    auto worker = make_io();
    auto wait_result = [&] {
        std::optional<Result> result;
        for (unsigned i = 0; i < 1000 && !result; ++i) {
            result = worker->poll();
            if (!result) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        expect(result.has_value(), "worker completes without a blocking game-thread wait");
        return *result;
    };
    fs::create_directory(data / "frames");
    fs::copy_file(png, data / "frames/000000.png");
    Job check; check.kind = JobKind::Frames; check.folder = data; check.frames = 1;
    worker->launch(check); expect(wait_result().ok, "worker verifies frames");
    check.frames = 2; worker->launch(check); worker->cancel();
    expect(!wait_result().ok && !worker->busy(), "cancel interrupts missing-frame wait");
    check.kind = JobKind::Audio; check.frames = 0;
    worker->launch(check); expect(wait_result().ok, "worker verifies audio completion");
    check.frames = 60;
    worker->launch(check); expect(!wait_result().ok, "worker rejects short audio");
    fs::remove(png); fs::remove(wav);
    Json commands = Json::object();
    const auto base = fs::absolute(root).parent_path().parent_path().parent_path();
    Request request; request.cine_folder = fs::path("Z:" + base.generic_string()); request.sound = "/Sound";
    const auto folder = fs::path("Z:" + fs::absolute(root.parent_path() / "smoke").generic_string());
    auto narrow = [](const std::vector<std::wstring>& args) {
        std::vector<std::string> result;
        for (const auto& arg : args) result.emplace_back(arg.begin(), arg.end());
        return result;
    };
    for (const auto& encoder : encoder_order(false)) {
        commands[encoder]["probe"] = narrow(probe_arguments(request.cine_folder / "ffmpeg/ffmpeg.exe", encoder, Quality::High));
        commands[encoder]["encode"] = narrow(encode_arguments(request, folder, encoder, 6));
    }
    const auto commands_temp = root.parent_path() / (data.filename().string() + ".json");
    { std::ofstream out(commands_temp); out << commands.dump(2) << '\n'; }
    fs::rename(commands_temp, root.parent_path() / "smoke-commands.json");
#ifndef _WIN32
    auto io = make_io();
    io->launch(Job{});
    std::optional<Result> result;
    for (unsigned i = 0; i < 1000 && !result; ++i) { result = io->poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    expect(result && !result->ok && !io->busy(), "portable process stub fails explicitly");
#endif
}
}
int main(int argc, char** argv) {
    try {
        pure_tests(); state_tests(); bridge_failure_tests();
        if (argc == 2) file_tests(argv[1]);
        else throw std::runtime_error("Pass a test output folder");
        std::cout << "cine render tests: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n'; return 1;
    }
}
