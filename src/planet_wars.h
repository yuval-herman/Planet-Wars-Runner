#ifndef PLANET_WARS_H
#define PLANET_WARS_H

#include "ffc.h"
#include "raymath.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_PLAYER_AMOUNT 32

// TODO Add verification when accepting data, like parsing maps, that all values
// are within the limits we use here. For example allowing up to 255 turns for a
// fleet get to its destination might be a bit too extreme.
typedef struct {
  Vector2 coords;
  // Used to save time on printing data that does not change
  // TODO investigate whether 20 is always enough
  char print_prefix[20];
  uint8_t owner;
  uint8_t growth;
  uint16_t ships;
} Planet;

typedef struct {
  uint8_t owner;
  uint8_t total;
  uint8_t remaining;
  uint16_t ships;
  uint16_t src_id;
  uint16_t dst_id;
} Fleet;

typedef struct {
  Planet *items;
  unsigned count;
  unsigned capacity;
} PlanetDA;

typedef struct {
  Fleet *items;
  unsigned count;
  unsigned capacity;
} FleetsDA;

static bool ParsePlanetLine(const char *line, unsigned line_len,
                            Planet *planet) {
  if (line[0] != 'P')
    return false;

  // Skip the first 'P'
  const char *s_idx = line + 1;
  const char *e_idx = line + line_len;

  Vector2 coords;
  unsigned owner;
  unsigned ships;
  unsigned growth;
  ffc_result result;
  ffc_parse_options parse_options = ffc_parse_options_default();
  parse_options.format |= FFC_FORMAT_FLAG_SKIP_WHITE_SPACE;

#define ParseFloat(output)                                                     \
  result = ffc_from_chars_float_options(s_idx, e_idx, &output, parse_options); \
  if (result.outcome != FFC_OUTCOME_OK) {                                      \
    fprintf(stderr, "failed parsing " #output "\n");                           \
    return false;                                                              \
  }                                                                            \
  s_idx = result.ptr;

#define ParseUint(output)                                                      \
  result =                                                                     \
      ffc_from_chars_u32_options(s_idx, e_idx, 10, &output, parse_options);    \
  if (result.outcome != FFC_OUTCOME_OK || output > UINT16_MAX) {               \
    fprintf(stderr, "failed parsing " #output "\n");                           \
    return false;                                                              \
  }                                                                            \
  s_idx = result.ptr;

  ParseFloat(coords.x);
  ParseFloat(coords.y);

  ParseUint(owner);
  ParseUint(ships);
  ParseUint(growth);

  if (s_idx != e_idx && *s_idx != '\0' && *s_idx != '\n') {
    fprintf(stderr, "Planet line must terminate with a NULL, newline, or on "
                    "the last parsed number.");
    return false;
  }

#undef ParseFloat
#undef ParseUint

  if (owner > MAX_PLAYER_AMOUNT) {
    fprintf(stderr,
            "Invalid number of player owned planets. There must be "
            "between 1 to %d players.\n",
            MAX_PLAYER_AMOUNT);
    return false;
  }

  planet->coords = coords;
  planet->owner = owner;
  planet->ships = ships;
  planet->growth = growth;
  return true;
}

#endif // PLANET_WARS_H
