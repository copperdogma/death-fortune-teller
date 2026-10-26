#include "skull_wave.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;
void put16(Bytes &bytes, unsigned value) {
    bytes.push_back(value & 255);
    bytes.push_back((value >> 8) & 255);
}
void put32(Bytes &bytes, uint32_t value) {
    put16(bytes, value & 65535);
    put16(bytes, value >> 16);
}
void set32(Bytes &bytes, size_t offset, uint32_t value) {
    for (unsigned i = 0; i != 4; ++i) bytes[offset + i] = (value >> (8 * i)) & 255;
}
Bytes format() {
    Bytes result;
    put16(result, 1); put16(result, 2); put32(result, 44100);
    put32(result, 176400); put16(result, 4); put16(result, 16);
    return result;
}
void chunk(Bytes &body, const char *id, const Bytes &payload) {
    body.insert(body.end(), id, id + 4);
    put32(body, static_cast<uint32_t>(payload.size()));
    body.insert(body.end(), payload.begin(), payload.end());
    if (payload.size() & 1) body.push_back(0);
}
Bytes wave(const Bytes &body) {
    Bytes result{'R', 'I', 'F', 'F'};
    put32(result, static_cast<uint32_t>(body.size() + 4));
    result.insert(result.end(), {'W', 'A', 'V', 'E'});
    result.insert(result.end(), body.begin(), body.end());
    return result;
}
Bytes normal() {
    Bytes body;
    chunk(body, "fmt ", format());
    chunk(body, "data", {1, 2, 3, 4, 5, 6, 7, 8});
    return wave(body);
}
void check(const Bytes &bytes, bool expected, uint32_t offset = 44, uint32_t size = 8) {
    FILE *file = std::tmpfile();
    assert(file);
    assert(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    skull::wave::Info info{123, 456};
    const bool result = skull::wave::inspect(file, info);
    assert(result == expected);
    if (expected) {
        assert(info.data_offset == offset && info.data_bytes == size);
        assert(std::ftell(file) == static_cast<long>(offset));
        assert(std::fgetc(file) == bytes[offset]);
    } else {
        assert(info.data_offset == 0 && info.data_bytes == 0);
    }
    std::fclose(file);
}
}

int main(int argc, char **argv) {
    check(normal(), true);
    const auto complete = normal();
    for (size_t length = 0; length < complete.size(); ++length)
        check(Bytes(complete.begin(), complete.begin() + length), false);
    check({}, false);
    auto bytes = normal(); bytes[0] = 'X'; check(bytes, false);
    bytes = normal(); bytes[8] = 'X'; check(bytes, false);
    bytes = normal(); set32(bytes, 4, 3); check(bytes, false);
    bytes = normal(); set32(bytes, 4, UINT32_MAX); check(bytes, false);
    bytes = normal(); bytes.pop_back(); check(bytes, false);
    bytes = normal(); set32(bytes, 40, UINT32_MAX); check(bytes, false);
    bytes = normal(); set32(bytes, 40, 4); check(bytes, false); // Stray RIFF bytes.
    bytes = normal(); bytes.push_back(99); check(bytes, false); // Outside RIFF.

    // Each required format field is independently rejected when inconsistent.
    for (size_t field : {size_t(20), size_t(22), size_t(24), size_t(28), size_t(32), size_t(34)}) {
        bytes = normal(); bytes[field] ^= 1; check(bytes, false);
    }
    Bytes body;
    chunk(body, "data", {1, 2, 3, 4}); check(wave(body), false); // Missing fmt.
    body.clear(); chunk(body, "fmt ", format()); check(wave(body), false);
    chunk(body, "data", {}); check(wave(body), false);
    body.clear(); chunk(body, "fmt ", format());
    chunk(body, "data", {1, 2, 3}); check(wave(body), false);
    body.clear(); chunk(body, "fmt ", Bytes(15));
    chunk(body, "data", Bytes(4)); check(wave(body), false);
    body.clear(); chunk(body, "fmt ", format()); chunk(body, "fmt ", format());
    chunk(body, "data", Bytes(4)); check(wave(body), false);
    body.clear(); chunk(body, "fmt ", format()); chunk(body, "data", Bytes(4));
    chunk(body, "data", Bytes(4)); check(wave(body), false);
    body.clear(); chunk(body, "data", Bytes(4)); chunk(body, "fmt ", format());
    chunk(body, "data", Bytes(4)); check(wave(body), false);

    // Unknown odd chunks, extended fmt, data preceding fmt, and nonzero padding.
    body.clear(); chunk(body, "JUNK", {9, 8, 7});
    body.back() = 0xff;
    chunk(body, "data", {1, 2, 3, 4});
    auto extended = format(); extended.insert(extended.end(), {0, 0});
    chunk(body, "fmt ", extended); chunk(body, "LIST", {2});
    check(wave(body), true, 32, 4);
    bytes = wave(body); bytes.pop_back();
    set32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
    check(bytes, false); // Missing odd-chunk padding, even at RIFF end.
    body.clear(); chunk(body, "fmt ", format()); chunk(body, "data", Bytes(4));
    body.push_back(1); check(wave(body), false); // Partial chunk header.

    skull::wave::Info info{123, 456};
    assert(!skull::wave::inspect(nullptr, info));
    assert(info.data_offset == 0 && info.data_bytes == 0);

    for (int i = 1; i < argc; ++i) {
        FILE *file = std::fopen(argv[i], "rb");
        if (!file || !skull::wave::inspect(file, info)) {
            std::fprintf(stderr, "Rejected fixture: %s\n", argv[i]);
            return EXIT_FAILURE;
        }
        std::printf("Accepted %s: offset=%u bytes=%u\n", argv[i], info.data_offset, info.data_bytes);
        std::fclose(file);
    }
    std::puts("RIFF parser tests passed");
}
