#include <unity.h>
#include "fortune_generator.h"
#include "fake_filesystem.h"
#include "infra/random_source.h"
#include "infra/log_sink.h"
#include "fake_log_sink.h"
#include "fixture_loader.h"

class FakeRandomSource : public infra::IRandomSource {
public:
    int nextInt(int minInclusive, int maxExclusive) override {
        (void)minInclusive;
        (void)maxExclusive;
        return forcedValue;
    }

    int forcedValue = 0;
};

static constexpr const char *FORTUNE_TAG = "FortuneGenerator";

void setUp(void) {}
void tearDown(void) {}

static void test_load_fortunes_from_fake_fs(void) {
    FakeFileSystem fs;
    fs.addFile("/printer/fortunes_littlekid.json", loadFixture("fortune_valid.json"));

    FortuneGenerator generator;
    FakeRandomSource random;
    infra::setLogSink(nullptr);
    generator.setFileSystem(&fs);
    generator.setRandomSource(&random);

    TEST_ASSERT_TRUE_MESSAGE(generator.loadFortunes("/printer/fortunes_littlekid.json"),
                             "FortuneGenerator should load fortunes via injected filesystem");
    TEST_ASSERT_TRUE(generator.isLoaded());

    random.forcedValue = 0;
    String fortune = generator.generateFortune();
    TEST_ASSERT_EQUAL_STRING("Hello World!", fortune.c_str());
}

static void test_load_fails_without_version(void) {
    FakeFileSystem fs;
    fs.addFile("/printer/fortunes_littlekid.json", loadFixture("fortune_missing_version.json"));

    FortuneGenerator generator;
    FakeRandomSource random;
    FakeLogSink logSink;
    logSink.clear();
    generator.setFileSystem(&fs);
    generator.setRandomSource(&random);
    generator.setLogSink(&logSink);

    TEST_ASSERT_FALSE(generator.loadFortunes("/printer/fortunes_littlekid.json"));
    TEST_ASSERT_FALSE(generator.isLoaded());

    bool foundError = false;
    for (const auto &entry : logSink.entries) {
        if (entry.level == infra::LogLevel::Error && entry.tag == FORTUNE_TAG) {
            foundError = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(foundError, "Expected error log when fortune version missing");
    infra::setLogSink(nullptr);
}

static void test_load_fails_when_wordlist_missing_token(void) {
    FakeFileSystem fs;
    fs.addFile("/printer/fortunes_littlekid.json", loadFixture("fortune_missing_wordlist.json"));

    FortuneGenerator generator;
    FakeRandomSource random;
    FakeLogSink logSink;
    logSink.clear();
    infra::setLogSink(nullptr);
    generator.setFileSystem(&fs);
    generator.setRandomSource(&random);
    generator.setLogSink(&logSink);
    logSink.clear();

    TEST_ASSERT_FALSE(generator.loadFortunes("/printer/fortunes_littlekid.json"));
    TEST_ASSERT_FALSE(generator.isLoaded());

    bool foundError = false;
    for (const auto &entry : logSink.entries) {
        if (entry.level == infra::LogLevel::Error && entry.tag == FORTUNE_TAG) {
            foundError = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(foundError, "Expected error log when wordlist missing token");
    infra::setLogSink(nullptr);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_load_fortunes_from_fake_fs);
    RUN_TEST(test_load_fails_without_version);
    RUN_TEST(test_load_fails_when_wordlist_missing_token);
    return UNITY_END();
}
