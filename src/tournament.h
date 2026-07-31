#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "game.h"

DefineComplexStruct(TournametData, {
  GameLog *items;
  size_t capacity;
  size_t count;
});

TournametData RunTournament(const char *map_file_path, BotsDA bots);
#endif // TOURNAMENT_H
