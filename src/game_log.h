#ifndef FILE_H
#define FILE_H

#include "planet_wars.h"
#include "player.h"
#include "utils.h"

DefineComplexStruct(LogEntry, {
  Fleet *fleets;
  Planet *planets;
  unsigned planet_count;
  unsigned fleet_count;
  unsigned remaining_players;
});

DefineComplexStruct(GameLog, {
  LogEntry *items;
  PlayerDA players;
  unsigned count;
  unsigned capacity;
  uint8_t winning_player;
  bool draw; // If no one won (a draw) this will be set true
});

bool WriteGameLogToFile(FILE *file, GameLog game_log);
bool ReadGameLogFromFile(FILE *file, GameLog *game_log);

#endif // FILE_H
