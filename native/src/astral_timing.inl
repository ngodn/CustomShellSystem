// Developer-only creation timings, all accessed on the CSS game-thread tick.
namespace {
#ifdef CSS_INVENTORY_DEV
enum class AstralPhase { source, adapters, materials, visual, registration, release, count };
struct AstralPhaseStats { uint64_t calls=0; double total_ms=0, max_ms=0, last_ms=0; };
std::array<AstralPhaseStats,size_t(AstralPhase::count)> astral_phase_stats{};
struct AstralTiming {
    AstralPhase phase;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    explicit AstralTiming(AstralPhase value):phase(value) {}
    ~AstralTiming() {
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        auto& row=astral_phase_stats[size_t(phase)];
        ++row.calls;row.total_ms+=ms;row.max_ms=std::max(row.max_ms,ms);row.last_ms=ms;
    }
};
Json astral_timing_report() {
    constexpr std::array names{"source", "adapters", "materials", "visual", "registration", "release"};
    Json result=Json::object();
    for(size_t i=0;i<names.size();++i) {
        const auto& row=astral_phase_stats[i];
        result[names[i]]={{"calls",row.calls},{"total_ms",row.total_ms},
            {"max_ms",row.max_ms},{"last_ms",row.last_ms}};
    }
    return result;
}
#endif
}
