#ifndef PLANET_WARS_H
#define PLANET_WARS_H
#include "raymath.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_BOT_AMOUNT 32

// TODO change owner, growth, src_id, etc. to uint8_t/uint16_t. Add max start
// count for this
typedef struct {
  Vector2 coords;
  int owner;
  int ships;
  int growth;
  // Used to save time on printing data that does not change
  // TODO investigate whether 20 is always enough
  char print_prefix[20];
} Planet;

typedef struct {
  int owner;
  int ships;
  int src_id;
  int dst_id;
  int total;
  int remaining;
} Fleet;

typedef struct {
  Planet *items;
  size_t count;
  size_t capacity;
} PlanetDA;

typedef struct {
  Fleet *items;
  size_t count;
  size_t capacity;
} FleetsDA;

// Parses a float, advances s to the end of the parsed string, sets out to the
// parsed value. Returns true on success false otherwise.
static bool parse_float(const char **s, float *out) {
  char *end;
  errno = 0;
  // TODO make custom parser that will accept a string view without a null
  // terminator.
  float v = strtof(*s, &end);
  if (end == *s || errno == ERANGE || !isfinite(v))
    return false;
  *s = end;
  *out = v;
  return true;
}

// Parses a int, advances s to the end of the parsed string, sets out to the
// parsed value. Returns true on success false otherwise.
static bool parse_int(const char **s, int *out) {
  char *end;
  // TODO make custom parser that will accept a string view without a null
  // terminator.
  long v = strtol(*s, &end, 10);
  if (end == *s || v < INT_MIN || v > INT_MAX)
    return false;
  *s = end;
  *out = (int)v;
  return true;
}

static bool ParsePlanetLine(char *line, Planet *planet) {
  if (line[0] != 'P')
    return false;

  const char *s_idx = line + 1;
  Vector2 coords;
  int owner;
  int ships;
  int growth;
  if (!(parse_float(&s_idx, &coords.x) && parse_float(&s_idx, &coords.y) &&
        parse_int(&s_idx, &owner) && parse_int(&s_idx, &ships) &&
        parse_int(&s_idx, &growth))) {
    return false;
  }

  if (owner < 0 || owner > MAX_BOT_AMOUNT) {
    fprintf(stderr,
            "Invalid number of player owned planets. There must be "
            "between 1 to %d players.\n",
            MAX_BOT_AMOUNT);
    return false;
  }

  planet->coords = coords;
  planet->owner = owner;
  planet->ships = ships;
  planet->growth = growth;
  return true;
}

#endif // PLANET_WARS_H
