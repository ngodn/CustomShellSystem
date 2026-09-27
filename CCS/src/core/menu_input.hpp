#pragma once
#include "engine.hpp"
#include "input_edges.hpp"

namespace ccs {
class MenuInput {
public:
    void reset();
    bool refresh(engine::UObject* controller, engine::UObject* handler);
    std::optional<runtime::MenuAction> poll(uint64_t now, bool typing = false);
    std::string hint(runtime::MenuAction action) const;
    uint64_t revision() const { return revision_; }
    void suppress_held() { edges_.reset(); }
private:
    struct Key {
        engine::FName name;
        std::string label;
        bool operator==(const Key& other) const { return name == other.name; }
    };
    using Routes = std::array<std::vector<size_t>, runtime::menu_action_count>;
    engine::WeakObject controller_, mapping_, subsystem_;
    std::vector<Key> keys_;
    Routes routes_;
    runtime::InputEdges edges_;
    uint64_t revision_{};
    bool typing_{};
};
}
