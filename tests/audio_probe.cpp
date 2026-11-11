#include "audio/beat_analyzer.hpp"
#include <atomic>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <windows.h>

namespace {
void jsonString(std::ostream &out,const std::string &s)
{
    out << '"';
    for(unsigned char c:s) { if(c=='"'||c=='\\') out << '\\' << c; else if(c<32) out << '?'; else out << c; }
    out << '"';
}
}
int wmain(int argc,wchar_t **argv)
{
    if(argc<2) { std::cerr << "audio-probe FILE [--expect-error|--cancel|--cancel-progress]\n"; return 64; }
    const std::wstring mode=argc>2?argv[2]:L"";
    std::atomic_bool cancel{mode==L"--cancel"};
    double last=0; bool monotonic=true; size_t calls=0;
    const HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
    auto a=spatial::audio::analyzeFile(argv[1],cancel,[&](double value){
        monotonic=monotonic&&value>=last&&value>=0&&value<=1; last=value; ++calls;
        if(mode==L"--cancel-progress"&&value>.1) cancel=true;
    });
    std::ostringstream out;
    out << std::setprecision(12) << "{\"duration\":" << a.duration << ",\"sampleRate\":" << a.sampleRate
              << ",\"channels\":" << a.channels << ",\"frames\":" << a.framesDecoded
              << ",\"bpm\":" << a.bpm << ",\"confidence\":" << a.confidence
              << ",\"beatCount\":" << a.beats.size() << ",\"firstBeat\":" << (a.beats.empty()?-1:a.beats.front())
              << ",\"envelopeBins\":" << a.envelope.size() << ",\"progressCalls\":" << calls
              << ",\"progressMonotonic\":" << (monotonic?"true":"false")
              << ",\"lastProgress\":" << last << ",\"cancelled\":" << (a.cancelled?"true":"false") << ",\"error\":";
    jsonString(out,a.error); out << "}\n";
    const auto line=out.str(); DWORD written=0;
    if(!WriteFile(output,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)||written!=line.size()) return 7;
    if(!monotonic) return 4;
    if(mode==L"--expect-error") return a.error.empty()?5:0;
    if(mode==L"--cancel"||mode==L"--cancel-progress") return a.cancelled&&a.beats.empty()?0:6;
    return a.error.empty()&&!a.cancelled&&a.framesDecoded&&last==1?0:2;
}
