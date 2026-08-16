#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "game.h"
#include "player.h"

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

DefineComplexStruct(TournametData, {
  GameLog *items;
  unsigned capacity;
  unsigned count;
});

TournametData RunTournament(const char *map_file_path, PlayerDA players);

GameLog RunMatch(GameState *state, PlayerDA players);

void WriteGameLogToFile(FILE *file, GameLog game_log);
bool ReadGameLogFromFile(FILE *file, GameLog *game_log);
#endif // TOURNAMENT_H
