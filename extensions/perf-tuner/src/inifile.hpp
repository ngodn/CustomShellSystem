#pragma once
// The player's own Engine.ini (Saved/Config/Windows/Engine.ini): where tweak packs from the
// web land, and where the game itself writes the video settings it keeps as console
// variables. The tuner reads it, never writes it. Pure parsing here, classification needs
// the live game and lives in the tuner.
#include <string>
#include <vector>

namespace tuner {
struct IniLine {
    std::string section, name, value;
    int number = 0;            // 1-based line in the file
    bool setting = false;      // a config property (bSmoothFrameRate=...), not a console variable
};
struct IniScan {
    std::vector<IniLine> lines;
    std::string pack;          // a known tweak pack recognised from its header, or empty
    bool written_by_game = false;   // the engine's own ";METADATA=" header: the game rewrites this file
    int total_lines = 0;
};
// Every name=value line in the sections the engine reads console variables and engine
// settings from. Comments (;) and blank lines are skipped.
IniScan scan_engine_ini(const std::string& text);

// Console variables the game's own Video settings write to the player's Engine.ini.
bool game_owned_cvar(const std::string& name);
// Console variables that change the picture when set: a tweak pack line with one of these
// is a look change, not a free win. (Beyond the catalog's own forbidden list.)
bool picture_cvar(const std::string& name);
}
