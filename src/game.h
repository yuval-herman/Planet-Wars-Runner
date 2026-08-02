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
// How much time to sleep between checks on bots responses in nanoseconds,
// currently set to 100 microseconds
#define WAIT_SLEEP_TIME (1000 * 100)
// A string used to denote the end of a message by the bots and engine
#define MESSAGE_DELIMETER "go" NOB_LINE_END

typedef uint32_t BotBitset;
_Static_assert(MAX_BOT_AMOUNT <= sizeof(BotBitset) * CHAR_BIT,
               "BotBitset is not wide enough to hold max amount of bots");

DefineComplexStruct(LogEntry, {
  Fleet *fleets;
  Planet *planets;
  unsigned planet_count;
  unsigned fleet_count;
  unsigned remaining_bots;
});

DefineComplexStruct(Bot, {
  char *name;
  char *start_command;
  struct subprocess_s *process;
});

DefineComplexStruct(BotsDA, {
  Bot *items;
  unsigned count;
  unsigned capacity;
});

DefineComplexStruct(GameLog, {
  LogEntry *items;
  BotsDA bots;
  unsigned count;
  unsigned capacity;
  uint8_t winning_bot;
  bool draw; // If no one won (a draw) this will be set true
});

DefineComplexStruct(GameState, {
  GameLog game_log;
  PlanetDA planets;
  FleetsDA fleets;
  BotsDA bots;
  FILE *log_file;
  BotBitset bot_bit_set;
  unsigned remaining_bots;
  unsigned turn;
});

GameState MakeGame(const char *map_file_path, BotsDA bots, bool log);
void StopBot(Bot bot);
void StartBot(Bot bot);
void RunGame(GameState *state);
void UpdateStateFromLogEntry(GameState *state, unsigned entry_idx);
void WriteGameLogToFile(FILE *file, GameLog game_log);

#endif // GAME_H
