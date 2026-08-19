#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "nob.h"

#define DEFINE_TESTS(name, ...)                                                \
  static int inner__log_start_##name(void **state) {                           \
    NOB_UNUSED(state);                                                         \
    nob_log(NOB_INFO, "========== Testing %s ==========", __FILE_NAME__);      \
    return 0;                                                                  \
  }                                                                            \
  static int inner__log_end_##name(void **state) {                             \
    NOB_UNUSED(state);                                                         \
    nob_log(NOB_INFO, "========== Done %s ==========", __FILE_NAME__);         \
    return 0;                                                                  \
  }                                                                            \
  static inline int test_##name() {                                            \
    const struct CMUnitTest test_array[] = {__VA_ARGS__};                      \
    return cmocka_run_group_tests_name(                                        \
        #name, test_array, inner__log_start_##name, inner__log_end_##name);    \
  }

#endif // TEST_UTILS_H
