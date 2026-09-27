#include "widget_pool.hpp"
#include <algorithm>
#include <cmath>

namespace ccs {
using namespace engine;
namespace {
bool same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
struct NewWidget {
    UObject* object;
    bool rooted;
    bool committed{};
    explicit NewWidget(UObject* value) : object(value), rooted(value && !value->IsRootSet()) {
        if (!object) throw std::runtime_error("Pool widget construction failed");
        if (rooted) object->SetRootSet();
    }
    ~NewWidget() {
        if (!committed) { try { invoke(object, L"RemoveFromParent"); } catch (...) {} }
        if (rooted) object->ClearRootSet();
    }
};
UObject* attach(UObject* parent, UObject* widget) {
    Call add(parent, L"AddChildToCanvas", 2); add.set(L"content", widget); add.run();
    auto* slot = add.get<UObject*>();
    if (!slot) throw std::runtime_error("Canvas rejected pooled widget");
    return slot;
}
}

void WidgetPool::reset() {
    host_.Reset(); root_.Reset(); root_slot_.Reset(); tree_.Reset(); font_source_.Reset();
    cells_.clear(); current_ = nullptr; shown_ = -1; extent_ = {}; layout_ = {};
}
void WidgetPool::invalidate() {
    for (auto& cell : cells_) for (auto& item : cell.kinds) {
        item.text.reset(); item.color.reset(); item.size.reset(); item.bounds.reset(); item.enabled.reset(); item.shown = -1;
    }
    shown_ = -1;
}
void WidgetPool::show(bool visible) {
    if (auto* root = root_.Get(); root && shown_ != static_cast<int>(visible)) {
        invoke(root, L"SetVisibility", L"InVisibility", uint8_t{visible ? uint8_t{4} : uint8_t{1}});
        shown_ = static_cast<int>(visible);
    }
}
bool WidgetPool::begin(UObject* host, UObject* tree, UObject* font_source, Vec2 extent, size_t budget) {
    if (!host || !tree || !font_source || !std::isfinite(extent.x) || !std::isfinite(extent.y) || extent.x < 320 || extent.y < 240)
        return false;
    const auto layout = runtime::MenuLayout::fit(extent.x, extent.y);
    if (!layout) return false;
    used_ = 0; budget_ = std::min(budget, creation_budget); current_ = nullptr; deferred_ = false;
    if (host_.Get() != host || tree_.Get() != tree || !root_.Get() || !root_slot_.Get()) {
        if (auto* root = root_.Get()) invoke(root, L"RemoveFromParent");
        reset();
    }
    tree_ = tree; font_source_ = font_source;
    if (!root_.Get()) {
        if (!budget_) return false;
        NewWidget root(construct(L"/Script/UMG.CanvasPanel", tree));
        invoke(root.object, L"SetClipping", L"InClipping", uint8_t{1});
        auto* slot = attach(host, root.object);
        host_ = host; root_ = root.object; root_slot_ = slot;
        root.committed = true; --budget_;
    }
    if (extent_.x != extent.x || extent_.y != extent.y) {
        invoke(root_slot_.Get(), L"SetPosition", L"InPosition", Vec2{});
        invoke(root_slot_.Get(), L"SetSize", L"InSize", extent);
        extent_ = extent;
        layout_ = *layout;
        for (auto& cell : cells_) for (auto& item : cell.kinds) { item.bounds.reset(); item.size.reset(); }
    }
    show(true);
    return true;
}
void WidgetPool::visibility(Item& item, bool visible) {
    if (auto* widget = item.widget.Get(); widget && item.shown != static_cast<int>(visible)) {
        invoke(widget, L"SetVisibility", L"InVisibility", uint8_t{visible ? (item.interactive ? uint8_t{0} : uint8_t{3}) : uint8_t{1}});
        item.shown = static_cast<int>(visible);
    }
}
WidgetPool::Item* WidgetPool::take(Kind kind) {
    if (used_ >= cell_limit) throw std::runtime_error("Page exceeded pooled cell bound");
    if (used_ == cells_.size()) cells_.emplace_back();
    auto& cell = cells_[used_++];
    const auto index = static_cast<size_t>(kind);
    for (size_t i = 0; i < cell.kinds.size(); ++i) if (i != index) visibility(cell.kinds[i], false);
    auto& item = cell.kinds[index];
    current_ = nullptr;
    if (!item.widget.Get() || !item.slot.Get()) {
        item = Item{};
        if (!budget_) { deferred_ = true; return nullptr; }
        static constexpr std::array classes{L"/Script/UMG.TextBlock", L"/Script/UMG.Border", L"/Script/UMG.EditableText", L"/Script/UMG.Button"};
        NewWidget widget(construct(classes[index], tree_.Get()));
        auto* slot = attach(root_.Get(), widget.object);
        item.widget = widget.object; item.slot = slot;
        item.interactive = kind == Kind::Editable || kind == Kind::Button;
        widget.committed = true; --budget_;
    }
    current_ = &item;
    visibility(item, true);
    return current_;
}
UObject* WidgetPool::text(const std::string& value, float size, Color color, UObject* font) {
    auto* item = take(Kind::Text);
    if (!item) return nullptr;
    auto* widget = item->widget.Get();
    const auto scaled_size = size * static_cast<float>(layout_.scale);
    if (!item->size || *item->size != scaled_size || item->font.Get() != font) {
        font_style(widget, font_source_.Get(), L"FontData", scaled_size);
        if (font) font_size(widget, scaled_size, font);
        item->size = scaled_size; item->font = font;
    }
    if (!item->text || *item->text != value) { text_value(widget, value); item->text = value; }
    if (!item->color || !same(*item->color, color)) {
        invoke(widget, L"SetColorAndOpacity", L"InColorAndOpacity", SlateColor{color}); item->color = color;
    }
    return widget;
}
UObject* WidgetPool::border(Color color) {
    auto* item = take(Kind::Border);
    if (!item) return nullptr;
    auto* widget = item->widget.Get();
    if (!item->color || !same(*item->color, color)) { invoke(widget, L"SetBrushColor", L"InBrushColor", color); item->color = color; }
    return widget;
}
UObject* WidgetPool::editable(const std::string& value, float size, bool enabled) {
    auto* item = take(Kind::Editable);
    if (!item) return nullptr;
    auto* widget = item->widget.Get();
    const auto scaled_size = size * static_cast<float>(layout_.scale);
    if (item->size != scaled_size) {
        font_style(widget, font_source_.Get(), L"FontData", scaled_size); item->size = scaled_size;
    }
    if (item->text != value) { text_value(widget, value); item->text = value; }
    if (item->enabled != enabled) { invoke(widget, L"SetIsEnabled", L"bInIsEnabled", enabled); item->enabled = enabled; }
    return widget;
}
UObject* WidgetPool::button(const std::string& value, float size, Color color, bool enabled) {
    auto* item = take(Kind::Button);
    if (!item) return nullptr;
    auto* widget = item->widget.Get();
    if (!item->label.Get()) {
        if (!budget_) { deferred_ = true; return nullptr; }
        NewWidget label(construct(L"/Script/UMG.TextBlock", tree_.Get()));
        Call add(widget, L"AddChild", 2); add.set(L"content", label.object); add.run();
        if (!add.get<UObject*>()) throw std::runtime_error("Save button rejected its label");
        item->label = label.object; label.committed = true; --budget_;
    }
    auto* label = item->label.Get();
    const auto scaled_size = size * static_cast<float>(layout_.scale);
    if (item->size != scaled_size) {
        font_style(label, font_source_.Get(), L"FontData", scaled_size); item->size = scaled_size;
    }
    if (item->text != value) { text_value(label, value); item->text = value; }
    if (!item->color || !same(*item->color, color)) {
        invoke(label, L"SetColorAndOpacity", L"InColorAndOpacity", SlateColor{color}); item->color = color;
    }
    if (item->enabled != enabled) { invoke(widget, L"SetIsEnabled", L"bInIsEnabled", enabled); item->enabled = enabled; }
    return widget;
}
void WidgetPool::place(UObject* widget, double x, double y, double width, double height) {
    if (!widget) return;
    if (!current_ || current_->widget.Get() != widget) throw std::runtime_error("Pooled placement must follow acquisition");
    const std::array<double, 4> bounds{layout_.left + x * layout_.scale, layout_.top + y * layout_.scale,
        width * layout_.scale, height * layout_.scale};
    if (std::any_of(bounds.begin(), bounds.end(), [](double value) { return !std::isfinite(value); }))
        throw std::runtime_error("Invalid pooled widget bounds");
    if (current_->bounds == bounds) return;
    auto* slot = current_->slot.Get();
    invoke(slot, L"SetPosition", L"InPosition", Vec2{bounds[0], bounds[1]});
    if (current_->text && !current_->interactive) {
        invoke(widget, L"SetAutoWrapText", L"InAutoTextWrap", width > 0 && height >= 60);
        invoke(widget, L"SetTextOverflowPolicy", L"InOverflowPolicy", uint8_t{1});
        invoke(widget, L"SetClipping", L"InClipping", uint8_t{1});
    }
    const bool auto_size = width <= 0 || height <= 0;
    invoke(slot, L"SetAutoSize", L"InbAutoSize", auto_size);
    if (!auto_size) invoke(slot, L"SetSize", L"InSize", Vec2{bounds[2], bounds[3]});
    current_->bounds = bounds;
}
bool WidgetPool::finish() {
    for (size_t i = used_; i < cells_.size(); ++i) for (auto& item : cells_[i].kinds) visibility(item, false);
    current_ = nullptr;
    return !deferred_;
}
}
