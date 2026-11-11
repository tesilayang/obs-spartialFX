#include "audio/beat_analyzer.hpp"
#include "audio/energy_accumulator.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace spatial::audio;
namespace {
int checks = 0, failed = 0;
void require(bool ok, const char *name) { ++checks; if (!ok) { ++failed; std::cerr << "FAIL " << name << '\n'; } }
bool near(double a, double b, double eps = .03) { return std::abs(a-b) <= eps; }
std::vector<float> clicks(const std::vector<double> &times, unsigned channels = 1, bool antiphase = false)
{
    constexpr unsigned rate = 48000;
    std::vector<float> pcm(static_cast<size_t>((times.back()+.8)*rate)*channels);
    for (double t : times) for (unsigned i = 0; i < 960; ++i) {
        const float v = static_cast<float>(.9 * std::exp(-static_cast<double>(i)/180) * std::sin(i*2*3.141592653589793*800/rate));
        for (unsigned ch = 0; ch < channels; ++ch) pcm[(static_cast<size_t>(t*rate)+i)*channels+ch] = antiphase && ch%2 ? -v : v;
    }
    return pcm;
}
std::vector<double> grid(double bpm) { std::vector<double> v; for (int i=0;i<24;++i) v.push_back(.3+i*60/bpm); return v; }
bool matches(const std::vector<double> &actual, const std::vector<double> &expected)
{
    return actual.size() == expected.size() && std::equal(actual.begin(),actual.end(),expected.begin(),[](double a,double b){return near(a,b);});
}
}
int main()
{
    for (double bpm : {90.,120.,180.}) {
        const auto times = grid(bpm); const auto a = analyzePcm(clicks(times),48000,1);
        std::cout << "synthetic " << bpm << " bpm=" << a.bpm << " confidence=" << a.confidence << " beats=" << a.beats.size() << '\n';
        require(a.error.empty() && near(a.bpm,bpm,1),"click track tempo within 1 BPM");
        require(matches(a.beats,times),"detected beats retain .3 second lead-in and all real click times");
        require(a.confidence>.7,"stable click track confidence");
    }
    const auto stereo=analyzePcm(clicks(grid(120),2,true),48000,2);
    require(matches(stereo.beats,grid(120)),"antiphase stereo is analyzed per channel without cancellation");
    auto variable=grid(90); variable.resize(12); for(int i=1;i<=12;++i) variable.push_back(variable[11]+i*.5);
    const auto changed=analyzePcm(clicks(variable),48000,1);
    require(matches(changed.beats,variable),"variable tempo retains actual timestamps instead of regular BPM grid");
    auto contaminated=grid(120); contaminated.push_back(2.5); std::sort(contaminated.begin(),contaminated.end());
    const auto tracked=analyzePcm(clicks(contaminated),48000,1);
    require(matches(tracked.beats,grid(120)),"unrelated transient between beats is excluded from tracked sequence");
    require(matches(candidates(tracked.beats,2,0),candidates(grid(120),2,0)),"unrelated transient never shifts two-beat candidate phase");
    auto changedWithTransient=variable; changedWithTransient.push_back(2.5); std::sort(changedWithTransient.begin(),changedWithTransient.end());
    require(matches(analyzePcm(clicks(changedWithTransient),48000,1).beats,variable),"transient filtering preserves observed 90 to 120 tempo transition");
    auto several=grid(120); for(double transient:{.1,2.5,6.05,8.6,12.0}) several.push_back(transient); std::sort(several.begin(),several.end());
    require(matches(analyzePcm(clicks(several),48000,1).beats,grid(120)),"local tracker rejects separated and boundary transients");
    std::vector<double> passages;
    for(int i=0;i<12;++i) passages.push_back(.3+i*.5);
    for(int i=0;i<12;++i) passages.push_back(9.3+i*.5);
    const auto separated=analyzePcm(clicks(passages),48000,1);
    require(matches(separated.beats,passages),"both coherent passages survive a long silent gap without synthetic beats");
    auto isolated=passages; isolated.push_back(6.1); isolated.push_back(7.6); std::sort(isolated.begin(),isolated.end());
    require(matches(analyzePcm(clicks(isolated),48000,1).beats,passages),"isolated onsets between coherent passages are not published as a passage");
    const auto silent=analyzePcm(std::vector<float>(48000*4),48000,1);
    require(silent.beats.empty()&&silent.bpm==0&&silent.confidence==0,"silence has no invented beats or tempo");
    std::vector<float> tone(48000*4); for(size_t i=0;i<tone.size();++i) tone[i]=static_cast<float>(.2*std::sin(i*2*3.141592653589793*440/48000));
    const auto steady=analyzePcm(tone,48000,1);
    require(steady.beats.empty()&&steady.confidence<.3,"steady tone has no stable beat");
    require(!analyzePcm({1,2,3},48000,2).error.empty(),"partial interleaved frame rejected");
    require(!analyzePcm({},0,1).error.empty(),"invalid sample format rejected");
    require(!analyzePcm({std::numeric_limits<float>::quiet_NaN()},48000,1).error.empty(),"nonfinite PCM rejected");
    const std::vector<double> b={.3,.8,1.3,1.8,2.3};
    for (const auto pair : {std::pair<double,double>{.25,.125},{.5,.25},{1,.5},{2,1}}) {
        auto c=candidates(b,pair.first,0); require(c.size()>1&&near(c[1]-c[0],pair.second,1e-9),"120 BPM ratio uses confirmed beat-length units");
    }
    require(matches(candidates({.3,.8,1.4},.5,0),{.3,.55,.8,1.1,1.4}),"subdivisions interpolate local changing beat length");
    require(matches(candidates(b,1.5,0),{.3,1.05,1.8}),"noninteger ratio samples uniform beat coordinates");
    require(matches(candidates(b,.25,.3),{.3,.675,1.05,1.425,1.8,2.175}),"minimum seconds filters density after subdivision");
    require(matches(candidates(b,1,0,.2),{.5,1,1.5,2,2.5}),"timeline offset applies once");
    require(matches(candidates(b,.25,.3,-.5),{.175,.55,.925,1.3,1.675}),"negative offset preserves spacing selection in source beat coordinates");
    require(candidates(b,0,0).empty()&&candidates(b,1e-12,0).empty(),"invalid and unbounded candidate density rejected");
    require(candidates({.5,.4},1,0).empty(),"nonincreasing detected sequence rejected");
    const auto pcm=clicks(grid(120),2,true);
    detail::EnergyAccumulator streaming(48000,2);
    for(size_t frame=0;frame<pcm.size()/2;frame+=137)
        streaming.append(pcm.data()+frame*2,std::min<size_t>(137,pcm.size()/2-frame));
    const auto streamed=streaming.finish();
    require(streamed.envelope==stereo.envelope&&streamed.beats==stereo.beats,"arbitrary decoder chunk boundaries preserve analysis exactly");
    require(near(streamed.duration,static_cast<double>(pcm.size()/2)/48000,1e-12),"partial final energy bin retains exact duration");
    detail::EnergyAccumulator limited(48000,2);
    require(!limited.silence(static_cast<std::uint64_t>((maxAnalysisSeconds+1)*48000))&&!limited.finish().error.empty(),"capacity overflow rejects before allocating the envelope");
    detail::EnergyAccumulator stopped(48000,2); stopped.append(pcm.data(),pcm.size()/2); std::atomic_bool stop{true};
    const auto aborted=stopped.finish(&stop);
    require(aborted.cancelled&&aborted.beats.empty()&&aborted.bpm==0,"analysis cancellation never publishes partial detected beats");
    std::cout << checks << " checks; " << failed << " failures\n";
    return failed ? 1 : 0;
}
