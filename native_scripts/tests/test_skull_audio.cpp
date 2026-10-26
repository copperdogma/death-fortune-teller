// Real producer/provider state machine with host queues and filesystem. Build:
// c++ -std=c++17 -pthread -DSKULL_AUDIO_SD_ROOT='"/tmp/skull-audio-test"' \
// -Iscripts/tests/audio_stubs -Ifirmware/main scripts/tests/test_skull_audio.cpp \
// firmware/main/skull_audio.cpp firmware/main/skull_wave.cpp -o /tmp/test-skull-audio
#include "skull_audio.h"
#include "skull_catalog.h"
#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <filesystem>
#include <vector>
#ifndef SKULL_AUDIO_SD_ROOT
#define SKULL_AUDIO_SD_ROOT "/tmp/skull-audio-test"
#endif
namespace skull::catalog {
bool speaking(const Asset &a, char role, uint32_t frame) {
    if (!a.skit) return true;
    for (unsigned i=0;i<a.line_count;++i) if (frame>=a.lines[i].first_frame && frame<a.lines[i].end_frame) return a.lines[i].role==role;
    return false;
}
}
static void wait_until(bool (*f)()) {
    for (unsigned i=0;i<2000 && !f();++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(f());
}
static skull::catalog::Asset asset;
static void fixture(unsigned frames, bool odd_header = false, bool varied = false) {
    std::filesystem::create_directories(SKULL_AUDIO_SD_ROOT);
    FILE *f=std::fopen(SKULL_AUDIO_SD_ROOT "/test.wav","wb");assert(f);
    uint8_t header[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0x44,0xac,0,0,0x10,0xb1,2,0,4,0,16,0,'d','a','t','a'};
    uint32_t data=frames*4,size=data+36+(odd_header?34:0);
    std::memcpy(header+4,&size,4);std::memcpy(header+40,&data,4);
    if (odd_header) {
        std::fwrite(header,1,36,f);
        const uint8_t junk[34]={'J','U','N','K',26};
        std::fwrite(junk,1,sizeof junk,f);
        std::fwrite(header+36,1,8,f);
    } else std::fwrite(header,1,44,f);
    for(unsigned i=0;i<frames*2;++i){int16_t sample=varied?static_cast<int16_t>((i*7919u)%60001u-30000):((i % 2 == 0) ? 1000 : -1000);std::fwrite(&sample,2,1,f);}
    // An authored trailing RIFF chunk must not become delivered PCM.
    if (varied) {const uint8_t tail[16]={'L','I','S','T',8,0,0,0,1,2,3,4,5,6,7,8};std::fwrite(tail,1,sizeof tail,f);size+=sizeof tail;std::fseek(f,4,SEEK_SET);std::fwrite(&size,4,1,f);}
    std::fclose(f);
    std::strcpy(asset.path,"/test.wav");asset.wave={odd_header?78u:44u,data};
}
int main() {
    using namespace skull;
    fixture(40000);asset.skit=true;asset.line_count=2;asset.lines[0]={0,5000,'A'};asset.lines[1]={5000,40000,'B'};
    assert(audio::quiescent());
    audio_test_hold_close.store(true);
    assert(audio::init(50,true)); assert(audio::prepare(&asset)); assert(!audio::start()); wait_until(audio::ready);assert(audio::start());
    assert(!audio::set_gain_percent(40));
    uint32_t position=0;
    while(!audio::complete()) {
        uint8_t output[1024]{};auto before=audio::stats().frames;assert(audio::provide(output,sizeof(output)));
        auto after=audio::stats().frames;
        for(unsigned j=0;j<after-before;++j){int16_t left,right;std::memcpy(&left,output+j*4,2);std::memcpy(&right,output+j*4+2,2);assert(left==((position+j<5000)?500:0));assert(right==((position+j<5000)?-500:0));}
        position+=after-before;std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(position==40000);assert(!audio::busy());
    assert(audio_test_psram_allocations==2); // Ring and lifetime stdio buffer.
    // The last block is consumed while the real worker is held inside fclose.
    // Playback completion alone must not permit an intentional CPU reset.
    wait_until([] { return audio_test_close_entered.load(); });
    assert(!audio::quiescent());
    audio_test_hold_close.store(false);
    wait_until(audio::quiescent);
    assert(audio_test_psram_allocations==2); // Baseline retains both buffers for device lifetime.
    assert(!audio::set_gain_percent(101));assert(audio::set_gain_percent(50));assert(audio::gain_percent()==50);
    const auto timing=audio::stats();assert(timing.blocks==46);assert(audio_test_max_burst.load()==2);
    assert(timing.read_us_total>=timing.read_us_max);
    assert(timing.process_us_total>=timing.process_us_max);
    assert(timing.refill_gap_us_max>0);
    assert(timing.role_frames==5000);assert(timing.input_nonzero_samples==80000);
    assert(timing.output_nonzero_samples==10000);assert(timing.output_peak==500);
    // Hold partial callback block, cancel while callbacks stop, and verify that
    // no reprepare/reset occurs until a provider acknowledges cancellation.
    assert(audio::prepare(&asset));wait_until(audio::ready);assert(audio::start());uint8_t tiny[4]{};assert(audio::provide(tiny,4));audio::cancel();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));assert(audio::stats().cancel_waiting);assert(audio::busy());assert(!audio::quiescent());assert(!audio::prepare(&asset));
    assert(audio::provide(tiny,4));for(unsigned i=0;i<100 && audio::busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(!audio::busy());wait_until(audio::quiescent);
    // Rapid provider drains prebuffer before worker refills: starvation preserves
    // source cursor and emits only the caller's zeros.
    assert(audio::prepare(&asset));wait_until(audio::ready);assert(audio::start());std::vector<uint8_t> large(300000,0);assert(audio::provide(large.data(),large.size()));assert(audio::stats().underruns>0);assert(audio::stats().frames<=8*882);
    audio::cancel();audio::provide(tiny,4);wait_until(audio::quiescent);
    // Data offset78 crosses stdio buffer boundaries; varied stereo samples and
    // a trailing RIFF chunk expose byte shifts, header leakage and overreads.
    fixture(40000,true,true);asset.skit=false;
    assert(audio::set_gain_percent(100));assert(audio::prepare(&asset));wait_until(audio::ready);assert(audio::start());
    position=0;uint32_t expected_hash=2166136261u;
    for(unsigned i=0;i<40000*2;++i){const int16_t sample=static_cast<int16_t>((i*7919u)%60001u-30000);const auto *p=reinterpret_cast<const uint8_t *>(&sample);for(unsigned j=0;j<2;++j)expected_hash=(expected_hash^p[j])*16777619u;}
    while(!audio::complete()) {
        uint8_t output[1024]{};const auto before=audio::stats().frames;assert(audio::provide(output,sizeof output));const auto after=audio::stats().frames;
        for(unsigned j=0;j<(after-before)*2;++j){int16_t sample;std::memcpy(&sample,output+j*2,2);assert(sample==static_cast<int16_t>(((position*2+j)*7919u)%60001u-30000));}
        position+=after-before;std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(position==40000);wait_until(audio::quiescent);assert(audio::stats().raw_pcm_fnv1a==expected_hash);
    // A truncated PCM extent must fail terminally, then permit cancellation
    // and a new request without trapping file ownership.
    std::filesystem::resize_file(SKULL_AUDIO_SD_ROOT "/test.wav", asset.wave.data_offset + asset.wave.data_bytes - 1);
    assert(audio::prepare(&asset));wait_until(audio::error);wait_until(audio::quiescent);
    audio::cancel();audio::provide(tiny,4);wait_until([]{return !audio::error()&&audio::quiescent();});
    // Failed open must clear worker file activity rather than trapping reboot.
    std::strcpy(asset.path,"/missing.wav");assert(audio::prepare(&asset));
    wait_until(audio::error);wait_until(audio::quiescent);
    std::puts("skull_audio producer, role gain/muting, EOF, cancellation hold, starvation, and reboot file quiescence passed");
    std::fflush(stdout);std::_Exit(0); // worker is intentionally device-lifetime
}
