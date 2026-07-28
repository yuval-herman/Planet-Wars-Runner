#ifndef GAME_H
#define GAME_H
#include "nob.h"
#include "planet_wars.h"

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

typedef struct {
  struct subprocess_s *items;
  size_t count;
  size_t capacity;
} BotProcesses;

typedef struct {
  GameLog game_log;
  PlanetDA planets;
  FleetsDA fleets;
  BotBitset bot_bit_set;
  int remaining_bots;
  int turn;
  BotProcesses bot_processes;
  FILE *log_file;
} GameState;

GameState MakeGame(const char *map_file_path,
                   const char *const bot_start_commands[], int bot_count);
void FreeGameLog(GameLog *game_log);
void StopAndFreeBots(BotProcesses *bot_processes);
void RunGame(GameState *state);
// This will free allocated memory, stop bot processes, close open files etc.
void FreeState(GameState *state);
void UpdateStateFromLogEntry(GameState *state, size_t entry_idx);

#endif // GAME_H
