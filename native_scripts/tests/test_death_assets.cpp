#include "skull_catalog.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <cstdio>
int main() {
    auto catalog = std::make_unique<skull::catalog::Catalog>();
    assert(skull::catalog::scan(*catalog, "sd-card-files/audio"));
    const char *categories[] = {"/audio/welcome/", "/audio/finger_prompt/", "/audio/finger_snap/",
        "/audio/no_finger/", "/audio/fortune_preamble/", "/audio/goodbye/"};
    for (const auto *prefix : categories) {
        bool found = false;
        for (size_t i = 0; i < catalog->count; ++i)
            found |= std::strncmp(catalog->assets[i].path, prefix, std::strlen(prefix)) == 0;
        assert(found);
    }
    bool startup = false;
    for (size_t i = 0; i < catalog->count; ++i) {
        const auto &asset = catalog->assets[i];
        assert(!asset.skit && asset.wave.data_bytes > 0);
        startup |= std::strcmp(asset.path, "/audio/initialized.wav") == 0;
    }
    assert(startup);
    std::printf("Death card assets accepted by production parser: %zu WAVs\n", catalog->count);
}
