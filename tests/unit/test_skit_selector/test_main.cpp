#include <unity.h>
#include "skit_selector.h"
#include "infra/random_source.h"

#include <vector>

namespace {

class StubRandom : public infra::IRandomSource {
public:
    int nextInt(int minInclusive, int maxExclusive) override {
        if (maxExclusive <= minInclusive) {
            return minInclusive;
        }
        int span = maxExclusive - minInclusive;
        if (index < static_cast<int>(values.size())) {
            int value = values[index++] % span;
            if (value < 0) value += span;
            return minInclusive + value;
        }
        return minInclusive;
    }

    std::vector<int> values;
    int index = 0;
};

ParsedSkit makeSkit(const char *name) {
    ParsedSkit skit;
    skit.audioFile = String(name);
    return skit;
}

}  // namespace

static void test_selectNextSkit_avoids_immediate_repeat(void) {
    std::vector<ParsedSkit> skits = {makeSkit("skit_a.wav"), makeSkit("skit_b.wav")};
    StubRandom random;
    random.values = {0, 0};
    unsigned long currentTime = 100;

    SkitSelector selector(skits, &random, [&]() -> unsigned long { return currentTime; });

    ParsedSkit first = selector.selectNextSkit();
    TEST_ASSERT_EQUAL_STRING("skit_a.wav", first.audioFile.c_str());

    currentTime += 1000;
    ParsedSkit second = selector.selectNextSkit();
    TEST_ASSERT_EQUAL_STRING("skit_b.wav", second.audioFile.c_str());
}

static void test_updateSkitPlayCount_updates_stats(void) {
    std::vector<ParsedSkit> skits = {makeSkit("skit_a.wav")};
    StubRandom random;
    unsigned long currentTime = 200;

    SkitSelector selector(skits, &random, [&]() -> unsigned long { return currentTime; });

    selector.updateSkitPlayCount("skit_a.wav");
    currentTime += 500;
    ParsedSkit selected = selector.selectNextSkit();
    TEST_ASSERT_EQUAL_STRING("skit_a.wav", selected.audioFile.c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_selectNextSkit_avoids_immediate_repeat);
    RUN_TEST(test_updateSkitPlayCount_updates_stats);
    return UNITY_END();
}
