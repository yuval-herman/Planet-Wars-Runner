#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "game.h"
#include "game_log.h"

DefineComplexStruct(TournametData, {
  GameLog *items;
  unsigned capacity;
  unsigned count;
});

TournametData RunTournament(const char *map_file_path, PlayerDA players);

GameLog RunMatch(GameState *state, PlayerDA players);
#endif // TOURNAMENT_H
