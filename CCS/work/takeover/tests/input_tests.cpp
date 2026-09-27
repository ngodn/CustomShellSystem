#include "input_edges.hpp"
#include <iostream>
#include <stdexcept>

using namespace ccs::runtime;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
size_t index(MenuAction action) { return static_cast<size_t>(action); }

int main() {
    try {
        check(!typing_key_allowed(MenuAction::Accept, "E", true) &&
            !typing_key_allowed(MenuAction::Down, "S", true) && !typing_key_allowed(MenuAction::Cancel, "Q", true),
            "Filename typing triggered mapped letter actions");
        check(typing_key_allowed(MenuAction::Cancel, "Escape", true) &&
            typing_key_allowed(MenuAction::Accept, "Gamepad_FaceButton_Bottom", true) &&
            typing_key_allowed(MenuAction::Accept, "E", false), "Typing removed safe or ordinary action routes");
        InputEdges input;
        InputEdges::Sample keys{};
        keys[index(MenuAction::Down)] = true;
        check(!input.update(100, keys) && !input.update(1000, keys), "Opening hold generated an action or repeat");
        keys.fill(false); input.update(1001, keys);
        keys[index(MenuAction::Down)] = true;
        check(input.update(1010, keys) == MenuAction::Down, "Fresh direction press was lost");
        check(!input.update(1409, keys), "Direction repeated before its delay");
        check(input.update(1410, keys) == MenuAction::Down, "First direction repeat was lost");
        check(!input.update(1499, keys) && input.update(1500, keys) == MenuAction::Down, "Repeat interval changed");
        check(input.update(10000, keys) == MenuAction::Down && !input.update(10000, keys), "Hitch generated a catch-up action burst");
        keys.fill(false); input.update(10001, keys);
        keys[index(MenuAction::Accept)] = true;
        keys[index(MenuAction::Unequip)] = true;
        check(input.update(10002, keys) == MenuAction::Accept, "Simultaneous action priority changed");
        check(!input.update(11000, keys), "Held secondary key appeared as a delayed fresh press");
        keys[index(MenuAction::Accept)] = false;
        check(!input.update(11001, keys), "Releasing primary retriggered held secondary");
        keys.fill(false); input.update(11002, keys);
        keys[index(MenuAction::Unequip)] = true;
        check(input.update(11003, keys) == MenuAction::Unequip, "Released secondary failed to trigger again");
        check(!input.update(20000, keys), "Destructive action auto-repeated");
        input.reset();
        check(!input.update(20001, keys), "Reopening activated a held action");
        keys.fill(false); input.update(20002, keys);
        keys[index(MenuAction::Cancel)] = keys[index(MenuAction::Accept)] = true;
        check(input.update(20003, keys) == MenuAction::Cancel, "Cancel did not take priority over confirmation");
        check(!input.update(1, keys), "Clock rollback retriggered held confirmation");
        keys.fill(false); input.update(2, keys);
        keys[index(MenuAction::Accept)] = true;
        check(input.update(3, keys) == MenuAction::Accept, "Clock reset never recovered after release");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
