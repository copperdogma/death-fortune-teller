#include "death_fortune.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <unistd.h>
uint32_t entropy=0;
uint32_t esp_random(){return entropy++;}
void fixture(const char *text){FILE *f=std::fopen("/tmp/death-fortune-host.json","w");assert(f);std::fputs(text,f);std::fclose(f);}
int main(){
    using namespace skull::fortune;
    fixture(R"({"version":1,"templates":["{{ x }} {{x}}"],"wordlists":{"x":["cat","dog"]}})");
    assert(load("/tmp/death-fortune-host.json"));const auto value=generate();assert(value=="dog dog"||value=="cat cat");
    fixture(R"({"version":1,"templates":["{{x}}"],"wordlists":{"x":[]}})");assert(!load("/tmp/death-fortune-host.json"));assert(generate()=="The spirits are silent...");
    fixture(R"({"version":1,"templates":[],"wordlists":{}})");assert(!load("/tmp/death-fortune-host.json"));
    fixture(R"({"version":1,"templates":["{{bad"],"wordlists":{}})");assert(!load("/tmp/death-fortune-host.json"));
    fixture(R"({"version":1,"templates":["literal\nfortune"],"wordlists":{}})");assert(load("/tmp/death-fortune-host.json"));assert(generate()=="literal\nfortune");
    fixture(R"({"version":1,"templates":["x"],"wordlists":{}}trailing)");assert(!load("/tmp/death-fortune-host.json"));
    assert(!load("/missing"));assert(generate()=="The spirits are silent...");
    // Legacy SD templates can omit spaces at sentence boundaries after tokens.
    fixture(R"({"version":1,"templates":["Beware {{x}}.Seek ye a carriage.On the dawn..."],"wordlists":{"x":["blue trousers"]}})");
    assert(load("/tmp/death-fortune-host.json"));
    assert(generate()=="Beware blue trousers. Seek ye a carriage. On the dawn...");
    fixture(R"({"version":1,"templates":["One. Two.\nThree.  Four.\tFive. Value 3.14, U.S.A., Mr. Smith...Next"],"wordlists":{}})");
    assert(load("/tmp/death-fortune-host.json"));
    assert(generate()=="One. Two.\nThree.  Four.\tFive. Value 3.14, U.S.A., Mr. Smith...Next");
    // Added spaces must not exceed the printer's 1024-byte fortune bound.
    const std::string long_prose=std::string(1018,'a')+".B.C.D";
    fixture(("{\"version\":1,\"templates\":[\""+long_prose+"\"],\"wordlists\":{}}").c_str());
    assert(load("/tmp/death-fortune-host.json"));assert(generate()=="The spirits are silent...");
    // The exact verified card template remains usable when SD loading fails.
    assert(load_builtin());
    for(unsigned i=0;i<100;++i){const auto built=generate();
        assert(!built.empty()&&built!="The spirits are silent...");
        assert(built.find("Beware the ")==0&&built.find("Seek ye ")!=std::string::npos);
        assert(built.find(". Seek ye ")!=std::string::npos&&built.find(". On the dawn ")!=std::string::npos);
        assert(built.find("{{")==std::string::npos&&built.find("}}" )==std::string::npos);
        assert(built.size()<=1024);
    }
    // A failed SD load must still clear previously loaded embedded content.
    assert(!load("/missing"));assert(generate()=="The spirits are silent...");
    assert(load_builtin());assert(!load(nullptr));assert(generate()=="The spirits are silent...");
    assert(load_builtin());assert(generate()!="The spirits are silent...");
    unlink("/tmp/death-fortune-host.json");std::puts("Death fortune scenarios passed");
}
