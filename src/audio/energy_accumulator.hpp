#pragma once
#include "beat_analyzer.hpp"

namespace spatial::audio::detail {
// Small streaming state: caller owns and releases each decoded PCM chunk.
class EnergyAccumulator {
public:
    EnergyAccumulator(unsigned sampleRate, unsigned channels);
    bool append(const float *samples, std::size_t frames);
    bool silence(std::uint64_t frames);
    Analysis finish(const std::atomic_bool *cancel = nullptr);
    const Analysis &state() const { return result_; }
private:
    void flush();
    Analysis result_;
    unsigned hop_ = 0, inHop_ = 0;
    double squareSum_ = 0;
};
} // namespace spatial::audio::detail
