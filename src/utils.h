#ifndef UTILS_H
#define UTILS_H

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include <stdint.h>

// ----- MACROS -----

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
  void FreeInner##name(name);                                                  \
  ForceFunctionImplementation(void, FreeInner##name, name);

// ----- FUNCTIONS -----

// Returns the index of the set bit in x if only one bit is set. Else return -1.
static inline int bit_index(uint32_t x) {
  return (x && !(x & (x - 1))) ? __builtin_ctz(x) : -1;
}

#ifdef _WIN32
static inline void sleep_ms(unsigned ms) { Sleep(ms); }
#else
static inline void sleep_ms(unsigned ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}
#endif
#endif // UTILS_H
