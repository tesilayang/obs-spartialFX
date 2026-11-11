#include "audio_decoder_win.hpp"
#include "energy_accumulator.hpp"
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace spatial::audio {
namespace {
using Microsoft::WRL::ComPtr;
constexpr DWORD audioStream=static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
constexpr DWORD allStreams=static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);
constexpr DWORD mediaSource=static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE);
struct Platform {
    HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HRESULT media=E_FAIL;
    Platform() { if(SUCCEEDED(apartment)||apartment==RPC_E_CHANGED_MODE) media=MFStartup(MF_VERSION); }
    ~Platform() { if(SUCCEEDED(media)) MFShutdown(); if(SUCCEEDED(apartment)) CoUninitialize(); }
};
std::string failure(const char *operation,HRESULT hr)
{
    std::ostringstream s; s<<operation<<" failed (HRESULT 0x"<<std::hex<<std::uppercase<<static_cast<unsigned long>(hr)<<")"; return s.str();
}
struct LockedBuffer {
    IMFMediaBuffer *buffer; BYTE *data=nullptr; DWORD length=0; HRESULT status;
    explicit LockedBuffer(IMFMediaBuffer *b):buffer(b),status(b->Lock(&data,nullptr,&length)) {}
    ~LockedBuffer() {if(SUCCEEDED(status)) buffer->Unlock();}
};
}
Analysis analyzeFile(const std::wstring &path,const std::atomic_bool &cancel,std::function<void(double)> progress)
{
    Analysis result;
    auto stopping=[&] {return cancel.load(std::memory_order_relaxed);};
    auto report=[&](double value) {if(progress) progress(value);};
    if(stopping()) {result.cancelled=true; return result;}
    try {
        report(0);
        if(stopping()) {result.cancelled=true; return result;}
        if(path.empty() || path.find(L"://")!=std::wstring::npos) {result.error="Offline analysis requires a local media file";return result;}
        Platform platform;
        if(FAILED(platform.media)) {result.error=failure("Media Foundation initialization",platform.media);return result;}
        ComPtr<IMFSourceReader> reader;
        HRESULT hr=MFCreateSourceReaderFromURL(path.c_str(),nullptr,&reader);
        if(FAILED(hr)) {result.error=failure("Open media file",hr);return result;}
        hr=reader->SetStreamSelection(allStreams,FALSE);
        if(FAILED(hr)) {result.error=failure("Disable unused media streams",hr);return result;}
        hr=reader->SetStreamSelection(audioStream,TRUE);
        if(FAILED(hr)) {result.error=failure("Select audio stream (file may have no audio track)",hr);return result;}
        ComPtr<IMFMediaType> requested;
        hr=MFCreateMediaType(&requested);
        if(SUCCEEDED(hr)) hr=requested->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);
        if(SUCCEEDED(hr)) hr=requested->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_Float);
        if(SUCCEEDED(hr)) hr=reader->SetCurrentMediaType(audioStream,nullptr,requested.Get());
        if(FAILED(hr)) {result.error=failure("Configure system decoder for float PCM",hr);return result;}
        ComPtr<IMFMediaType> actual;
        hr=reader->GetCurrentMediaType(audioStream,&actual);
        if(FAILED(hr)) {result.error=failure("Read decoded PCM format",hr);return result;}
        UINT32 rate=0,channels=0,bits=0; GUID subtype{};
        actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,&rate);
        actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,&channels);
        actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,&bits);
        actual->GetGUID(MF_MT_SUBTYPE,&subtype);
        if(subtype!=MFAudioFormat_Float || bits!=32 || !rate || !channels) {result.error="System decoder did not provide interleaved 32 bit float PCM";return result;}
        detail::EnergyAccumulator accumulator(rate,channels);
        if(!accumulator.state().error.empty()) return accumulator.finish();
        PROPVARIANT duration; PropVariantInit(&duration);
        double expectedDuration=0;
        if(SUCCEEDED(reader->GetPresentationAttribute(mediaSource,MF_PD_DURATION,&duration)) && duration.vt==VT_UI8)
            expectedDuration=static_cast<double>(duration.uhVal.QuadPart)/10000000.0;
        PropVariantClear(&duration);
        if(expectedDuration>maxAnalysisSeconds) {result.error="Media exceeds the 6 hour analysis capacity; select a shorter source";return result;}
        double lastProgress=0;
        while(!stopping()) {
            ComPtr<IMFSample> sample; DWORD flags=0; LONGLONG timestamp=0;
            hr=reader->ReadSample(audioStream,0,nullptr,&flags,&timestamp,&sample);
            if(stopping()) break;
            if(FAILED(hr)) {result.error=failure("Decode audio sample",hr);return result;}
            if(flags&MF_SOURCE_READERF_ERROR) {result.error="Media Foundation reported an audio decoding error";return result;}
            if(flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {result.error="Audio format changed during decoding; analyze a constant format source";return result;}
            if(sample) {
                ComPtr<IMFMediaBuffer> buffer;
                hr=sample->ConvertToContiguousBuffer(&buffer);
                if(FAILED(hr)) {result.error=failure("Access decoded PCM buffer",hr);return result;}
                LockedBuffer locked(buffer.Get());
                if(FAILED(locked.status)) {result.error=failure("Lock decoded PCM buffer",locked.status);return result;}
                if(locked.length>64u*1024u*1024u) {result.error="Decoded sample exceeds the 64 MiB chunk capacity";return result;}
                const size_t frameBytes=sizeof(float)*channels;
                if(locked.length%frameBytes) {result.error="Decoder returned an incomplete PCM frame";return result;}
                // Preserve actual media time when a source has a leading gap or
                // a discontinuity. Normal sample rounding does not insert gaps.
                const double decodedTime=static_cast<double>(accumulator.state().framesDecoded)/rate;
                const double sampleTime=static_cast<double>(timestamp)/10000000.0;
                if(sampleTime-decodedTime>.025) {
                    const double gap=(sampleTime-decodedTime)*rate;
                    if(gap>maxAnalysisSeconds*rate) {result.error="Audio timestamp exceeds the 6 hour analysis capacity";return result;}
                    if(!accumulator.silence(static_cast<std::uint64_t>(std::llround(gap)))) return accumulator.finish(&cancel);
                }
                const float *pcm=reinterpret_cast<const float *>(locked.data);
                const size_t frames=locked.length/frameBytes;
                for(size_t start=0;start<frames&&!stopping();start+=8192) {
                    if(!accumulator.append(pcm+start*channels,std::min<size_t>(8192,frames-start))) return accumulator.finish(&cancel);
                }
                if(expectedDuration>0) {
                    const double p=std::min(.94,.94*static_cast<double>(accumulator.state().framesDecoded)/rate/expectedDuration);
                    if(p-lastProgress>=.002) {lastProgress=p;report(p);}
                }
            }
            if(flags&MF_SOURCE_READERF_ENDOFSTREAM) break;
        }
        if(stopping()) return accumulator.finish(&cancel);
        report(.95);
        result=accumulator.finish(&cancel);
        if(result.error.empty()&&!result.cancelled&&!result.framesDecoded) result.error="No analyzable audio PCM was decoded";
        if(result.error.empty()&&!result.cancelled) report(1);
        // reader/sample/buffer are released before Platform shuts MF down.
        return result;
    } catch(const std::exception &e) {result.error=std::string("Audio analysis failed: ")+e.what();return result;}
    catch(...) {result.error="Audio analysis failed with an unexpected exception";return result;}
}
}
