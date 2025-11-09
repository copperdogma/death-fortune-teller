#include <unity.h>
#include "audio_directory_selector.h"
#include "infra/random_source.h"

#include <vector>

namespace {

class StubEnumerator : public AudioDirectorySelector::IFileEnumerator {
public:
    bool listWavFiles(const String &, std::vector<String> &out) override {
        out = clips;
        return true;
    }

    std::vector<String> clips;
};

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

}  // namespace

static void test_selectClip_avoids_immediate_repeat(void) {
    StubEnumerator enumerator;
    enumerator.clips = {"/audio/test/A.wav", "/audio/test/B.wav"};
    StubRandom random;
    random.values = {0, 0, 0};
    unsigned long currentTime = 1000;

    AudioDirectorySelector::Dependencies deps;
    deps.enumerator = &enumerator;
    deps.randomSource = &random;
    deps.nowFn = [&]() -> unsigned long { return currentTime; };

    AudioDirectorySelector selector(deps);

    String first = selector.selectClip("/audio/test");
    TEST_ASSERT_EQUAL_STRING("/audio/test/A.wav", first.c_str());

    currentTime += 1000;
    String second = selector.selectClip("/audio/test");
    TEST_ASSERT_EQUAL_STRING("/audio/test/B.wav", second.c_str());
}

static void test_selectClip_returns_empty_when_no_clips(void) {
    StubEnumerator enumerator;
    StubRandom random;
    unsigned long currentTime = 500;

    AudioDirectorySelector::Dependencies deps;
    deps.enumerator = &enumerator;
    deps.randomSource = &random;
    deps.nowFn = [&]() -> unsigned long { return currentTime; };

    AudioDirectorySelector selector(deps);
    String clip = selector.selectClip("/audio/empty");
    TEST_ASSERT_TRUE_MESSAGE(clip.isEmpty(), "Expected empty string for empty directory");
}

static void test_refresh_handles_removed_clips(void) {
    StubEnumerator enumerator;
    enumerator.clips = {"/audio/test/A.wav", "/audio/test/B.wav"};
    StubRandom random;
    random.values = {0, 0};
    unsigned long currentTime = 100;

    AudioDirectorySelector::Dependencies deps;
    deps.enumerator = &enumerator;
    deps.randomSource = &random;
    deps.nowFn = [&]() -> unsigned long { return currentTime; };

    AudioDirectorySelector selector(deps);

    String first = selector.selectClip("/audio/test");
    TEST_ASSERT_EQUAL_STRING("/audio/test/A.wav", first.c_str());

    // Remove the previously selected clip from the enumerator results.
    enumerator.clips = {"/audio/test/B.wav"};
    currentTime += 1000;

    String second = selector.selectClip("/audio/test");
    TEST_ASSERT_EQUAL_STRING("/audio/test/B.wav", second.c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_selectClip_avoids_immediate_repeat);
    RUN_TEST(test_selectClip_returns_empty_when_no_clips);
    RUN_TEST(test_refresh_handles_removed_clips);
    return UNITY_END();
}
