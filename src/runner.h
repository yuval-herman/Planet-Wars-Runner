#ifndef TOURNAMENT_H
#define TOURNAMENT_H

#include "game.h"
#include "game_log.h"

DefineComplexStruct(TournametData, {
  GameLog *items;
  unsigned capacity;
  unsigned count;
});

bool RunTournament(TournametData *tournament, const char *map_file_path,
                   PlayerDA players);

// Run a game until only one player is left or a 1000 turns. If `game_log` isn't
// null, save the game in `game_log`.
bool RunMatch(GameLog *game_log, GameState *state, PlayerDA players);
// Run one turn for game state. If `game_log` isn't null, save the turn in
// `game_log`. Returns true if the game isn't over, false otherwise.
bool RunTurn(GameState *state, GameLog *game_log, PlayerDA players,
             Nob_String_Builder sb);
#endif // TOURNAMENT_H
