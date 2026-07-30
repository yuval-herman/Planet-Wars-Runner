#ifndef GAME_H
#define GAME_H
#include "nob.h"
#include "planet_wars.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>

#define LOG_FILE "log.txt"
// Time in nanoseconds, currently set to 100ms
#define MAX_BOT_RESPONSE_TIME (1000 * 1000 * 100)
// A string used to denote the end of a message by the bots and engine
#define MESSAGE_DELIMETER "go" NOB_LINE_END

typedef uint32_t BotBitset;
_Static_assert(MAX_BOT_AMOUNT <= sizeof(BotBitset) * CHAR_BIT,
               "BotBitset is not wide enough to hold max amount of bots");

DefineComplexStruct(LogEntry, {
  Fleet *fleets;
  Planet *planets;
  size_t planet_count;
  size_t fleet_count;
  int remaining_bots;
});

DefineComplexStruct(GameLog, {
  char **bot_commands;
  LogEntry *items;
  size_t count;
  size_t capacity;
  uint_fast8_t bot_amount;
  uint_fast8_t winning_bot;
});

typedef struct {
  struct subprocess_s *items;
  size_t count;
  size_t capacity;
} BotProcesses;

DefineComplexStruct(GameState, {
  GameLog game_log;
  PlanetDA planets;
  FleetsDA fleets;
  BotProcesses bot_processes;
  FILE *log_file;
  BotBitset bot_bit_set;
  int remaining_bots;
  int turn;
});

GameState MakeGame(const char *map_file_path,
                   const char *const bot_start_commands[], int bot_count,
                   bool log);
void StopAndFreeBots(BotProcesses *bot_processes);
void RunGame(GameState *state);
void UpdateStateFromLogEntry(GameState *state, size_t entry_idx);

#endif // GAME_H
