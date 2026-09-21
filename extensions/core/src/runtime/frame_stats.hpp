#pragma once
// Bounded frame-interval statistics. Portable; the loader feeds it QPC ticks
// on Windows and the tests feed synthetic values. Cost per frame: one store.
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
#include <nlohmann/json.hpp>

namespace cssx {
struct FrameSummary {
    uint64_t count=0;
    double mean_ms=0, median_ms=0, p95_ms=0, p99_ms=0, max_ms=0, min_ms=0;
    double hz=0;                 // count / sum
    uint64_t hitches=0;          // intervals above hitch_ratio * median
    double hitch_ratio=2.0;
    nlohmann::json json() const {
        return {{"count",count},{"mean_ms",mean_ms},{"median_ms",median_ms},{"p95_ms",p95_ms},{"p99_ms",p99_ms},
                {"max_ms",max_ms},{"min_ms",min_ms},{"hz",hz},{"hitches",hitches},{"hitch_ratio",hitch_ratio}};
    }
};

// Summarise the newest `take` intervals (ticks) of a ring. `values` is a
// snapshot copy; this function sorts a private copy.
inline FrameSummary summarize(std::vector<int64_t> values,int64_t frequency,double hitch_ratio=2.0) {
    FrameSummary s;s.hitch_ratio=hitch_ratio;
    std::erase_if(values,[](int64_t v){return v<=0;});
    if(values.empty() || frequency<=0) return s;
    std::sort(values.begin(),values.end());
    const double to_ms=1000.0/double(frequency);
    auto at=[&](double q){size_t i=size_t(q*double(values.size()-1)+0.5);return double(values[std::min(i,values.size()-1)])*to_ms;};
    long double sum=0;for(auto v:values) sum+=(long double)v;
    s.count=values.size();s.mean_ms=double(sum)*to_ms/double(values.size());
    s.median_ms=at(0.5);s.p95_ms=at(0.95);s.p99_ms=at(0.99);s.max_ms=double(values.back())*to_ms;s.min_ms=double(values.front())*to_ms;
    s.hz=s.mean_ms>0?1000.0/s.mean_ms:0;
    const double limit=s.median_ms*hitch_ratio;
    for(auto v:values) if(double(v)*to_ms>limit) ++s.hitches;
    return s;
}

// Fixed-capacity ring. The loader owns one for engine tick intervals; the core
// owns one per phase for the time spent inside that phase.
template<size_t N> class FrameRing {
    std::array<int64_t,N> values_{};
    volatile uint32_t head_=0;
    volatile uint64_t total_=0;
public:
    static constexpr uint32_t capacity=uint32_t(N);
    void push(int64_t ticks) noexcept { values_[head_]=ticks; head_=(head_+1)%capacity; total_=total_+1; }
    uint64_t total() const noexcept { return total_; }
    uint32_t head() const noexcept { return head_; }
    const int64_t* data() const noexcept { return values_.data(); }
    // Newest `count` entries, oldest first. Bounded by what has been written.
    std::vector<int64_t> newest(size_t count) const {
        const uint64_t have=std::min<uint64_t>(uint64_t(total_),uint64_t(capacity));count=std::min<size_t>(count,size_t(have));
        std::vector<int64_t> out;out.reserve(count);
        uint32_t start=(head_+capacity-uint32_t(count))%capacity;
        for(size_t i=0;i<count;++i) out.push_back(values_[(start+i)%capacity]);
        return out;
    }
};

// Snapshot the newest entries of a raw ring view (loader-owned, C layout).
inline std::vector<int64_t> newest_of(const int64_t* data,uint32_t capacity,uint32_t head,uint64_t total,size_t count) {
    const uint64_t have=std::min<uint64_t>(total,uint64_t(capacity));count=std::min<size_t>(count,size_t(have));
    std::vector<int64_t> out;out.reserve(count);
    const uint32_t start=(head+capacity-uint32_t(count))%capacity;
    for(size_t i=0;i<count;++i) out.push_back(data[(start+i)%capacity]);
    return out;
}
}
