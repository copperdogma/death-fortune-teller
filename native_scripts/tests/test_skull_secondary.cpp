#include "skull_secondary_protocol.h"
#include <cassert>
#include <cstring>
using namespace skull::secondary::protocol;
int main() {
    Assembly a;
    const char *path = "/audio/Skit - names.wav";
    auto *bytes = reinterpret_cast<const uint8_t *>(path);
    assert(a.append(8, bytes + 8, std::strlen(path)-8)); assert(!a.complete());
    assert(a.append(0, bytes, 8)); assert(a.complete());
    assert(a.append(0, bytes, 8)); assert(a.complete()); // Identical retransmit.
    const uint8_t different[] = {'X'};
    assert(!a.append(0, different, 1)); assert(!a.complete());
    a.reset(); assert(!a.complete());
    uint8_t maximum[max_path]; std::memset(maximum, 'a', sizeof(maximum));
    for (size_t i = 0; i < sizeof(maximum); i += 18)
        assert(a.append(i, maximum+i, sizeof(maximum)-i < 18 ? sizeof(maximum)-i : 18));
    assert(a.complete()); assert(a.length == max_path);
    assert(!a.append(max_path, different, 1)); assert(!a.complete());
    a.reset(); assert(!a.append(max_path+1, different, 1));
    a.reset(); uint8_t nul[] = {'a', 0, 'b'}; assert(!a.append(0, nul, 3)); assert(!a.complete());
    a.reset(); assert(!a.append(0, bytes, 0)); assert(!a.complete());
    a.reset(); assert(a.append(0, bytes, 1)); assert(a.append(2, bytes+2, 1)); assert(!a.complete());
    a.reset(); assert(!a.append(0, nullptr, 1));
    Ticket t; auto first = t.issue(); assert(t.matches(first));
    t.clear(); assert(!t.matches(first)); assert(!t.matches(0));
    auto second = t.issue(); assert(second != first); assert(!t.matches(first)); assert(t.matches(second));
    t.serial = UINT32_MAX; t.clear(); assert(t.issue() == 1); assert(!t.matches(0));
}
