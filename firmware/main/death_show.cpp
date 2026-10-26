#include "death_show.h"
#include "death_fortune.h"
#include "death_printer.h"
#include "skull_catalog.h"
#include "skull_audio.h"
#include "skull_radio.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
namespace skull::death {
namespace {
constexpr char tag[]="death";
enum class State {waiting,welcome,preamble,print_wait,done,cooldown,local,cancelling};
enum class Playback {none,preparing,playing};
State state=State::waiting;
Playback playback=Playback::none;
catalog::Catalog *assets=nullptr;
Config config;
// A single atomic command slot makes Stop overwrite queued Start and prevents
// tick's ready publication from accidentally overwriting a concurrent Stop.
constexpr unsigned ready_gate=1, start_request=2, stop_request=4;
std::atomic<unsigned> commands{0};
std::atomic<Mode> published_mode{Mode::waiting};
int64_t entered=0,prepare_deadline=0,print_at=0;
bool print_attempted=false,initialized=false,cancel_to_waiting=false;
std::string fortune;
int64_t now(){return esp_timer_get_time();}
void enter(State next){
    state=next;entered=now();playback=Playback::none;print_at=0;
    published_mode.store(next==State::waiting||(next==State::cancelling&&cancel_to_waiting) ? Mode::waiting : Mode::start);
    ESP_LOGI(tag,"State=%s",state_name());
}
void abort(bool explicit_stop=false){
    // Do not lose an explicit Stop that arrived while detecting an error.
    const unsigned pending=commands.exchange(0);
    cancel_to_waiting=explicit_stop||pending==stop_request;
    audio::cancel();printer::cancel();
    enter(State::cancelling);
}
const catalog::Asset *pick(const char *directory){
    if(!assets)return nullptr;
    size_t matches[catalog::max_assets],count=0;
    const size_t length=std::strlen(directory);
    for(size_t i=0;i<assets->count;++i){const char *path=assets->assets[i].path;
        if(!std::strncmp(path,directory,length)&&path[length]=='/'&&!std::strchr(path+length+1,'/'))matches[count++]=i;
    }
    if(!count)return nullptr;
    // Prefer the least-played category assets; avoid repeated clips when possible.
    uint32_t least=UINT32_MAX;
    for(size_t i=0;i<count;++i)least=std::min(least,assets->assets[matches[i]].play_count);
    size_t candidates[catalog::max_assets],n=0;
    for(size_t i=0;i<count;++i)if(assets->assets[matches[i]].play_count==least)candidates[n++]=matches[i];
    auto &asset=assets->assets[candidates[esp_random()%n]];
    if(asset.play_count!=UINT32_MAX)++asset.play_count;
    return &asset;
}
bool begin(const catalog::Asset *asset){
    if(!asset)return false;
    if(!audio::quiescent()||!audio::prepare(asset)){abort();return false;}
    playback=Playback::preparing;prepare_deadline=now()+5000000;return true;
}
void print_once(){
    if(print_attempted)return;
    print_attempted=true;
    const bool accepted=printer::queue_fortune(fortune.c_str());
    ESP_LOGI(tag,"Fortune print %s; physical paper output requires bench verification",accepted?"queued":"rejected");
}
void stage(State next,const char *dir){enter(next);
    const auto *asset=pick(dir);
    if(!asset&&next==State::done)asset=pick("/audio/goodbye");
    begin(asset);
}
void preamble(){
    fortune=fortune::generate();print_attempted=false;
    stage(State::preamble,"/audio/fortune_preamble");
    if(state==State::preamble&&playback==Playback::none)print_once();
}
void advance(){
    switch(state){
    case State::welcome:preamble();break;
    case State::preamble:print_once();enter(State::print_wait);break;
    case State::done:case State::local:enter(State::cooldown);break;
    default:break;
    }
}

}
void init(catalog::Catalog *content,const Config &settings){
    assets=content;config=settings;
    initialized=false;cancel_to_waiting=false;commands.store(0);enter(State::waiting);
    const char *source=config.fortunes_path;
    bool loaded=fortune::load(source);
    if(!loaded){source="/sdcard/fortunes/little_kid_fortunes.json";loaded=fortune::load(source);}
    if(!loaded){source="embedded verified card template";loaded=fortune::load_builtin();}
    ESP_LOGI(tag,"Fortune templates %s; source=%s",loaded?"loaded":"unavailable",source?source:"(no configured path)");
}
bool trigger(){unsigned expected=ready_gate;return commands.compare_exchange_strong(expected,start_request);}
void cancel(){commands.store(stop_request);}
Mode mode(){
    const unsigned command=commands.load();
    if(command==stop_request)return Mode::waiting;
    if(command==start_request)return Mode::start;
    return published_mode.load();
}
bool local(const char *path){
    if(!initialized||state!=State::waiting||commands.load()!=ready_gate||!radio::stream_ready()||!audio::quiescent()||!assets||!path)return false;
    for(size_t i=0;i<assets->count;++i)if(!std::strcmp(path,assets->assets[i].path)){
        unsigned expected=ready_gate;
        if(!commands.compare_exchange_strong(expected,0))return false;
        enter(State::local);return begin(&assets->assets[i]);
    }
    return false;
}
bool busy(){const unsigned command=commands.load();return state!=State::waiting||command==start_request||command==stop_request;}
int jaw_override(){
    if(state==State::cancelling||state==State::cooldown||state==State::print_wait)return 0;
    return -1;
}
const char *state_name(){
    switch(state){
    case State::waiting:return "waiting";case State::welcome:return "welcome";
    case State::preamble:return "fortune-preamble";
    case State::print_wait:return "print-wait";case State::done:return "fortune-done";case State::cooldown:return "cooldown";
    case State::local:return "local";case State::cancelling:return "cancelling";
    }return "invalid";
}
void tick(bool initialization_finished,bool startup_active){
    const int64_t time=now();
    const unsigned command=commands.exchange(0);
    if(command==stop_request)abort(true);
    if(state==State::cancelling){
        // Explicit Stop returns Home to Waiting immediately. Automatic errors
        // retain Start through cleanup and the normal cooldown, preventing a
        // failed final clip from immediately admitting another full show.
        if(audio::quiescent()&&!printer::busy())enter(cancel_to_waiting?State::waiting:State::cooldown);
        return;
    }
    initialized=initialization_finished&&!startup_active;
    if(!initialized)return;
    if(state!=State::waiting&&state!=State::cooldown&&!radio::stream_ready()){abort();return;}
    if(command==start_request&&state==State::waiting&&radio::stream_ready()&&audio::quiescent())
        stage(State::welcome,"/audio/welcome");
    if(playback!=Playback::none){
        if(audio::error()){abort();return;}
        if(playback==Playback::preparing){
            if(audio::ready()){
                if(!audio::start()){abort();return;}
                playback=Playback::playing;
                if(state==State::preamble)print_at=time+250000;
            }else if(time>=prepare_deadline){abort();return;}
        }
        if(state==State::preamble&&print_at&&time>=print_at)print_once();
        if(playback==Playback::playing&&audio::complete()&&audio::quiescent()){
            playback=Playback::none;advance();
        }
    }else {
        switch(state){
        case State::welcome:case State::preamble:case State::done:advance();break;
        case State::print_wait:
            if(!printer::busy())stage(State::done,"/audio/fortune_told");
            else if(time-entered>=60000000){ESP_LOGW(tag,"Print transport timeout");abort();}
            break;
        case State::cooldown:if(time-entered>=int64_t(config.cooldown_ms)*1000)enter(State::waiting);break;
        default:break;
        }
    }
    if(state==State::waiting&&radio::stream_ready()&&audio::quiescent()&&!printer::busy()){
        unsigned expected=0;
        commands.compare_exchange_strong(expected,ready_gate);
    }
}
}
