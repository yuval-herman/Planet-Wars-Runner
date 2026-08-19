#include <cmocka.h>

/* A test that will always pass */
static void null_test_success(void **state) {
    (void) state; /* unused */
}

/* A test that will always fail */
static void null_test_fail(void **state) {
    (void) state; /* unused */
    assert_true(0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(null_test_success),
        cmocka_unit_test(null_test_fail),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
