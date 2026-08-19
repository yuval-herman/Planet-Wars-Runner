#define RAYMATH_IMPLEMENTATION
#include "raymath.h"

#include "test_game.c"

#include <cmocka.h>

#include "miniz.c"

#define NOB_IMPLEMENTATION
#include "nob.h"

FILE *test_log_file = NULL;

NOBDEF void test_log_handler(Nob_Log_Level level, const char *fmt,
                             va_list args) {
  if (level < nob_minimal_log_level)
    return;

  switch (level) {
  case NOB_DEBUG:
    fprintf(test_log_file, "[DEBUG] ");
    break;
  case NOB_INFO:
    fprintf(test_log_file, "[INFO] ");
    break;
  case NOB_WARNING:
    fprintf(test_log_file, "[WARNING] ");
    break;
  case NOB_ERROR:
    fprintf(test_log_file, "[ERROR] ");
    break;
  case NOB_NO_LOGS:
    return;
  default:
    NOB_UNREACHABLE("Nob_Log_Level");
  }

  vfprintf(test_log_file, fmt, args);
  fprintf(test_log_file, "\n");
}

int main(void) {
  // We set up logging to disk instead of the terminal to avoid massive clutter
  // but save the logs to help in debugging if needed.
  test_log_file = fopen("tests.log", "w");
  if (!test_log_file) {
    printf("failed to open log file");
    return 1;
  }
  nob_set_log_handler(test_log_handler);

  test_game();

  fclose(test_log_file);
}
