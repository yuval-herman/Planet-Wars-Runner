#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#define DEFINE_TESTS(name, ...)                                                \
  static inline int test_##name() {                                            \
    const struct CMUnitTest test_array[] = {__VA_ARGS__};                      \
    return cmocka_run_group_tests_name(#name, test_array, NULL, NULL);         \
  }

#endif // TEST_UTILS_H
