#pragma once
// A sound file played with the take, for takes cut to music. The DLL plays it through
// Windows' own multimedia API in the game's process, so whatever records the game's sound
// records it too. WAV always works; other formats go through the system's media player
// interface, which a Proton prefix may not have (then play() says so). Game thread only.
#include <filesystem>
#include <string>

namespace cine {

class Audio {
public:
    ~Audio() { stop(); }
    // Starts `file` from its beginning. False, with the reason, when it cannot.
    bool play(const std::filesystem::path& file, std::string& why);
    void stop() noexcept;
    bool playing() const { return playing_; }
private:
    bool playing_ = false, through_mci_ = false;
};

} // namespace cine
