#include "tournament.h"

#include <threads.h>

TournametData DeepCopyTournametData(TournametData tournament) {
  TournametData new_tournament = {
      .capacity = tournament.count,
      .count = tournament.count,
      .items = malloc(sizeof *tournament.items * tournament.count),
  };
  for (unsigned i = 0; i < tournament.count; i++) {
    new_tournament.items[i] = DeepCopyGameLog(tournament.items[i]);
  }
  return tournament;
}

void FreeInnerTournametData(TournametData tournament) {
  for (unsigned i = 0; i < tournament.count; i++) {
    FreeInnerGameLog(tournament.items[i]);
  }
  nob_da_free(tournament);
}

#define GetBotName(idx)                                                        \
  (bots.items[idx].name ? bots.items[idx].name                                 \
                        : nob_temp_sprintf("Player %u", idx + 1))

TournametData RunTournament(const char *map_file_path, BotsDA bots) {
  if (bots.count < 3) {
    nob_log(NOB_ERROR, "A tournament cannot be run for less then 3 bots.");
    exit(1);
  }

  nob_log(NOB_INFO, "Creating directory for tournament data.");
  if (!nob_mkdir_if_not_exists("tournament")) {
    nob_log(NOB_ERROR, "Failed creating tournament directory.");
    exit(1);
  }

  FILE *tournament_data_file = fopen("tournament/tournament.txt", "w");
  if (!tournament_data_file) {
    perror("Failed opening tournament file.");
    exit(1);
  }

  TournametData tournament = {0};
  BotsDA playing_bots = {0};
  playing_bots.count = 2;
  playing_bots.capacity = playing_bots.count;
  playing_bots.items = malloc(sizeof *playing_bots.items * playing_bots.count);

  unsigned match_num = 0;
  for (unsigned p1_idx = 0; p1_idx < bots.count - 1; p1_idx++) {
    for (unsigned p2_idx = p1_idx + 1; p2_idx < bots.count; p2_idx++) {
      size_t mark = nob_temp_save();
      match_num++;
      fprintf(tournament_data_file, "match %u:\n%s\n%s\n", match_num,
              GetBotName(p1_idx), GetBotName(p2_idx));

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

      if (game_log_copy.draw) {
        nob_log(NOB_INFO, "Game ended in a draw");
        fprintf(tournament_data_file, "draw\n");
      } else {
        unsigned winner_idx = game_log_copy.winning_bot == 0 ? p1_idx : p2_idx;
        nob_log(NOB_INFO, "%s won.", GetBotName(winner_idx));
        fprintf(tournament_data_file, "winner: %s\n", GetBotName(winner_idx));
      }

      char *save_file_name = nob_temp_sprintf("match_%u.plws", match_num);
      fprintf(tournament_data_file, "save file name: %s\n", save_file_name);

      FILE *save_file =
          fopen(nob_temp_sprintf("tournament/%s", save_file_name), "wb");
      WriteGameLogToFile(save_file, state.game_log);
      fclose(save_file);

      nob_da_append(&tournament, game_log_copy);
      FreeInnerGameState(state);
      nob_temp_rewind(mark);
    }
  }
  fclose(tournament_data_file);

  nob_da_free(playing_bots);
  return tournament;
}
