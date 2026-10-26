#include "platform.h"
#include "death_fault_archive.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <thread>
namespace mock {
int64_t now=1000000;
vprintf_like_t hook=nullptr;
std::map<std::string,std::vector<unsigned char>> nvs;
size_t entries=1000;unsigned commits=0,archive_calls=0,sets=0;
bool archive_ok=false,commit_ok=true;
uint64_t archived_id=0;
std::string archived;
int discard(const char *,va_list){return 0;}
}
const char *esp_err_to_name(esp_err_t r){return r==ESP_OK?"OK":"error";}
vprintf_like_t esp_log_set_vprintf(vprintf_like_t f){auto old=mock::hook?mock::hook:mock::discard;mock::hook=f;return old;}
void emit(const char *fmt,...){va_list a;va_start(a,fmt);if(mock::hook)mock::hook(fmt,a);va_end(a);}
void test_log(const char *level,const char *tag,const char *fmt,...){
 char body[512],line[600];va_list a;va_start(a,fmt);std::vsnprintf(body,sizeof(body),fmt,a);va_end(a);
 std::snprintf(line,sizeof(line),"%s (1) %s: %s\n",level,tag,body);emit("%s",line);
}
void *heap_caps_malloc(size_t n,int){return std::malloc(n);}
int64_t esp_timer_get_time(){return mock::now;}
uint32_t esp_random(){static uint32_t session=100;return ++session;}
const esp_app_desc_t *esp_app_get_description(){static esp_app_desc_t app{};return &app;}
esp_err_t nvs_get_stats(const char *,nvs_stats_t *s){s->available_entries=mock::entries;return ESP_OK;}
esp_err_t nvs_open(const char *name,int,nvs_handle_t *h){assert(std::string(name)=="death_fault");*h=1;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t,const char *key,const void *data,size_t n){++mock::sets;auto p=static_cast<const unsigned char*>(data);mock::nvs[key]={p,p+n};return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t,const char *key,void *data,size_t *n){auto it=mock::nvs.find(key);if(it==mock::nvs.end())return ESP_FAIL;if(*n<it->second.size())return ESP_FAIL;*n=it->second.size();std::memcpy(data,it->second.data(),*n);return ESP_OK;}
esp_err_t nvs_set_u64(nvs_handle_t h,const char *k,uint64_t v){return nvs_set_blob(h,k,&v,sizeof(v));}
esp_err_t nvs_get_u64(nvs_handle_t h,const char *k,uint64_t *v){size_t n=sizeof(*v);return nvs_get_blob(h,k,v,&n);}
esp_err_t nvs_commit(nvs_handle_t){++mock::commits;return mock::commit_ok?ESP_OK:ESP_FAIL;}
void nvs_close(nvs_handle_t){}
namespace skull::faultlog {
bool archive_report(const char *path,unsigned slot,const Report &r){
 assert(std::string(path)=="/sdcard/diagnostics"&&slot<archive_slots);
 ++mock::archive_calls;if(mock::archive_ok){mock::archived.assign(r.text,r.length);mock::archived_id=r.id;}return mock::archive_ok;
}
}
// Include production wrapper to simulate a reboot (reset volatile globals only)
// while preserving the mock NVS partition. All runtime behavior is production.
#include "../../firmware/main/death_fault_log.cpp"
void reboot(){
 using namespace skull::faultlog;
 mock::hook=mock::discard;std::free(core);core=nullptr;compact=nullptr;
 previous.store(mock::discard);sd_cadence={};nv_cadence={};ack_cadence={};
 working_id=0;slot=0;nv_saved=sd_saved=false;nv_error=ESP_OK;sd_error=0;
 init();
}
int main(){
 using namespace skull::faultlog;
 mock::nvs["simulated_matter_key"]={1,2,3};const auto matter=mock::nvs["simulated_matter_key"];
 init();test_fault();assert(core->pending&&mock::commits==0&&mock::archive_calls==0);
 const uint64_t id=core->report.id;
 poll(false,true);assert(mock::commits==0); // no I/O during playback
 poll(true,false);assert(nv_saved&&mock::commits==1&&mock::archive_calls==0&&core->pending);
 for(int i=2;i<60;++i){mock::now=int64_t(i)*1000000;poll(true,true);}
 assert(mock::archive_calls==0); // unavailable card still consumes retry interval
 mock::now=61000000;poll(false,true);assert(mock::archive_calls==0);
 poll(true,true);assert(mock::archive_calls==1&&core->pending);
 test_fault();assert(core->report.id==id&&core->coalesced==1);
 // Concurrent log capture stays RAM-only and cannot replace pending report.
 const unsigned commits=mock::commits;
 std::thread a([]{for(int i=0;i<200;++i)test_fault();});
 std::thread b([]{for(int i=0;i<200;++i)test_log("I","death","State=waiting");});
 a.join();b.join();assert(mock::commits==commits&&mock::archive_calls==1&&core->report.id==id);
 // Reboot while card is bad recovers compact report and original identity.
 reboot();assert(core->pending&&core->report.id==id&&nv_saved&&core->report.length<1024);
 assert(std::strstr(core->report.text,"FAULT_LOG_TEST"));
 mock::archive_ok=true;poll(true,true);assert(!core->pending&&mock::archived_id==id);
 reboot();assert(!core->pending); // saved/acknowledged reports are not re-exported
 // NVS exhaustion never erases another namespace or loses pending RAM.
 mock::entries=0;mock::archive_ok=false;mock::now+=61000000;test_fault();
 const unsigned before=mock::sets;poll(true,false);assert(core->pending&&!nv_saved&&mock::sets==before);
 mock::entries=1000;mock::now+=61000000;poll(true,false);assert(nv_saved);
 // Commit/ack failure retains report even after successful SD export.
 mock::archive_ok=true;mock::commit_ok=false;mock::now+=61000000;poll(true,true);
 assert(core->pending&&sd_saved);const unsigned attempts=mock::archive_calls;
 mock::commit_ok=true;mock::now+=61000000;poll(true,true);assert(!core->pending&&mock::archive_calls==attempts);
 assert(mock::nvs["simulated_matter_key"]==matter);
 std::free(core);core=nullptr;mock::hook=mock::discard;
 std::puts("Fault runtime retry/NVS/reboot/concurrency scenarios passed");
}
