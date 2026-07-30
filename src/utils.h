#ifndef UTILS_H
#define UTILS_H

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include <stdint.h>
#define SetBit(bitset, index)                                                  \
  do {                                                                         \
    bitset |= 1u << (index);                                                   \
  } while (0)
#define UnsetBit(bitset, index)                                                \
  do {                                                                         \
    bitset &= ~(1u << (index));                                                \
  } while (0)
#define TestBit(bitset, index) (bitset & 1u << (index))

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
