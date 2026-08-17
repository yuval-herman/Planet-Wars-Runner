#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <strings.h>
#include <time.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
void sleep_ns(uint64_t ns) {
  // Create a manual-reset waitable timer
  HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
  if (!timer)
    return;

  // Negative values indicate relative time for SetWaitableTimer
  // Windows uses 100-nanosecond units.
  // Clamp to what fits in signed 64-bit.
  const int64_t ticks = (ns > (uint64_t)LLONG_MAX / 10ULL)
                            ? LLONG_MIN / 2
                            : -(int64_t)(ns / 100ULL); // ns -> 100ns

  LARGE_INTEGER due;
  due.QuadPart = ticks;

  // We don't need periodic behavior
  SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
  WaitForSingleObject(timer, INFINITE);

  CloseHandle(timer);
}
void sleep_ms(unsigned ms) { Sleep(ms); }
#else
void sleep_ns(uint64_t ns) {
  struct timespec ts;
  ts.tv_sec = (time_t)(ns / 1000000000ULL);
  ts.tv_nsec = (long)(ns % 1000000000ULL);
  nanosleep(&ts, NULL);
}
void sleep_ms(unsigned ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}
#endif

void SplitStringByDelimEx(Nob_Cmd *split_str, const char *str, char delim) {
  split_str->count = 0;
  Nob_String_View view = {0};
  view = nob_sv_from_cstr(str);

  while (view.count > 0) {
    Nob_String_View chop = nob_sv_chop_by_delim(&view, delim);
    char *chop_str = malloc(sizeof *chop_str * chop.count + 1);
    memcpy(chop_str, chop.data, chop.count);
    chop_str[chop.count] = '\0';
    nob_cmd_append(split_str, chop_str);
  }
}

Nob_Cmd SplitStringByDelim(const char *str, char delim) {
  Nob_Cmd split_str = {0};
  SplitStringByDelimEx(&split_str, str, delim);

  return split_str;
}

char *DupeString(const char *str) {
  unsigned len = strlen(str) + 1;
  char *new_str = malloc(sizeof *new_str * len);
  memcpy(new_str, str, len);
  return new_str;
}

char **DupeMultiDString(char const *const *md_str, unsigned dim) {
  char **new_md_str = malloc(sizeof *new_md_str * dim);
  for (unsigned i = 0; i < dim; i++) {
    new_md_str[i] = DupeString(md_str[i]);
  }
  return new_md_str;
}

void FreeMultiDString(char **md_str, unsigned dim) {
  for (unsigned i = 0; i < dim; i++) {
    free(md_str[i]);
  }
  free(md_str);
}

int ParseBool(const char *str) {
  if (!str)
    return -1;

#ifdef _WIN32
  if (_stricmp(str, "true") == 0)
    return 1;
  if (_stricmp(str, "false") == 0)
    return 0;
#else
  if (strcasecmp(str, "true") == 0)
    return 1;
  if (strcasecmp(str, "false") == 0)
    return 0;
#endif

  return -1;
}
