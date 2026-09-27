#pragma once
#include "engine.hpp"
#include "menu_layout.hpp"
#include <array>
#include <optional>

namespace ccs {
class WidgetPool {
public:
    static constexpr size_t cell_limit = 128;
    static constexpr size_t creation_budget = 4;
    void reset();
    void invalidate();
    bool begin(engine::UObject* host, engine::UObject* tree, engine::UObject* font_source, engine::Vec2 extent, size_t budget = creation_budget);
    size_t remaining_budget() const { return budget_; }
    engine::UObject* text(const std::string& text, float size, engine::Color color, engine::UObject* font);
    engine::UObject* border(engine::Color color);
    engine::UObject* editable(const std::string& value, float size, bool enabled = true);
    engine::UObject* button(const std::string& value, float size, engine::Color color, bool enabled = true);
    void place(engine::UObject* widget, double x, double y, double width, double height);
    bool finish();
    void show(bool visible);
private:
    enum class Kind : size_t { Text, Border, Editable, Button };
    struct Item {
        engine::WeakObject widget, slot, font, label;
        std::optional<std::string> text;
        std::optional<engine::Color> color;
        std::optional<float> size;
        std::optional<bool> enabled;
        std::optional<std::array<double, 4>> bounds;
        int shown{-1};
        bool interactive{};
    };
    struct Cell { std::array<Item, 4> kinds; };
    Item* take(Kind kind);
    void visibility(Item& item, bool visible);
    engine::WeakObject host_, root_, root_slot_, tree_, font_source_;
    std::vector<Cell> cells_;
    engine::Vec2 extent_{};
    runtime::MenuLayout layout_{};
    size_t used_{}, budget_{};
    Item* current_{};
    bool deferred_{};
    int shown_{-1};
};
}
