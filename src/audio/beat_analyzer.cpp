#include "beat_analyzer.hpp"
#include "energy_accumulator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace spatial::audio {
namespace {
bool cancelled(const std::atomic_bool *flag) { return flag && flag->load(std::memory_order_relaxed); }
double median(std::vector<double> values)
{
    const auto middle = values.begin() + values.size()/2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}
void detect(Analysis &a, const std::atomic_bool *cancel)
{
    const auto n = a.envelope.size();
    if (n < 3) return;
    std::vector<double> flux(n), prefix(n+1);
    const double maximum = *std::max_element(a.envelope.begin(),a.envelope.end());
    if (maximum < 1e-5) return;
    for (size_t i=0;i<n;++i) {
        if ((i & 4095)==0 && cancelled(cancel)) { a.cancelled=true; return; }
        flux[i]=std::max(0.0,a.envelope[i]-(i ? a.envelope[i-1] : 0.0));
        prefix[i+1]=prefix[i]+flux[i];
    }
    struct Peak { double time, strength; };
    std::vector<Peak> peaks;
    for (size_t i=0;i<n;++i) {
        if ((i & 4095)==0 && cancelled(cancel)) { a.cancelled=true; return; }
        const size_t first = i>20 ? i-20 : 0, last=std::min(n,i+21);
        const double local=(prefix[last]-prefix[first])/static_cast<double>(last-first);
        if (flux[i] < std::max(maximum*.08,local*2.5)) continue;
        bool peak=true;
        for(size_t j=i>2?i-2:0;j<std::min(n,i+3);++j)
            if (flux[j]>flux[i] || (j<i && flux[j]==flux[i])) { peak=false; break; }
        if(!peak) continue;
        const double time=static_cast<double>(i)*a.envelopeStep;
        if(!peaks.empty() && time-peaks.back().time < .16) {
            if(flux[i]>peaks.back().strength) peaks.back()={time,flux[i]};
        } else peaks.push_back({time,flux[i]});
    }
    if(peaks.size()<4) return;
    std::vector<double> intervals;
    for(size_t i=1;i<peaks.size();++i) {
        const double gap=peaks[i].time-peaks[i-1].time;
        if(gap>=.28 && gap<=1.25) intervals.push_back(gap);
    }
    if(intervals.size()<3) return;
    // The mode of observed inter-onset intervals estimates tempo. It never
    // replaces observed peaks with a constant-tempo grid.
    double best=0; size_t bestCount=0;
    for(int bpm=48;bpm<=210;++bpm) {
        if(cancelled(cancel)) {a.cancelled=true;return;}
        const double period=60.0/bpm; size_t count=0;
        for(double gap:intervals) if(std::abs(gap-period)<period*.06) ++count;
        if(count>bestCount) { bestCount=count; best=period; }
    }
    std::vector<double> matching;
    for(double gap:intervals) if(std::abs(gap-best)<best*.06) matching.push_back(gap);
    if(matching.empty()) return;
    const double period=median(matching);
    size_t continuous=0;
    for(size_t i=1;i<intervals.size();++i) {
        const double ratio=intervals[i]/intervals[i-1];
        if(ratio>=.72 && ratio<=1.39) ++continuous;
    }
    const double fit=static_cast<double>(bestCount)/static_cast<double>(peaks.size()-1);
    const double continuity=static_cast<double>(continuous)/static_cast<double>(std::max<size_t>(1,intervals.size()-1));
    a.confidence=std::min(.98,(.1+.65*fit+.25*continuity)*std::min(1.0,static_cast<double>(peaks.size())/8.0));
    // Weak/non-periodic material remains available as an envelope; it must not
    // silently become a detected-beat grid.
    if(a.confidence<.45) return;
    // Track a coherent path through the observed onsets. A state describes an
    // actual pair of peaks, so its incoming local period remains available when
    // comparing alternatives. Skipping an unrelated onset cannot change the
    // beat coordinate/phase of every later candidate.
    struct State {size_t from,to,parent; double score;};
    constexpr size_t none=std::numeric_limits<size_t>::max();
    std::vector<State> states;
    std::vector<size_t> starts(peaks.size()+1);
    size_t strongest=none;
    const auto appendPassage=[&] {
        if(strongest==none) return;
        const size_t begin=a.beats.size();
        for(size_t state=strongest;state!=none;state=states[state].parent) {
            a.beats.push_back(peaks[states[state].to].time);
            if(states[state].parent==none) a.beats.push_back(peaks[states[state].from].time);
        }
        if(a.beats.size()-begin<4) a.beats.resize(begin);
        else std::reverse(a.beats.begin()+begin,a.beats.end());
        strongest=none;
    };
    for(size_t to=0;to<peaks.size();++to) {
        if((to&1023)==0&&cancelled(cancel)) {a.cancelled=true;return;}
        // A long gap disconnects the onset graph; retain the coherent path in
        // each passage instead of selecting only one winner for the whole file.
        // No edge or synthetic beat is inserted across the unobserved silence.
        if(to&&peaks[to].time-peaks[to-1].time>1.25) appendPassage();
        starts[to]=states.size();
        for(size_t from=to;from>0;) {
            --from;
            const double gap=peaks[to].time-peaks[from].time;
            if(gap>1.25) break;
            if(gap<.28) continue;
            const double tempoPrior=std::abs(std::log(gap/period));
            State state{from,to,none,2.0-.35*tempoPrior};
            // A sustained new local interval supports a real tempo change.
            // One isolated split interval does not receive that allowance.
            bool confirmedChange=to+2<peaks.size();
            for(size_t next=to+1;confirmedChange&&next<=to+2;++next) {
                const double nextGap=peaks[next].time-peaks[next-1].time;
                if(std::abs(nextGap-gap)>gap*.12) confirmedChange=false;
            }
            for(size_t previous=starts[from];previous<starts[from+1];++previous) {
                const auto &incoming=states[previous];
                const double oldGap=peaks[incoming.to].time-peaks[incoming.from].time;
                double change=4.0*std::abs(std::log(gap/oldGap));
                if(confirmedChange) change=std::min(change,.45);
                const double score=incoming.score+1.0-change-.1*tempoPrior-.03*static_cast<double>(to-from-1);
                if(score>state.score) {state.score=score;state.parent=previous;}
            }
            states.push_back(state);
            const size_t current=states.size()-1;
            if(strongest==none||states[current].score>states[strongest].score) strongest=current;
        }
        starts[to+1]=states.size();
    }
    appendPassage();
    if(a.beats.size()<4) {a.beats.clear();a.confidence=0;return;}
    a.bpm=60.0/period;
}
}
namespace detail {
EnergyAccumulator::EnergyAccumulator(unsigned rate,unsigned channels)
{
    result_.sampleRate=rate; result_.channels=channels;
    if(rate<1000 || rate>768000 || channels==0 || channels>64) {
        result_.error="Unsupported PCM sample rate or channel count"; return;
    }
    hop_=std::max(1u,rate/200); result_.envelopeStep=static_cast<double>(hop_)/rate;
}
void EnergyAccumulator::flush()
{
    if(!inHop_) return;
    result_.envelope.push_back(std::sqrt(squareSum_/(static_cast<double>(inHop_)*result_.channels)));
    inHop_=0; squareSum_=0;
}
bool EnergyAccumulator::append(const float *samples,size_t frames)
{
    if(!result_.error.empty()) return false;
    if(frames>static_cast<std::uint64_t>(maxAnalysisSeconds*result_.sampleRate)-result_.framesDecoded) {
        result_.error="Audio exceeds the 6 hour analysis capacity; select a shorter source"; return false;
    }
    for(size_t f=0;f<frames;++f) {
        for(unsigned ch=0;ch<result_.channels;++ch) {
            const double sample=*samples++;
            if(!std::isfinite(sample)) {result_.error="Nonfinite PCM sample"; return false;}
            squareSum_+=sample*sample;
        }
        ++inHop_; ++result_.framesDecoded;
        if(inHop_==hop_) flush();
    }
    return true;
}
bool EnergyAccumulator::silence(std::uint64_t frames)
{
    if(!result_.error.empty()) return false;
    if(frames>static_cast<std::uint64_t>(maxAnalysisSeconds*result_.sampleRate)-result_.framesDecoded) {
        result_.error="Audio timestamp exceeds the 6 hour analysis capacity"; return false;
    }
    while(frames) {
        const unsigned chunk=static_cast<unsigned>(std::min<std::uint64_t>(frames,hop_-inHop_));
        inHop_+=chunk; result_.framesDecoded+=chunk; frames-=chunk;
        if(inHop_==hop_) flush();
    }
    return true;
}
Analysis EnergyAccumulator::finish(const std::atomic_bool *cancel)
{
    flush();
    if(result_.sampleRate) result_.duration=static_cast<double>(result_.framesDecoded)/result_.sampleRate;
    if(cancelled(cancel)) result_.cancelled=true;
    if(result_.error.empty()&&!result_.cancelled) detect(result_,cancel);
    if(result_.cancelled) { result_.beats.clear(); result_.bpm=0; result_.confidence=0; }
    return std::move(result_);
}
}
Analysis analyzePcm(const std::vector<float> &samples,unsigned rate,unsigned channels)
{
    detail::EnergyAccumulator accumulator(rate,channels);
    if(channels && samples.size()%channels) { Analysis a; a.error="Incomplete interleaved PCM frame"; return a; }
    if(channels) accumulator.append(samples.data(),samples.size()/channels);
    return accumulator.finish();
}
std::vector<double> candidates(const std::vector<double> &beats,double ratio,double minSeconds,double offset)
{
    if(beats.empty()||!std::isfinite(ratio)||ratio<=0||!std::isfinite(minSeconds)||minSeconds<0||!std::isfinite(offset)) return {};
    for(size_t i=0;i<beats.size();++i) if(!std::isfinite(beats[i])||beats[i]<0||(i&&beats[i]<=beats[i-1])) return {};
    const double last=static_cast<double>(beats.size()-1);
    constexpr size_t maxCandidates=1000000;
    const double count=std::floor(last/ratio+1e-9)+1;
    if(!std::isfinite(count)||count>maxCandidates) return {};
    std::vector<double> result; result.reserve(static_cast<size_t>(count));
    double lastSelected=0; bool selected=false;
    for(size_t i=0;i<static_cast<size_t>(count);++i) {
        const double coordinate=std::min(last,static_cast<double>(i)*ratio);
        const size_t index=static_cast<size_t>(coordinate);
        const double fraction=coordinate-static_cast<double>(index);
        const double sourceTime=beats[index]+(index+1<beats.size()?fraction*(beats[index+1]-beats[index]):0);
        if(selected&&sourceTime-lastSelected+1e-10<minSeconds) continue;
        lastSelected=sourceTime; selected=true;
        const double time=sourceTime+offset;
        if(std::isfinite(time)&&time>=0) result.push_back(time);
    }
    return result;
}
} // namespace spatial::audio
