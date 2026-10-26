#include "death_show.h"
#include "death_printer.h"
#include "death_fortune.h"
#include "skull_audio.h"
#include "skull_catalog.h"
#include "skull_radio.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
#include <thread>
#include <atomic>
namespace mock {
int64_t time=1000000;
bool speaker=true,audio_busy=false,ready=false,complete=false,error=false,printing=false;
unsigned prints=0,cancels=0,prepares=0,printer_cancels=0,fortune_loads=0,builtin_loads=0;
bool sd_fortunes=true;
std::string fortune_paths[2];
bool cleanup=false,prepare_ok=true,print_ok=true,cancel_in_quiescent=false,cancel_in_error=false;
const skull::catalog::Asset *prepared=nullptr;
skull::catalog::Catalog assets;
void tick(int64_t advance=0){time+=advance;skull::death::tick(true,false);}
void is(const char *name){assert(std::strcmp(skull::death::state_name(),name)==0);}
void setup(bool content=true,bool sd_templates=true){
    audio_busy=ready=complete=error=printing=false;speaker=true;prints=cancels=prepares=printer_cancels=0;
    fortune_loads=builtin_loads=0;sd_fortunes=sd_templates;fortune_paths[0].clear();fortune_paths[1].clear();
    prepared=nullptr;assets.count=0;cleanup=cancel_in_quiescent=cancel_in_error=false;prepare_ok=print_ok=true;
    const char *dirs[]={"welcome","fortune_preamble","fortune_told"};
    if(content)for(auto *dir:dirs)std::snprintf(assets.assets[assets.count++].path,128,"/audio/%s/test.wav",dir);
    skull::death::Config cfg;cfg.cooldown_ms=100;
    skull::death::init(&assets,cfg);tick();
}
void finish(){assert(audio_busy);audio_busy=false;complete=true;tick();}
void start(){assert(audio_busy);ready=true;tick();}
void reject_start(){assert(!skull::death::trigger());assert(skull::death::mode()==skull::death::Mode::start);}
void welcome(){assert(skull::death::trigger());assert(skull::death::mode()==skull::death::Mode::start);tick();is("welcome");}
void preamble(){welcome();start();finish();is("fortune-preamble");}
void printing_phase(){preamble();start();tick(250000);assert(prints==1);}
void print_wait(){printing_phase();finish();is("print-wait");}
void cooldown(){print_wait();printing=false;tick();is("fortune-done");start();finish();is("cooldown");}
void stop(){
    const unsigned previous_printer_cancels=printer_cancels;
    skull::death::cancel();assert(skull::death::mode()==skull::death::Mode::waiting);
    assert(!skull::death::trigger());tick();is("waiting");
    assert(!skull::death::trigger()); // Cleanup tick cannot accept another Start.
    assert(printer_cancels==previous_printer_cancels+1);tick();assert(skull::death::trigger());
}
}
int64_t esp_timer_get_time(){return mock::time;}
uint32_t esp_random(){return 0;}
namespace skull::audio {
bool prepare(const catalog::Asset *asset){++mock::prepares;if(!mock::prepare_ok)return false;mock::prepared=asset;mock::audio_busy=true;mock::complete=mock::ready=false;return true;}
bool ready(){return mock::ready;}bool start(){return true;}
bool quiescent(){if(mock::cancel_in_quiescent){mock::cancel_in_quiescent=false;skull::death::cancel();}return !mock::audio_busy&&!mock::cleanup;}bool error(){if(mock::cancel_in_error){mock::cancel_in_error=false;skull::death::cancel();}return mock::error;}bool complete(){return mock::complete;}
void cancel(){++mock::cancels;mock::audio_busy=false;}
}
namespace skull::radio {bool stream_ready(){return mock::speaker;}}
namespace skull::fortune {
bool load(const char *path){if(mock::fortune_loads<2)mock::fortune_paths[mock::fortune_loads]=path?path:"";++mock::fortune_loads;return mock::sd_fortunes;}
bool load_builtin(){++mock::builtin_loads;return true;}
std::string generate(){return "A fortune";}
}
namespace skull::printer {
bool enqueue(const char *s){assert(!std::strcmp(s,"A fortune"));++mock::prints;mock::printing=mock::print_ok;return mock::print_ok;}
Status status(){Status s{};s.busy=mock::printing;return s;}void cancel(){++mock::printer_cancels;mock::printing=false;}
}
int main(){
    // Both SD paths precede the verified embedded fallback; no fallback when SD works.
    mock::setup();assert(mock::fortune_loads==1&&mock::builtin_loads==0);
    mock::setup(true,false);assert(mock::fortune_loads==2&&mock::builtin_loads==1);
    assert(mock::fortune_paths[0]=="/sdcard/printer/fortunes_littlekid.json");
    assert(mock::fortune_paths[1]=="/sdcard/fortunes/little_kid_fortunes.json");
    // One Start runs the entire sequence without Near/touch/timeout gates.
    mock::setup();mock::is("waiting");assert(skull::death::mode()==skull::death::Mode::waiting);
    mock::welcome();mock::reject_start();mock::start();mock::reject_start();mock::finish();mock::is("fortune-preamble");
    mock::reject_start();mock::start();mock::tick(250000);assert(mock::prints==1);mock::reject_start();
    mock::tick(1000000);assert(mock::prints==1);mock::finish();mock::is("print-wait");mock::reject_start();
    mock::tick();mock::is("print-wait");mock::printing=false;mock::tick();mock::is("fortune-done");mock::reject_start();
    mock::start();mock::finish();mock::is("cooldown");mock::reject_start();
    mock::tick(99999);mock::reject_start();mock::tick(1);mock::is("waiting");
    assert(skull::death::mode()==skull::death::Mode::waiting);const unsigned completed=mock::prepares;
    mock::tick(1000000);assert(mock::prepares==completed&&mock::prints==1);assert(skull::death::trigger());

    // Stop supersedes an accepted but not yet serviced Start, with no replay.
    mock::setup();assert(skull::death::trigger());skull::death::cancel();
    assert(skull::death::mode()==skull::death::Mode::waiting);mock::tick();mock::is("waiting");
    assert(mock::prepares==0&&mock::prints==0);assert(!skull::death::trigger());
    mock::tick(1000000);assert(mock::prepares==0);assert(skull::death::trigger());

    // Stop in every active phase returns directly to Waiting, without cooldown.
    mock::setup();mock::welcome();mock::stop();
    mock::setup();mock::welcome();mock::start();mock::stop();
    mock::setup();mock::preamble();mock::stop();assert(mock::prints==0);
    mock::setup();mock::printing_phase();mock::stop();assert(mock::prints==1);
    mock::setup();mock::print_wait();mock::stop();
    mock::setup();mock::print_wait();mock::printing=false;mock::tick();mock::is("fortune-done");mock::stop();
    mock::setup();mock::cooldown();mock::stop();

    // The Home mode is Waiting immediately even if callback/file cleanup waits.
    mock::setup();mock::welcome();mock::cleanup=true;skull::death::cancel();
    assert(skull::death::mode()==skull::death::Mode::waiting);mock::tick();mock::is("cancelling");
    mock::tick(1000000);assert(!skull::death::trigger());assert(skull::death::jaw_override()==0);
    mock::cleanup=false;mock::tick();mock::is("waiting");assert(!skull::death::trigger());
    mock::tick();assert(skull::death::trigger());

    // An arriving Stop cannot be overwritten by tick publishing its ready gate.
    mock::setup();mock::cancel_in_quiescent=true;mock::tick();assert(!skull::death::trigger());
    assert(skull::death::mode()==skull::death::Mode::waiting);mock::tick();assert(mock::prepares==0);
    // Stop while already Waiting is harmless; manual playback remains available.
    mock::setup();mock::stop();
    mock::setup();assert(skull::death::local("/audio/welcome/test.wav"));mock::is("local");
    mock::reject_start();mock::start();mock::finish();mock::is("cooldown");assert(mock::prints==0);mock::stop();

    // Automatic errors retain Start through cleanup and the full cooldown.
    // Link loss does not print or replay the discarded fortune.
    mock::setup();mock::preamble();mock::speaker=false;mock::tick();mock::is("cancelling");
    assert(mock::prints==0);mock::reject_start();mock::tick();mock::is("cooldown");mock::reject_start();
    mock::tick(100000);mock::is("waiting");assert(!skull::death::trigger());
    mock::speaker=true;mock::tick();assert(skull::death::trigger());
    // Preparation timeout also waits a full cooldown after cleanup completes.
    mock::setup();mock::welcome();mock::tick(5000000);mock::is("cancelling");mock::reject_start();
    mock::tick();mock::is("cooldown");mock::tick(99999);mock::reject_start();
    mock::tick(1);mock::is("waiting");assert(skull::death::trigger());
    // Failed next-clip preparation and active SD errors use the same cooldown.
    mock::setup();mock::welcome();mock::start();mock::prepare_ok=false;mock::finish();mock::is("cancelling");
    assert(mock::prints==0);mock::reject_start();mock::tick();mock::is("cooldown");mock::reject_start();
    mock::setup();mock::welcome();mock::error=true;mock::tick();mock::is("cancelling");mock::reject_start();
    mock::tick();mock::is("cooldown");mock::reject_start();
    // A printer timeout cannot reopen Start early either.
    mock::setup();mock::print_wait();mock::tick(60000000);mock::is("cancelling");mock::reject_start();
    mock::tick();mock::is("cooldown");mock::reject_start();assert(mock::prints==1);
    // Explicit Stop still preempts error cleanup, even when SD ownership waits.
    mock::setup();mock::welcome();mock::cleanup=true;mock::error=true;mock::tick();mock::is("cancelling");
    mock::reject_start();skull::death::cancel();assert(skull::death::mode()==skull::death::Mode::waiting);
    mock::tick();mock::is("cancelling");assert(!skull::death::trigger());
    mock::cleanup=false;mock::tick();mock::is("waiting");assert(!skull::death::trigger());
    mock::tick();assert(skull::death::trigger());
    // A Stop arriving during error detection must not be overwritten by abort.
    mock::setup();mock::welcome();mock::error=mock::cancel_in_error=true;mock::tick();
    mock::is("cancelling");assert(skull::death::mode()==skull::death::Mode::waiting);
    mock::tick();mock::is("waiting");assert(!skull::death::trigger());
    // Explicit Stop during error cooldown returns Waiting without its delay.
    mock::setup();mock::welcome();mock::error=true;mock::tick();mock::tick();mock::is("cooldown");mock::stop();

    // Missing clips skip bounded stages while still printing only one fortune.
    mock::setup(false);assert(skull::death::trigger());mock::tick();mock::is("fortune-preamble");assert(mock::prints==1);
    mock::tick();mock::is("print-wait");mock::printing=false;mock::tick();mock::tick();mock::is("cooldown");
    // Rejected printer enqueue is not automatically duplicated.
    mock::setup(false);mock::print_ok=false;assert(skull::death::trigger());mock::tick();
    mock::tick();mock::tick();mock::tick();assert(mock::prints==1);
    // Existing cards have goodbye rather than fortune_told; use that fallback.
    mock::setup();std::strcpy(mock::assets.assets[2].path,"/audio/goodbye/test.wav");mock::print_wait();
    mock::printing=false;mock::tick();assert(std::strstr(mock::prepared->path,"/goodbye/"));

    // Startup/readiness gates drop rather than replay commands.
    mock::setup();skull::death::tick(false,true);assert(!skull::death::trigger());mock::tick();assert(skull::death::trigger());assert(!skull::death::trigger());
    // One atomic slot allows only one concurrent Start, and Stop wins its slot.
    for(unsigned trial=0;trial<50;++trial){
        mock::setup();std::atomic<unsigned> accepted{0};
        std::thread a([&]{if(skull::death::trigger())++accepted;});
        std::thread b([&]{if(skull::death::trigger())++accepted;});
        a.join();b.join();assert(accepted==1);skull::death::cancel();mock::tick();assert(mock::prepares==0);
        mock::setup();
        std::thread c([]{skull::death::trigger();});std::thread d([]{skull::death::cancel();});
        c.join();d.join();mock::tick();assert(mock::prepares==0&&mock::prints==0);
    }
    std::puts("Death automatic Start/Waiting/Dead Stop scenarios passed");
}
