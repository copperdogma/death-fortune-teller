#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace skull::faultlog {
constexpr size_t line_bytes=192, ring_lines=32, report_bytes=7168;
constexpr unsigned archive_slots=8;
constexpr int64_t retry_us=60000000;
struct Report { uint64_t id=0; int64_t uptime_us=0; uint32_t length=0; char text[report_bytes]{}; };
// Caller serializes capture/state changes. A pending report is immutable while
// the owner performs I/O; further errors are coalesced rather than replacing it.
struct Core {
    char recent[ring_lines][line_bytes]{};
    size_t head=0,count=0;
    uint32_t sequence=0,session=0,coalesced=0;
    char firmware[65]="unknown";
    bool pending=false,frozen=false;
    Report report;
    void capture(const char *line,bool fault,int64_t time) {
        if(!line)return;
        if(frozen){if(fault&&coalesced!=UINT32_MAX)++coalesced;return;}
        const size_t bytes=std::min(std::strlen(line),line_bytes-1);
        std::memcpy(recent[head],line,bytes);recent[head][bytes]=0;
        head=(head+1)%ring_lines;count=std::min(count+1,ring_lines);
        if(!fault)return;
        if(pending){if(coalesced!=UINT32_MAX)++coalesced;return;}
        report.id=(uint64_t(session)<<32)|++sequence;
        report.uptime_us=time;report.length=0;
        pending=frozen=true;coalesced=0;
    }
    // Owner builds text outside the capture lock while history is frozen.
    // Caller must release frozen under its lock after this returns.
    void materialize(){
        if(!frozen)return;
        report.length=static_cast<uint32_t>(std::snprintf(report.text,report_bytes,
            "Death fault report v1 id=%016llx uptime_ms=%lld firmware=%s\n",
            static_cast<unsigned long long>(report.id),static_cast<long long>(report.uptime_us/1000),firmware));
        for(size_t i=0;i<count;++i){
            if(report.length>=report_bytes-2)break;
            const char *entry=recent[(head+ring_lines-count+i)%ring_lines];
            const size_t n=std::min<size_t>(std::strlen(entry),report_bytes-2-report.length);
            std::memcpy(report.text+report.length,entry,n);report.length+=n;
            if(!n||entry[n-1]!='\n')report.text[report.length++]='\n';
        }
        report.text[report.length]=0;
    }
    void restored(const Report &saved){report=saved;pending=true;frozen=false;coalesced=0;}
    void acknowledged(){pending=false;}
};
struct Cadence {
    int64_t next=0;
    uint32_t attempts=0;
    bool due(int64_t now,bool idle,bool pending)const{return idle&&pending&&now>=next;}
    void attempted(int64_t now){next=now+retry_us;if(attempts!=UINT32_MAX)++attempts;}
};
// Only selected application/driver tags are retained; provisioning and raw
// configuration/CHIP/WiFi logs never enter the buffer. Drop ANSI CSI sequences.
inline bool select_line(const char *source,char *out,size_t capacity,bool &fault){
    size_t n=0;
    for(size_t i=0;source[i]&&n+1<capacity;++i){
        if(source[i]=='\033'&&source[i+1]=='['){i+=2;while(source[i]&&(source[i]<'@'||source[i]>'~'))++i;if(!source[i])break;continue;}
        const unsigned char c=source[i];if(c=='\n'||c=='\r'){if(n)break;continue;}if(c>=32)out[n++]=source[i];
    }
    out[n]=0;
    if(std::strstr(out,"Card config ")||std::strstr(out,"password=")||std::strstr(out,"passphrase=")||
       std::strstr(out,"setup_code=")||std::strstr(out,"ssid="))return false;
    const char *tags[]={") death:",") skull_sd:",") skull_radio:",") skull_startup:",") skull_actuators:",") sdmmc_cmd:",") sdmmc_req:",") sdspi_host:",") spi_master:",") diskio_sdmmc:",") death_printer:",") printer:"};
    bool selected=false;for(const char *tag:tags)if(std::strstr(out,tag)){selected=true;break;}
    fault=n>2&&out[0]=='E'&&out[1]==' ';
    // Show failures also have a warning with an explicit reason. User Stop is
    // normal operation and must not create an incident.
    if(std::strstr(out,") death: Interrupted")&&!std::strstr(out,"reason=Dead Stop"))fault=true;
    return selected;
}
}
