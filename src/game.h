#ifndef GAME_H
#define GAME_H
#include "bot.h"
#include "nob.h"
#include "planet_wars.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>

#define LOG_FILE "log.txt"

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

void RunGame(GameState *state);
// Get map representation for a specific player. Each player should see itself
// as player 1 according to the protocol. This function takes care of that. If
// `player_idx` is 0 the real map is returned with no modification.
void GetMapRepresentation(GameState *state, Nob_String_Builder *sb,
                          unsigned player_idx);

// Return true if the player can play this actions. Return false in case the
// player should be disqualified.
bool SendPlayerShips(GameState *state, unsigned player_idx, uint16_t src_id,
                     uint16_t dst_id, uint16_t ships);

// Play player actions from string
bool SendPlayerShipsStr(GameState *state, unsigned player_idx,
                        Nob_String_View order_sv);

void WriteGameLogToFile(FILE *file, GameLog game_log);
bool ReadGameLogFromFile(FILE *file, GameLog *game_log);

#endif // GAME_H
