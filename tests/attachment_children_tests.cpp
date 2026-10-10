#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

// Exercise the production snapshot against reflected-call fixtures. Real UE
// object registration and gameplay consumers still require a live core check.
struct UClass {};
UClass scene_class;
struct UObject {
    UObject* parent = nullptr;
    bool scene = true;
    bool alive = true;
    bool IsA(UClass* type) const { return type == &scene_class && scene; }
};
struct WeakObject {
    UObject* value;
    explicit WeakObject(UObject* object) : value(object) {}
    UObject* Get() const { return value && value->alive ? value : nullptr; }
};
struct FProperty {
    virtual ~FProperty() = default;
    template<class T> bool IsA() const { return dynamic_cast<const T*>(this); }
    virtual size_t GetElementSize() const { return sizeof(UObject*); }
};
struct FObjectProperty : FProperty {};
struct FArrayProperty : FProperty {
    FObjectProperty object;
    FProperty* GetInner() { return &object; }
};
struct Fixture {
    FArrayProperty property;
    std::vector<UObject*> values;
    int count = 0;
    int population = 0;
    bool null_storage = false;
    bool wrong_property = false;
} fixture;
struct FUObjectArray {
    static int GetNumElements() { return fixture.population; }
};
struct Call {
    Call(UObject*, const wchar_t* name, int params) {
        if(std::wstring_view(name) != L"GetChildrenComponents" || params != 2)
            throw std::runtime_error("Unexpected reflection call");
    }
    void set(const wchar_t* name, bool descendants) {
        if(std::wstring_view(name) != L"bIncludeAllDescendants" || descendants)
            throw std::runtime_error("Attachment query became recursive");
    }
    void run() {}
    FProperty* param(const wchar_t*) {
        return fixture.wrong_property ? static_cast<FProperty*>(&fixture.property.object) : &fixture.property;
    }
    void* data(FProperty*) { return &fixture; }
};
struct FScriptArrayHelper {
    FScriptArrayHelper(FArrayProperty*, void*) {}
    int Num() const { return fixture.count; }
    void* GetRawPtr(int index = 0) {
        return fixture.null_storage ? nullptr : &fixture.values.at(index);
    }
};
static UClass* find(const wchar_t*) { return &scene_class; }
template<class T> T read(UObject* object, const wchar_t*) { return object->parent; }

#include "attachment_children.inl"

static void check(bool value, const char* message) {
    if(!value) throw std::runtime_error(message);
}
template<class Fn> static void rejects(Fn&& fn, const char* message) {
    try { fn(); } catch(const std::runtime_error&) { return; }
    throw std::runtime_error(message);
}
int main() {
    try {
        UObject parent, other;
        for(int count : {0, 64, 128, 129, 257, 4097}) {
            fixture = {};
            std::vector<UObject> objects(count);
            for(auto& object : objects) { object.parent = &parent; fixture.values.push_back(&object); }
            fixture.count = count;
            fixture.population = count + 2;
            auto snapshot = attached_children(&parent);
            check(snapshot.size() == objects.size(), "Valid attachments were truncated or rejected");
            for(int i = 0; i < count; ++i)
                check(snapshot[i].Get() == &objects[i], "Attachment order or identity changed");
            if(!count) continue;
            objects.back().alive = false;
            check(!snapshot.back().Get(), "Removed attachment retained a live handle");
            objects.back().alive = true;
            if(count != 4097) continue;
            objects.back().parent = &other;
            check(attached_children(&parent).size() == objects.size() - 1,
                  "Reparented attachment remained in this parent's snapshot");
        }
        fixture = {};
        fixture.count = -1;
        rejects([&] { attached_children(&parent); }, "Negative attachment count accepted");
        fixture.count = 3; fixture.population = 2;
        rejects([&] { attached_children(&parent); }, "Impossible object count accepted");
        fixture.count = 1; fixture.null_storage = true;
        rejects([&] { attached_children(&parent); }, "Null array storage accepted");
        fixture.null_storage = false; fixture.values = {&other}; other.scene = false;
        rejects([&] { attached_children(&parent); }, "Non-component attachment accepted");
        fixture.wrong_property = true;
        rejects([&] { attached_children(&parent); }, "Invalid reflected layout accepted");
        std::cout << "Attachment snapshots through 4097 children and invalid-data checks passed\n";
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
