#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "game.h"

DefineComplexStruct(TournametData, {
  GameLog *items;
  unsigned capacity;
  unsigned count;
});

TournametData RunTournament(const char *map_file_path, PlayerDA players);

void RunMatch(GameState *state);

void WriteGameLogToFile(FILE *file, GameLog game_log);
bool ReadGameLogFromFile(FILE *file, GameLog *game_log);
#endif // TOURNAMENT_H
