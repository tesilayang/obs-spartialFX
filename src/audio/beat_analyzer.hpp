#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace spatial::audio {
// The decoder retains a 5ms energy envelope, never the entire decoded PCM.
inline constexpr unsigned algorithmVersion = 3;
inline constexpr double maxAnalysisSeconds = 6.0 * 60.0 * 60.0;
struct Analysis {
    double bpm = 0, confidence = 0, duration = 0;
    std::vector<double> beats, envelope;
    std::string error;
    bool cancelled = false;
    unsigned sampleRate = 0, channels = 0;
    double envelopeStep = 0;
    std::uint64_t framesDecoded = 0;
};
Analysis analyzeFile(const std::wstring &path, const std::atomic_bool &cancel,
                     std::function<void(double)> progress = {});
Analysis analyzePcm(const std::vector<float> &interleaved, unsigned sampleRate, unsigned channels);
// ratio is measured in beat coordinates. Subdivisions are interpolated candidates,
// not new detected onsets. No extrapolation beyond the last observed beat.
std::vector<double> candidates(const std::vector<double> &beats, double ratio,
                               double minSeconds, double offset = 0);
} // namespace spatial::audio
