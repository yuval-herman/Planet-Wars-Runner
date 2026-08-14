#ifndef CONFIGS_H
#define CONFIGS_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  MODE_NULL = 0,
  MODE_TOURNAMENT,
  MODE_SINGLE_MATCH,
  MODE_REPLAY,
} RunMode;

typedef struct {
  RunMode mode;

  // External process bots participating in the game
  char **bot_names;
  char **bot_start_commands;
  unsigned bot_count;

  // Path for a map file, required for tournament or single match mode
  char *map_file;

  // Whether to write a plws file for the running game. Ignored for tournament
  // mode.
  bool write_save;

  // A path for a plws file to replay.
  char *save_file;
} Configs;

// Make a default config struct filled with sane values.
Configs MakeDefaultConfig();
// Parses an ini file and writes into `configs`.
// Returns true in case of successful parsing,
// prints error and returns false otherwise.
// This function may override configs already set in `configs`.
bool ParseConfigsFromIni(Configs *configs, FILE *ini_file);
// Parses CLI arguments and writes into `configs`.
// Returns true in case of successful parsing,
// prints error and returns false otherwise.
// This function may override configs already set in `configs`.
bool ParseConfigsFromCLI(Configs *configs, int argc, char *argv[]);

#endif // CONFIGS_H
