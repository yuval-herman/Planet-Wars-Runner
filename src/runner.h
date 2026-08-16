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
#endif // TOURNAMENT_H
