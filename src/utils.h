#ifndef UTILS_H
#define UTILS_H

#include "nob.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <strings.h>
#include <time.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ----- MACROS -----

#if defined(_MSC_VER)
// For MSVC (Visual Studio / Windows)
#define BREAKPOINT() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
// For GCC/Clang
#define BREAKPOINT() __builtin_trap()
#endif

#define SetBit(bitset, index)                                                  \
  do {                                                                         \
    bitset |= 1u << (index);                                                   \
  } while (0)

#define UnsetBit(bitset, index)                                                \
  do {                                                                         \
    bitset &= ~(1u << (index));                                                \
  } while (0)

#define TestBit(bitset, index) (bitset & 1u << (index))

#define ForceFunctionImplementation(func_ret, func_name, func_args)            \
  static func_ret (*const __force_impl_##func_name)(func_args)                 \
      __attribute__((used)) = func_name;

#define DefineComplexStruct(name, structure)                                   \
  typedef struct structure name;                                               \
  /* Dupes all pointers in the struct recursively and returns a copy with the  \
   * dupes. Does not allocate memory for the entire struct, only pointers      \
   * within it. */                                                             \
  name DeepCopy##name(name);                                                   \
  ForceFunctionImplementation(name, DeepCopy##name, name);                     \
  /* Free all pointers inside the struct recursively. */                       \
  /* TODO Maybe this function should accept a pointer? That way it could set   \
   * inner pointer fields to null which would help debugging in case of        \
   * errors. */                                                                \
  void FreeInner##name(name);                                                  \
  ForceFunctionImplementation(void, FreeInner##name, name);

// ----- FUNCTIONS -----

// Returns the index of the set bit in x if only one bit is set. Else return -1.
static inline int bit_index(uint32_t x) {
  return (x && !(x & (x - 1))) ? __builtin_ctz(x) : -1;
}

#ifdef _WIN32
static inline void sleep_ns(uint64_t ns) {
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
static inline void sleep_ms(unsigned ms) { Sleep(ms); }
#else
static inline void sleep_ns(uint64_t ns) {
  struct timespec ts;
  ts.tv_sec = (time_t)(ns / 1000000000ULL);
  ts.tv_nsec = (long)(ns % 1000000000ULL);
  nanosleep(&ts, NULL);
}
static inline void sleep_ms(unsigned ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}
#endif

// A version of `SplitStringByDelim` that accepts a Nob_Cmd pointer. If you
// already used one and it has memory this can save allocations and time.
inline static void SplitStringByDelimEx(Nob_Cmd *split_str, const char *str,
                                        char delim) {
  split_str->count = 0;
  Nob_String_View view = {0};
  view = nob_sv_from_cstr(str);

  while (view.count > 0) {
    nob_cmd_append(split_str,
                   nob_temp_sv_to_cstr(nob_sv_chop_by_delim(&view, delim)));
  }
}

inline static Nob_Cmd SplitStringByDelim(const char *str, char delim) {
  Nob_Cmd split_str = {0};
  SplitStringByDelimEx(&split_str, str, delim);

  return split_str;
}

inline static char **DupeMultiDString(char const *const *md_str, size_t dim) {
  char **new_md_str = malloc(sizeof *new_md_str * dim);
  for (size_t i = 0; i < dim; i++) {
    size_t cmd_len = strlen(md_str[i]) + 1;
    new_md_str[i] = malloc(sizeof *new_md_str[i] * cmd_len);
    memcpy(new_md_str[i], md_str[i], cmd_len);
  }
  return new_md_str;
}

inline static void FreeMultiDString(char **md_str, size_t dim) {
  for (size_t i = 0; i < dim; i++) {
    free(md_str[i]);
  }
  free(md_str);
}

inline static int ParseBool(const char *str) {
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
#endif // UTILS_H
