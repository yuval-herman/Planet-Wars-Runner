#include "tournament.h"

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

TournametData RunTournament(const char *map_file_path,
                            const char *const bot_start_commands[],
                            int bot_count) {
  TournametData tournament = {0};
  char const *playing_bot_commands[2];

  for (int p1_idx = 0; p1_idx < bot_count - 1; p1_idx++) {
    for (int p2_idx = p1_idx + 1; p2_idx < bot_count; p2_idx++) {
      playing_bot_commands[0] = bot_start_commands[p1_idx];
      playing_bot_commands[1] = bot_start_commands[p2_idx];

      // Run a full game, slilence normal logging so we don't clog the terminal
      GameState state = MakeGame(map_file_path, playing_bot_commands, 2, false);
      nob_minimal_log_level = NOB_WARNING;
      RunGame(&state);
      nob_minimal_log_level = NOB_INFO;

      GameLog game_log_copy = DeepCopyGameLog(state.game_log);

      nob_da_append(&tournament, game_log_copy);
      FreeInnerGameState(state);
    }
  }

  return tournament;
}
