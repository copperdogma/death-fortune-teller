#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

static void test_sanity(void) {
    TEST_ASSERT_EQUAL_INT(1, 1);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_sanity);
    return UNITY_END();
}
