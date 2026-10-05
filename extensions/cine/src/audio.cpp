#include "audio.hpp"
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace cine {
namespace {
std::string lower_extension(const std::filesystem::path& file) {
    auto e = file.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return e;
}
#ifdef _WIN32
constexpr const wchar_t* alias = L"cine_take_audio";
bool mci(const std::wstring& command) { return mciSendStringW(command.c_str(), nullptr, 0, nullptr) == 0; }
#endif
}

bool Audio::play(const std::filesystem::path& file, std::string& why) {
    stop();
    std::error_code missing;
    if (!std::filesystem::is_regular_file(file, missing)) { why = "the file is not there"; return false; }
#ifdef _WIN32
    const std::wstring path = file.wstring();
    if (lower_extension(file) == ".wav") {
        if (!PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) { why = "Windows could not play it"; return false; }
        playing_ = true; through_mci_ = false;
        return true;
    }
    if (path.find(L'"') != std::wstring::npos) { why = "the path has a quote in it"; return false; }
    if (!mci(L"open \"" + path + L"\" type mpegvideo alias " + alias)) { why = "this system cannot open that format; use a .wav file"; return false; }
    if (!mci(std::wstring(L"play ") + alias + L" from 0")) { mci(std::wstring(L"close ") + alias); why = "it opened but would not play; use a .wav file"; return false; }
    playing_ = through_mci_ = true;
    return true;
#else
    // Off the game (host tests): nothing to hear; the file being there is what can be checked.
    playing_ = true; through_mci_ = lower_extension(file) != ".wav";
    return true;
#endif
}

void Audio::stop() noexcept {
    if (!playing_) return;
#ifdef _WIN32
    if (through_mci_) { mci(std::wstring(L"stop ") + alias); mci(std::wstring(L"close ") + alias); }
    else PlaySoundW(nullptr, nullptr, 0);
#endif
    playing_ = false;
}

} // namespace cine
