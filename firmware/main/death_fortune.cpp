#include "death_fortune.h"
#include "death_fortune_builtin.h"
#include "cJSON.h"
#include "esp_random.h"
#include <cstdio>
#include <map>
#include <vector>
namespace skull::fortune {
namespace {
std::vector<std::string> templates;
std::map<std::string, std::vector<std::string>> words;
bool tokens(const std::string &text) {
    size_t cursor=0;
    while ((cursor=text.find("{{",cursor))!=std::string::npos) {
        const size_t end=text.find("}}",cursor+2);
        if(end==std::string::npos) return false;
        std::string key=text.substr(cursor+2,end-cursor-2);
        const auto first=key.find_first_not_of(" \t");
        const auto last=key.find_last_not_of(" \t");
        if(first==std::string::npos) return false;
        key=key.substr(first,last-first+1);
        if(!words.count(key)||words[key].empty()) return false;
        cursor=end+2;
    }
    return true;
}
bool parse(const char *bytes, size_t n) {
    templates.clear(); words.clear();
    if(!bytes||n==0||n>=65536)return false;
    const char *end=nullptr;
    cJSON *root=cJSON_ParseWithLengthOpts(bytes,n+1,&end,true);
    if(!root) return false;
    auto *version=cJSON_GetObjectItemCaseSensitive(root,"version");
    auto *ts=cJSON_GetObjectItemCaseSensitive(root,"templates");
    auto *ws=cJSON_GetObjectItemCaseSensitive(root,"wordlists");
    bool ok=cJSON_IsNumber(version)&&version->valuedouble==1&&cJSON_IsArray(ts)&&cJSON_IsObject(ws);
    if(ok) {
        cJSON *item=nullptr;
        cJSON_ArrayForEach(item,ws) {
            if(!item->string||!cJSON_IsArray(item)||cJSON_GetArraySize(item)>256) {ok=false;break;}
            auto &list=words[item->string]; cJSON *word=nullptr;
            cJSON_ArrayForEach(word,item) {
                if(!cJSON_IsString(word)||!word->valuestring||std::char_traits<char>::length(word->valuestring)>512) {ok=false;break;}
                list.emplace_back(word->valuestring);
            }
            if(!ok)break;
        }
        cJSON_ArrayForEach(item,ts) {
            if(!cJSON_IsString(item)||!item->valuestring||std::char_traits<char>::length(item->valuestring)>1024||templates.size()>=256) {ok=false;break;}
            templates.emplace_back(item->valuestring);
        }
        if(templates.empty())ok=false;
        for(const auto &text:templates) if(!tokens(text))ok=false;
    }
    cJSON_Delete(root);
    if(!ok){templates.clear();words.clear();}
    return ok;
}
}
bool load(const char *path) {
    templates.clear(); words.clear();
    if(!path) return false;
    FILE *f=std::fopen(path,"rb"); if(!f) return false;
    std::vector<char> bytes(65537);
    const size_t n=std::fread(bytes.data(),1,65536,f);
    const bool valid=!std::ferror(f)&&n>0&&n<65536;
    std::fclose(f); if(!valid) return false;
    bytes[n]=0;
    return parse(bytes.data(),n);
}
bool load_builtin() {return parse(builtin::json,sizeof(builtin::json)-1);}

std::string generate() {
    if(templates.empty())return "The spirits are silent...";
    const std::string &text=templates[esp_random()%templates.size()];
    std::string result; size_t cursor=0;
    std::map<std::string,std::string> replacements;
    while(cursor<text.size()) {
        const size_t start=text.find("{{",cursor);
        if(start==std::string::npos){result+=text.substr(cursor);break;}
        result+=text.substr(cursor,start-cursor);
        const size_t end=text.find("}}",start+2);
        std::string key=text.substr(start+2,end-start-2);
        const auto first=key.find_first_not_of(" \t"),last=key.find_last_not_of(" \t");
        key=key.substr(first,last-first+1);
        if(!replacements.count(key))replacements[key]=words[key][esp_random()%words[key].size()];
        result+=replacements[key]; cursor=end+2;
        if(result.size()>1024)return "The spirits are silent...";
    }
    return result.size()<=1024?result:"The spirits are silent...";
}
}
