#ifndef UTILS_H
#define UTILS_H

#include "nob.h"

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

#if defined(__STRICT_ANSI__)
#define FORCEINLINE
#elif defined(_MSC_VER)
#define FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define FORCEINLINE __inline__ __attribute__((__always_inline__))
#else
#define FORCEINLINE inline
#endif

// Returns the index of the set bit in x if only one bit is set. Else return -1.
static FORCEINLINE int bit_index(uint32_t x) {
  return (x && !(x & (x - 1))) ? __builtin_ctz(x) : -1;
}

void sleep_ns(uint64_t ns);
void sleep_ms(unsigned ms);

// Reads the entire file, supports text mode.
// If the file is bigger then `max_size` nothing is read and en error is
// returned.
//
// The returned buffer is NUL-terminated.
// Caller must free(*data).
//
// Returns true on success, false on failure.
bool ReadEntireFile(FILE *file, size_t max_size, char **data, size_t *length);

// A version of `SplitStringByDelim` that accepts a Nob_Cmd pointer. If you
// already used one and it has memory this can save allocations and time.
void SplitStringByDelimEx(Nob_Cmd *split_str, const char *str, char delim);

Nob_Cmd SplitStringByDelim(const char *str, char delim);

char *DupeString(const char *str);

char **DupeMultiDString(char const *const *md_str, unsigned dim);

void FreeMultiDString(char **md_str, unsigned dim);

int ParseBool(const char *str);
#endif // UTILS_H
