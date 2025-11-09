#include <unity.h>
#include "infra/circular_audio_buffer.h"

#include <array>
#include <cstdint>

namespace {

template <std::size_t N>
std::array<uint8_t, N> makeSequential(uint8_t start = 0) {
    std::array<uint8_t, N> data{};
    for (std::size_t i = 0; i < N; ++i) {
        data[i] = static_cast<uint8_t>(start + i);
    }
    return data;
}

}  // namespace

static void test_circular_buffer_wraparound(void) {
    infra::CircularAudioBuffer<8> buffer;
    auto first = makeSequential<6>(1);
    auto second = makeSequential<6>(21);
    uint8_t out[8] = {};

    std::size_t writtenFirst = buffer.write(first.data(), first.size());
    TEST_ASSERT_EQUAL_UINT32(6, writtenFirst);
    TEST_ASSERT_EQUAL_UINT32(6, buffer.available());

    std::size_t readFirst = buffer.read(out, 4, false, false);
    TEST_ASSERT_EQUAL_UINT32(4, readFirst);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(first.data(), out, 4);
    TEST_ASSERT_EQUAL_UINT32(2, buffer.available());

    std::size_t writtenSecond = buffer.write(second.data(), second.size());
    TEST_ASSERT_EQUAL_UINT32(6, writtenSecond);
    TEST_ASSERT_EQUAL_UINT32(8, buffer.available());

    uint8_t outWrap[10] = {};
    std::size_t readSecond = buffer.read(outWrap, sizeof(outWrap), true, false);
    TEST_ASSERT_EQUAL_UINT32(8, readSecond);

    uint8_t expected[8] = {first[4], first[5],
                           second[0], second[1], second[2],
                           second[3], second[4], second[5]};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, outWrap, 8);
    for (std::size_t i = 8; i < sizeof(outWrap); ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, outWrap[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(0, buffer.available());
}

static void test_circular_buffer_force_silence(void) {
    infra::CircularAudioBuffer<4> buffer;
    auto payload = makeSequential<4>(100);
    buffer.write(payload.data(), payload.size());
    TEST_ASSERT_EQUAL_UINT32(4, buffer.available());

    uint8_t outMuted[4] = {};
    std::size_t readBytes = buffer.read(outMuted, sizeof(outMuted), true, true);
    TEST_ASSERT_EQUAL_UINT32(4, readBytes);
    for (uint8_t value : outMuted) {
        TEST_ASSERT_EQUAL_UINT8(0, value);
    }
    TEST_ASSERT_EQUAL_UINT32(0, buffer.available());
}

static void test_circular_buffer_partial_read_silence(void) {
    infra::CircularAudioBuffer<6> buffer;
    auto first = makeSequential<3>(10);
    buffer.write(first.data(), first.size());
    buffer.write(first.data(), 1);  // total 4 bytes
    TEST_ASSERT_EQUAL_UINT32(4, buffer.available());

    uint8_t out[6] = {};
    std::size_t readBytes = buffer.read(out, sizeof(out), true, false);
    TEST_ASSERT_EQUAL_UINT32(4, readBytes);
    for (std::size_t i = 0; i < readBytes; ++i) {
        TEST_ASSERT_EQUAL_UINT8(first[i % first.size()], out[i]);
    }
    for (std::size_t i = readBytes; i < sizeof(out); ++i) {
        TEST_ASSERT_EQUAL_UINT8(0, out[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(0, buffer.available());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_circular_buffer_wraparound);
    RUN_TEST(test_circular_buffer_force_silence);
    RUN_TEST(test_circular_buffer_partial_read_silence);
    return UNITY_END();
}
