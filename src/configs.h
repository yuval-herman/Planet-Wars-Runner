#ifndef CONFIGS_H
#define CONFIGS_H

#include <stdbool.h>

#include "player.h"

typedef enum {
  MODE_NULL = 0,
  MODE_TOURNAMENT,
  MODE_SINGLE_MATCH,
  MODE_REPLAY,
} RunMode;

typedef struct {
  RunMode mode;

  PlayerDA players;

  // Path for a map file, required for tournament or single match mode
  char *map_file;

  // Whether to write a plws file for the running game. Ignored for tournament
  // and replay mode.
  bool write_save;

  // A path for a plws file to load in replay mode, or to save into when using
  // `write_save`.
  char *save_file;
} Configs;

// Make a default config struct filled with sane values.
Configs MakeDefaultConfig();
// Parses CLI arguments and writes into `configs`.
// Returns true in case of successful parsing,
// prints error and returns false otherwise.
// This function may override configs already set in `configs`.
// If the CLI arguments specify an ini config filem it will also be read and
// parsed into `configs`.
bool ParseConfigsFromCLI(Configs *configs, int argc, char *argv[]);

#endif // CONFIGS_H
