#include "death_fault_log.h"
#include "death_fault_log_core.h"
#include "death_fault_archive.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include <cstdarg>
#include <new>
#include <cerrno>
#include <atomic>
namespace skull::faultlog {
namespace {
constexpr char tag[]="faultlog", ns[]="death_fault";
constexpr uint32_t disk_version=1;
struct Compact {
    uint32_t version=0,slot=0,length=0;
    uint64_t id=0;
    int64_t uptime_us=0;
    char text[1024]{};
};
Core *core=nullptr;
Compact *compact=nullptr;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
std::atomic<vprintf_like_t> previous{std::vprintf};
Cadence sd_cadence,nv_cadence,ack_cadence;
uint64_t working_id=0;
unsigned slot=0;
bool nv_saved=false,sd_saved=false;
esp_err_t nv_error=ESP_OK;
int sd_error=0;
int hook(const char *format,va_list args){
    char rendered[256],line[line_bytes];bool fault=false;
    va_list copy;va_copy(copy,args);std::vsnprintf(rendered,sizeof(rendered),format,copy);va_end(copy);
    if(core&&select_line(rendered,line,sizeof(line),fault)){
        const int64_t time=esp_timer_get_time();
        portENTER_CRITICAL(&mux);core->capture(line,fault,time);portEXIT_CRITICAL(&mux);
    }
    return previous.load()(format,args);
}
void materialize(){
    bool frozen;portENTER_CRITICAL(&mux);frozen=core->frozen;portEXIT_CRITICAL(&mux);
    if(!frozen)return;
    core->materialize();
    portENTER_CRITICAL(&mux);core->frozen=false;portEXIT_CRITICAL(&mux);
}
esp_err_t save_compact(){
    // Reserve room for system/Matter state; never erase NVS to make space.
    nvs_stats_t stats{};esp_err_t result=nvs_get_stats(nullptr,&stats);
    if(result!=ESP_OK)return result;
    if(stats.available_entries<176)return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    *compact=Compact{};compact->version=disk_version;compact->id=core->report.id;
    compact->slot=slot;compact->uptime_us=core->report.uptime_us;
    const auto &r=core->report;
    const int header=std::snprintf(compact->text,sizeof(compact->text),
        "Death compact fault report v1 id=%016llx uptime_ms=%lld firmware=%s\n",
        static_cast<unsigned long long>(r.id),static_cast<long long>(r.uptime_us/1000),core->firmware);
    const size_t tail=std::min(size_t(r.length),sizeof(compact->text)-1-size_t(header));
    std::memcpy(compact->text+header,r.text+r.length-tail,tail);
    compact->length=header+tail;
    nvs_handle_t handle{};result=nvs_open(ns,NVS_READWRITE,&handle);
    if(result!=ESP_OK)return result;
    result=nvs_set_blob(handle,"last",compact,sizeof(*compact));
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result;
}
esp_err_t acknowledge(){
    nvs_handle_t handle{};esp_err_t result=nvs_open(ns,NVS_READWRITE,&handle);
    if(result!=ESP_OK)return result;
    result=nvs_set_u64(handle,"acked",core->report.id);
    if(result==ESP_OK)result=nvs_commit(handle);
    nvs_close(handle);return result;
}
void restore(){
    nvs_handle_t handle{};if(nvs_open(ns,NVS_READONLY,&handle)!=ESP_OK)return;
    size_t size=sizeof(*compact);uint64_t acked=0;
    const esp_err_t result=nvs_get_blob(handle,"last",compact,&size);
    nvs_get_u64(handle,"acked",&acked);nvs_close(handle);
    if(result!=ESP_OK||size!=sizeof(*compact)||compact->version!=disk_version||
       compact->slot>=archive_slots||compact->length>=sizeof(compact->text)||!compact->id)return;
    slot=(compact->slot+1)%archive_slots;
    if(acked==compact->id)return;
    core->report.id=compact->id;core->report.uptime_us=compact->uptime_us;
    core->report.length=compact->length;
    std::memcpy(core->report.text,compact->text,compact->length);
    core->report.text[compact->length]=0;core->pending=true;
    slot=compact->slot;working_id=compact->id;nv_saved=true;
}
}
void init(){
    if(core)return;
    void *memory=heap_caps_malloc(sizeof(Core)+sizeof(Compact),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!memory){ESP_LOGW(tag,"RAM fault logger unavailable; serial logging continues");return;}
    core=new(memory)Core;
    // Core alignment is at least that of Compact (both have uint64_t fields).
    compact=new(static_cast<char*>(memory)+sizeof(Core))Compact;
    core->session=esp_random();
    const auto *app=esp_app_get_description();
    for(size_t i=0;i<32;++i)std::snprintf(core->firmware+i*2,3,"%02x",app->app_elf_sha256[i]);
    restore();previous.store(esp_log_set_vprintf(hook));
    ESP_LOGI(tag,"RAM ring=%u lines, frozen report=%u bytes; SD slots=%u retry=60s; recovered=%d",
             unsigned(ring_lines),unsigned(report_bytes),archive_slots,core->pending);
}
void poll(bool idle,bool card_mounted){
    if(!core)return;
    materialize(); // RAM-only work also runs during cleanup/cooldown.
    if(!idle)return;
    bool pending;portENTER_CRITICAL(&mux);pending=core->pending;portEXIT_CRITICAL(&mux);
    if(!pending)return;
    const int64_t now=esp_timer_get_time();
    // The frozen report cannot change while pending; hooks only update recent
    // history/coalesced count. All persistence is owned by this control task.
    if(working_id!=core->report.id){
        working_id=core->report.id;nv_saved=sd_saved=false;
        // Do not bypass the cadence when a new error follows a saved report.
    }
    if(!nv_saved&&nv_cadence.due(now,true,true)){
        nv_cadence.attempted(now);nv_error=save_compact();nv_saved=nv_error==ESP_OK;
        ESP_LOGI(tag,"Internal fault retention: %s",esp_err_to_name(nv_error));
    }
    if(!sd_saved&&sd_cadence.due(now,true,true)){
        sd_cadence.attempted(now);
        if(card_mounted){errno=0;sd_saved=archive_report("/sdcard/diagnostics",slot,core->report);sd_error=sd_saved?0:errno;}
        else sd_error=ENODEV;
        ESP_LOGI(tag,"SD fault archive %s slot=%u errno=%d; pending retained on failure",
                 sd_saved?"saved":"deferred",slot,sd_error);
    }
    if(!sd_saved)return;
    if(nv_saved){
        if(!ack_cadence.due(now,true,true))return;
        ack_cadence.attempted(now);nv_error=acknowledge();
        if(nv_error!=ESP_OK){ESP_LOGW(tag,"Archive saved; flash acknowledgement deferred: %s",esp_err_to_name(nv_error));return;}
    }
    portENTER_CRITICAL(&mux);core->acknowledged();portEXIT_CRITICAL(&mux);
    slot=(slot+1)%archive_slots;
}
void status(){
    if(!core){ESP_LOGI(tag,"Fault logger disabled (allocation unavailable)");return;}
    bool pending;uint32_t coalesced;uint64_t id;
    portENTER_CRITICAL(&mux);pending=core->pending;coalesced=core->coalesced;id=core->report.id;portEXIT_CRITICAL(&mux);
    ESP_LOGI(tag,"pending=%d id=%016llx coalesced=%lu SD_attempts=%lu NVS_attempts=%lu SD_errno=%d NVS=%s next_SD_ms=%lld",
        pending,static_cast<unsigned long long>(id),static_cast<unsigned long>(coalesced),
        static_cast<unsigned long>(sd_cadence.attempts),static_cast<unsigned long>(nv_cadence.attempts),sd_error,
        esp_err_to_name(nv_error),static_cast<long long>(std::max<int64_t>(0,sd_cadence.next-esp_timer_get_time())/1000));
}
void dump(){
    status();if(!core)return;
    bool pending;portENTER_CRITICAL(&mux);pending=core->pending;portEXIT_CRITICAL(&mux);
    if(!pending){ESP_LOGI(tag,"No pending RAM report; inspect SD diagnostics for saved reports");return;}
    materialize();
    // Pending snapshot is immutable until this owner calls poll again.
    for(size_t i=0;i<report_bytes;){
        char chunk[160];size_t n;
        portENTER_CRITICAL(&mux);
        n=i<core->report.length?std::min(sizeof(chunk)-1,size_t(core->report.length)-i):0;
        if(n)std::memcpy(chunk,core->report.text+i,n);
        portEXIT_CRITICAL(&mux);
        if(!n)break;
        chunk[n]=0;ESP_LOGI(tag,"%s",chunk);i+=n;
    }
}
void test_fault(){ESP_LOGE("death","FAULT_LOG_TEST controlled diagnostic; no SD/audio failure injected");}
}
