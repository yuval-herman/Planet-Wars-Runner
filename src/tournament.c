#include "tournament.h"

#include <stdatomic.h>
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
  return new_tournament;
}

void FreeInnerTournametData(TournametData tournament) {
  for (unsigned i = 0; i < tournament.count; i++) {
    FreeInnerGameLog(tournament.items[i]);
  }
  nob_da_free(tournament);
}

#define sbAppendBotName(sb, idx)                                               \
  if (match_args->bots.items[idx].name)                                        \
    nob_sb_append_cstr(sb, match_args->bots.items[idx].name);                  \
  else                                                                         \
    nob_sb_appendf(sb, "Player %u", idx + 1);

typedef struct {
  unsigned p1_idx;
  unsigned p2_idx;
  BotsDA bots;
  mtx_t *idx_mtx;
  mtx_t *file_write_mtx;
  mtx_t *tournament_data_mtx;
  FILE *tournament_data_file;
  const char *map_file_path;
  TournametData *tournament_data;
} MatchRunnerArgs;

static bool AdvancePlayerIndex(MatchRunnerArgs *args, unsigned *out_p1,
                               unsigned *out_p2) {
  mtx_lock(args->idx_mtx);

  if (args->p1_idx >= args->bots.count - 1) {
    mtx_unlock(args->idx_mtx);
    return false;
  }

  *out_p1 = args->p1_idx;
  *out_p2 = args->p2_idx;

  args->p2_idx++;
  if (args->p2_idx >= args->bots.count) {
    args->p1_idx++;
    args->p2_idx = args->p1_idx + 1;
  }

  mtx_unlock(args->idx_mtx);
  return true;
}

int ThrdMatchRunner(void *args) {
  MatchRunnerArgs *match_args = args;
  Nob_String_Builder sb = {0};
  Nob_String_Builder temp_sb = {0};
  BotsDA playing_bots = {0};
  playing_bots.count = 2;
  playing_bots.capacity = playing_bots.count;
  playing_bots.items = malloc(sizeof *playing_bots.items * playing_bots.count);

  unsigned p1_idx;
  unsigned p2_idx;

  while (AdvancePlayerIndex(match_args, &p1_idx, &p2_idx)) {
    sb.count = 0;
    temp_sb.count = 0;

    nob_sb_append_cstr(&sb, "match:\n");
    sbAppendBotName(&sb, p1_idx);
    nob_sb_append(&sb, '\n');
    sbAppendBotName(&sb, p2_idx);
    nob_sb_append(&sb, '\n');

    playing_bots.items[0] = match_args->bots.items[p1_idx];
    playing_bots.items[1] = match_args->bots.items[p2_idx];
    // nob_log(NOB_INFO, "Running match between %s and %s", GetBotName(p1_idx),
    //         GetBotName(p2_idx));

    // Run a full game, silence normal logging so we don't clog the terminal
    // TODO split the `MakeGame` function to more sub-functions so we can load
    // maps, bots and other stuff once instead of on every match.
    GameState state = MakeGame(match_args->map_file_path, playing_bots, false);
    RunGame(&state);

    GameLog game_log_copy = DeepCopyGameLog(state.game_log);
    // Free as soon as possible to destroy bot threads
    FreeInnerGameState(state);

    if (game_log_copy.draw) {
      // nob_log(NOB_INFO, "Game ended in a draw");
      nob_sb_append_cstr(&sb, "draw\n");
    } else {
      unsigned winner_idx = game_log_copy.winning_bot == 0 ? p1_idx : p2_idx;
      // nob_log(NOB_INFO, "%s won.", GetBotName(winner_idx));
      nob_sb_append_cstr(&sb, "winner: ");
      sbAppendBotName(&sb, winner_idx);
      nob_sb_append(&sb, '\n');
    }

    temp_sb.count = 0;
    nob_sb_append_cstr(&temp_sb, "tournament/match_");
    sbAppendBotName(&temp_sb, p1_idx);
    nob_sb_append(&temp_sb, '-');
    sbAppendBotName(&temp_sb, p2_idx);
    nob_sb_append_cstr(&temp_sb, ".plws");
    sb_append_null(&temp_sb);

    char *save_file_name = temp_sb.items;
    nob_sb_appendf(&sb, "save file name: %s\n", save_file_name);

    FILE *save_file = fopen(save_file_name, "wb");
    WriteGameLogToFile(save_file, game_log_copy);
    fclose(save_file);

    mtx_lock(match_args->tournament_data_mtx);
    nob_da_append(match_args->tournament_data, game_log_copy);
    mtx_unlock(match_args->tournament_data_mtx);

    mtx_lock(match_args->file_write_mtx);
    fwrite(sb.items, 1, sb.count, match_args->tournament_data_file);
    mtx_unlock(match_args->file_write_mtx);
  }
  nob_da_free(playing_bots);
  nob_sb_free(sb);
  nob_sb_free(temp_sb);
  return 0;
}

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

  unsigned match_count = bots.count * (bots.count - 1) / 2;
  // Could be nob_nprocs()-1 because we use the current thread to manage it all,
  // but this thread doesn't do a lot of work, mainly waits around, so I think
  // it fine that way.
  const unsigned proc_count = nob_nprocs();
  const unsigned thread_count =
      match_count < proc_count ? match_count : proc_count;
  thrd_t *thread_pool = malloc(sizeof *thread_pool * thread_count);

  mtx_t file_write_mtx;
  mtx_init(&file_write_mtx, mtx_plain);
  mtx_t tournament_data_mtx;
  mtx_init(&tournament_data_mtx, mtx_plain);
  mtx_t idx_mtx;
  mtx_init(&idx_mtx, mtx_plain);

  MatchRunnerArgs thread_args = {
      .p1_idx = 0,
      .p2_idx = 1,
      .idx_mtx = &idx_mtx,
      .bots = bots,
      .file_write_mtx = &file_write_mtx,
      .map_file_path = map_file_path,
      .tournament_data = &tournament,
      .tournament_data_mtx = &tournament_data_mtx,
      .tournament_data_file = tournament_data_file,
  };

  nob_log(NOB_INFO,
          "Running tournament between %u bots. This will run %u matches.",
          bots.count, match_count);
  // All normal logging must stop as this functions are not thread-safe
  nob_minimal_log_level = NOB_WARNING;
  for (unsigned i = 0; i < thread_count; i++) {
    thrd_create(&thread_pool[i], ThrdMatchRunner, &thread_args);
  }
  for (unsigned i = 0; i < thread_count; i++) {
    thrd_join(thread_pool[i], NULL);
  }
  nob_minimal_log_level = NOB_INFO;
  nob_log(NOB_INFO,
          "Tournament finished, data is saved in `tournament` directory.");

  fclose(tournament_data_file);
  free(thread_pool);
  mtx_destroy(&file_write_mtx);
  mtx_destroy(&tournament_data_mtx);
  mtx_destroy(&idx_mtx);

  return tournament;
}
