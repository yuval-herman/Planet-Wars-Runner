#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "game.h"

DefineComplexStruct(TournametData, {
  GameLog *items;
  size_t capacity;
  size_t count;
});

TournametData RunTournament(const char *map_file_path,
                            const char *const bot_start_commands[],
                            int bot_count);
#endif // TOURNAMENT_H
