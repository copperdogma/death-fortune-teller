#include "skull_catalog.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>
using namespace skull::catalog;
namespace fs = std::filesystem;
void put16(std::ofstream &f, unsigned n) { f.put(n & 255); f.put((n >> 8) & 255); }
void put32(std::ofstream &f, unsigned n) { put16(f, n & 65535); put16(f, n >> 16); }
void wav(const fs::path &path, unsigned frames = 4410) {
    std::ofstream f(path, std::ios::binary);
    f.write("RIFF", 4); put32(f, 36 + frames * 4); f.write("WAVEfmt ", 8);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, 44100);
    put32(f, 176400); put16(f, 4); put16(f, 16); f.write("data", 4); put32(f, frames * 4);
    for (unsigned i = 0; i < frames; ++i) put32(f, 0);
}
void text(const fs::path &path, const std::string &value) { std::ofstream(path) << value; }
int main(int argc, char **argv) {
    auto c = std::make_unique<Catalog>();
    char pattern[] = "/tmp/skull-catalog-XXXXXX";
    const fs::path dir = mkdtemp(pattern);
    assert(!scan(*c, dir.c_str()));
    wav(dir / "Skit - test.wav");
    text(dir / "Skit - test.txt", "\xef\xbb\xbf A,0,1\nB,2,2,0.25\nA,10,91\n");
    assert(scan(*c, dir.c_str()) && c->count == 1);
    const Asset &a = c->assets[0];
    assert(a.clipped_timing && a.line_count == 3 && a.lines[0].end_frame == 45);
    assert(speaking(a, 'A', 0) && speaking(a, 'A', 44));
    assert(!speaking(a, 'A', 45) && !speaking(a, 'B', 88));
    assert(speaking(a, 'B', 89) && !speaking(a, 'B', 177));
    assert(!speaking(a, 'A', 4410) && !speaking(a, 'X', 0));
    assert(select(*c, 1000, 30) == 0 && select(*c, 1001, 30) == 0);
    assert(a.play_count == 2 && a.last_played_ms == 1001);
    const std::vector<std::string> bad = {
        "A,0,0\n", "C,0,1\n", "A,-1,1\n", "A,0,2\nB,1,1\n",
        "A,0,2\nB,0,1\n", "A,100,1\n", "A,4294967295,1\n",
        "A,0,1,nan\n", "A,0,1,inf\n", "A,0,1,\n", "A,0,1,2,x\n",
        "A,0,1\n# comment\n", "A,0.5,1\n", "A,0,1junk\n", "\n",
        std::string(300, 'x') + "\n"
    };
    for (const auto &s : bad) { text(dir / "Skit - test.txt", s); assert(!scan(*c, dir.c_str())); }
    std::string lines;
    for (int i = 0; i < 129; ++i) lines += "A," + std::to_string(i) + ",1\n";
    wav(dir / "Skit - test.wav", 44100);
    text(dir / "Skit - test.txt", lines);
    assert(!scan(*c, dir.c_str()));
    fs::remove(dir / "Skit - test.txt");
    assert(!scan(*c, dir.c_str()));
    fs::remove(dir / "Skit - test.wav");
    wav(dir / "Initialized.wav");
    assert(scan(*c, dir.c_str()) && !c->assets[0].skit);
    assert(speaking(c->assets[0], 'B', 50) && select(*c, 1, 0) == -1);
    for (int i = 0; i < 5; ++i) {
        const auto stem = "Skit - " + std::to_string(i);
        wav(dir / (stem + ".wav")); text(dir / (stem + ".txt"), "A,0,50\n");
    }
    assert(scan(*c, dir.c_str()) && c->count == 6);
    int indices[5];
    for (size_t i = 0; i < c->count; ++i) if (c->assets[i].skit) {
        int number = c->assets[i].path[std::strlen("/audio/Skit - ")] - '0';
        indices[number] = static_cast<int>(i);
        c->assets[i].play_count = number;
    }
    for (int entropy = 0; entropy < 3; ++entropy) {
        c->last_selected = -1;
        for (int i = 0; i < 5; ++i) { c->assets[indices[i]].play_count = i; c->assets[indices[i]].last_played_ms = 0; }
        assert(select(*c, 1000, entropy) == indices[entropy]);
    }
    for (int i = 0; i < 50; ++i) {
        const int previous = c->last_selected;
        assert(select(*c, 2000 + i, i) != previous);
    }
    fs::remove_all(dir);
    fs::create_directory(dir);
    for (int i = 0; i < 33; ++i) wav(dir / ("clip" + std::to_string(i) + ".wav"), 1);
    wav(dir / (std::string(125, 'z') + ".wav"), 1);
    assert(scan(*c, dir.c_str()) && c->count == max_assets);
    fs::remove_all(dir);
    fs::create_directory(dir);
    wav(dir / (std::string(125, 'z') + ".wav"), 1);
    assert(!scan(*c, dir.c_str()));
    fs::remove_all(dir);
    fs::create_directories(dir / "welcome");
    fs::create_directories(dir / "finger_prompt");
    wav(dir / "welcome" / "welcome.wav");
    wav(dir / "finger_prompt" / "prompt.wav");
    fs::create_directory_symlink(dir, dir / "cycle");
    assert(scan(*c, dir.c_str()) && c->count == 2);
    bool welcome_found = false, prompt_found = false;
    for (size_t i = 0; i < c->count; ++i) {
        welcome_found |= !std::strcmp(c->assets[i].path, "/audio/welcome/welcome.wav");
        prompt_found |= !std::strcmp(c->assets[i].path, "/audio/finger_prompt/prompt.wav");
        assert(speaking(c->assets[i], 'A', 0));
    }
    assert(welcome_found && prompt_found);
    fs::remove_all(dir);
    if (argc > 1) {
        assert(scan(*c, argv[1]) && c->count == 15);
        int skits = 0, clipped = 0;
        for (size_t i = 0; i < c->count; ++i) { skits += c->assets[i].skit; clipped += c->assets[i].clipped_timing; }
        assert(skits == 11 && clipped == 0);
        bool names_found = false;
        for (size_t i = 0; i < c->count; ++i) {
            const Asset &asset = c->assets[i];
            if (std::strcmp(asset.path, "/audio/Skit - names.wav") == 0) {
                names_found = true;
                assert(asset.line_count == 15);
                assert(asset.lines[14].first_frame == 2745225);
                assert(asset.lines[14].end_frame == 2807098);
                assert(asset.lines[14].end_frame == asset.wave.data_bytes / 4);
            }
        }
        assert(names_found);
    }
    std::puts("catalog tests passed");
}
