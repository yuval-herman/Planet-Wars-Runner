#include "tournament.h"
#include "src/game.h"
#include <stdlib.h>

TournametData DeepCopyTournametData(TournametData tournament) {
  TournametData new_tournament = {
      .capacity = tournament.count,
      .count = tournament.count,
      .items = malloc(sizeof *tournament.items * tournament.count),
  };
  for (size_t i = 0; i < tournament.count; i++) {
    new_tournament.items[i] = DeepCopyGameLog(tournament.items[i]);
  }
  return tournament;
}

void FreeInnerTournametData(TournametData tournament) {
  for (size_t i = 0; i < tournament.count; i++) {
    FreeInnerGameLog(tournament.items[i]);
  }
  nob_da_free(tournament);
}

#define GetBotName(idx)                                                        \
  (bots.items[idx].name ? bots.items[idx].name                                 \
                        : nob_temp_sprintf("Player %zu", idx + 1))

TournametData RunTournament(const char *map_file_path, BotsDA bots) {
  if (bots.count < 3) {
    nob_log(NOB_ERROR, "A tournament cannot be run for less then 3 bots.");
    exit(1);
  }
  TournametData tournament = {0};
  BotsDA playing_bots = {0};
  playing_bots.count = 2;
  playing_bots.capacity = playing_bots.count;
  playing_bots.items = malloc(sizeof *playing_bots.items * playing_bots.count);

  for (size_t p1_idx = 0; p1_idx < bots.count - 1; p1_idx++) {
    for (size_t p2_idx = p1_idx + 1; p2_idx < bots.count; p2_idx++) {
      playing_bots.items[0] = bots.items[p1_idx];
      playing_bots.items[1] = bots.items[p2_idx];
      nob_log(NOB_INFO, "Running match between %s and %s", GetBotName(p1_idx),
              GetBotName(p2_idx));

      // Run a full game, silence normal logging so we don't clog the terminal
      // TODO split the `MakeGame` function to more sub-functions so we can load
      // maps, bots and other stuff once instead of on every match.
      GameState state = MakeGame(map_file_path, playing_bots, false);
      nob_minimal_log_level = NOB_WARNING;
      RunGame(&state);
      nob_minimal_log_level = NOB_INFO;

      GameLog game_log_copy = DeepCopyGameLog(state.game_log);
      size_t winner_idx = game_log_copy.winning_bot == 0 ? p1_idx : p2_idx;
      nob_log(NOB_INFO, "%s won.", GetBotName(winner_idx));

      nob_da_append(&tournament, game_log_copy);
      FreeInnerGameState(state);
    }
  }

  nob_da_free(playing_bots);
  return tournament;
}
