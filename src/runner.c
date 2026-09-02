#include "runner.h"

#include <stdatomic.h>
#include <stdio.h>
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

LogEntry DeepCopyLogEntry(LogEntry entry) {
  LogEntry new_entry = {
      .fleets = entry.fleet_count
                    ? malloc(sizeof *entry.fleets * entry.fleet_count)
                    : NULL,
      .planets = entry.planet_count
                     ? malloc(sizeof *entry.planets * entry.planet_count)
                     : NULL,
      .planet_count = entry.planet_count,
      .fleet_count = entry.fleet_count,
      .remaining_players = entry.remaining_players,
  };
  if (entry.fleet_count)
    memcpy(new_entry.fleets, entry.fleets,
           sizeof *entry.fleets * entry.fleet_count);
  if (entry.planet_count)
    memcpy(new_entry.planets, entry.planets,
           sizeof *entry.planets * entry.planet_count);
  return new_entry;
}

void FreeInnerLogEntry(LogEntry entry) {
  free(entry.fleets);
  free(entry.planets);
}

GameLog DeepCopyGameLog(GameLog game_log) {
  LogEntry *log_entries = malloc(sizeof *game_log.items * game_log.count);
  for (unsigned i = 0; i < game_log.count; i++) {
    log_entries[i] = DeepCopyLogEntry(game_log.items[i]);
  }

  GameLog new_game_log = {
      .count = game_log.count,
      .capacity = game_log.capacity,
      .winning_player = game_log.winning_player,
      .items = log_entries,
      .players = DeepCopyPlayerDA(game_log.players),
      .draw = game_log.draw,
  };
  return new_game_log;
}

void FreeInnerGameLog(GameLog game_log) {
  for (unsigned i = 0; i < game_log.count; i++) {
    FreeInnerLogEntry(game_log.items[i]);
  }
  free(game_log.items);
  FreeInnerPlayerDA(game_log.players);
}

#define sbAppendPlayerName(sb, idx)                                            \
  if (match_args->players.items[idx].name.count)                               \
    nob_sb_append_buf(sb, match_args->players.items[idx].name.items,           \
                      match_args->players.items[idx].name.count);              \
  else                                                                         \
    nob_sb_appendf(sb, "Player %u", idx + 1);

typedef struct {
  unsigned p1_idx;
  unsigned p2_idx;
  PlayerDA players;
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

  if (args->p1_idx >= args->players.count - 1) {
    mtx_unlock(args->idx_mtx);
    return false;
  }

  *out_p1 = args->p1_idx;
  *out_p2 = args->p2_idx;

  args->p2_idx++;
  if (args->p2_idx >= args->players.count) {
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
  PlayerDA playing_players = {0};
  playing_players.count = 2;
  playing_players.capacity = playing_players.count;
  playing_players.items =
      malloc(sizeof *playing_players.items * playing_players.count);

  unsigned p1_idx;
  unsigned p2_idx;

  while (AdvancePlayerIndex(match_args, &p1_idx, &p2_idx)) {
    sb.count = 0;
    temp_sb.count = 0;

    nob_sb_append_cstr(&sb, "match:\n");
    sbAppendPlayerName(&sb, p1_idx);
    nob_sb_append(&sb, '\n');
    sbAppendPlayerName(&sb, p2_idx);
    nob_sb_append(&sb, '\n');

    playing_players.items[0] = match_args->players.items[p1_idx];
    playing_players.items[1] = match_args->players.items[p2_idx];
    // nob_log(NOB_INFO, "Running match between %s and %s", GetBotName(p1_idx),
    //         GetBotName(p2_idx));

    // Run a full game, silence normal logging so we don't clog the terminal
    // TODO split the `MakeGame` function to more sub-functions so we can load
    // maps, bots and other stuff once instead of on every match.
    GameState state = {0};
    // TODO don't ignore failure
    MakeGame(&state, match_args->map_file_path, playing_players.count);
    GameLog game_log = {0};
    // TODO don't ignore failure
    RunMatch(&game_log, &state, playing_players);

    // Free as soon as possible to destroy bot threads
    FreeInnerGameState(state);

    if (game_log.draw) {
      // nob_log(NOB_INFO, "Game ended in a draw");
      nob_sb_append_cstr(&sb, "draw\n");
    } else {
      unsigned winner_idx = game_log.winning_player == 0 ? p1_idx : p2_idx;
      // nob_log(NOB_INFO, "%s won.", GetBotName(winner_idx));
      nob_sb_append_cstr(&sb, "winner: ");
      sbAppendPlayerName(&sb, winner_idx);
      nob_sb_append(&sb, '\n');
    }

    temp_sb.count = 0;
    nob_sb_append_cstr(&temp_sb, "tournament/match_");
    sbAppendPlayerName(&temp_sb, p1_idx);
    nob_sb_append(&temp_sb, '-');
    sbAppendPlayerName(&temp_sb, p2_idx);
    nob_sb_append_cstr(&temp_sb, ".plws");
    sb_append_null(&temp_sb);

    char *save_file_name = temp_sb.items;
    nob_sb_appendf(&sb, "save file name: %s\n", save_file_name);

    FILE *save_file = fopen(save_file_name, "wb");
    // TODO handle errors
    WriteGameLogToFile(save_file, game_log);
    fclose(save_file);

    mtx_lock(match_args->tournament_data_mtx);
    nob_da_append(match_args->tournament_data, game_log);
    mtx_unlock(match_args->tournament_data_mtx);

    mtx_lock(match_args->file_write_mtx);
    fwrite(sb.items, 1, sb.count, match_args->tournament_data_file);
    mtx_unlock(match_args->file_write_mtx);
  }
  nob_da_free(playing_players);
  nob_sb_free(sb);
  nob_sb_free(temp_sb);
  return 0;
}

bool RunTournament(TournametData *tournament, const char *map_file_path,
                   PlayerDA players) {
  // This is not an if that returns false because it is expected the caller
  // checks this. If the caller does not check it, they cannot recover even
  // after getting an error from this function.
  assert(players.count >= 3 &&
         "A tournament cannot be run for less then 3 bots.");

  nob_log(NOB_INFO, "Creating directory for tournament data.");
  if (!nob_mkdir_if_not_exists("tournament")) {
    nob_log(NOB_ERROR, "Failed creating tournament directory.");
    return false;
  }

  FILE *tournament_data_file = fopen("tournament/tournament.txt", "w");
  if (!tournament_data_file) {
    perror("Failed opening tournament file.");
    return false;
  }

  unsigned match_count = players.count * (players.count - 1) / 2;
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
      .players = players,
      .file_write_mtx = &file_write_mtx,
      .map_file_path = map_file_path,
      .tournament_data = tournament,
      .tournament_data_mtx = &tournament_data_mtx,
      .tournament_data_file = tournament_data_file,
  };

  nob_log(NOB_INFO,
          "Running tournament between %u bots. This will run %u matches.",
          players.count, match_count);
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

  return true;
}

void RunPlayerCycle(GameState *state, PlayerDA players,
                        Nob_String_Builder *sb) {
  unsigned bot_num = 0;
  nob_da_foreach(Player, player, &players) {
    // Skip disqualified or lost bots.
    if (TestBit(state->player_bit_set, bot_num)) {
      nob_log(NOB_DEBUG, "sending map to bot %u", bot_num);

      sb->count = 0;

      GetMapRepresentation(state, sb, bot_num);
      SendMessageToPlayer(*player, sb->items, sb->count);
    }
    bot_num++;
  }

  bot_num = 0;
  bool bot_okay = true;
  nob_da_foreach(Player, player, &players) {
    // Skip disqualified or lost bots.
    if (TestBit(state->player_bit_set, bot_num)) {
      sb->count = 0;
      bot_okay = GetPlayerMessage(*player, sb);

      if (bot_okay)
        bot_okay = SendPlayerShipsStr(state, bot_num,
                                      nob_sv_from_parts(sb->items, sb->count));

      if (bot_okay) {
        sb->count = 0;
        GetPlayerDebugMessage(*player, sb);
        if (sb->count)
          nob_log(NOB_INFO, "bot %.*s says: |%.*s|", (int)player->name.count,
                  player->name.items, (unsigned)sb->count, sb->items);
      } else {
        // We don't remove the player from the dynamic array because we use the
        // DA index to address different players. Instead it is marked as
        // disqualified and not used.
        StopPlayer(player);
      }

      nob_log(NOB_DEBUG, "done with bot %u, advancing to bot %u", bot_num,
              bot_num + 1);
    }
    bot_num++;
  }
}

bool RunTurn(GameState *state, GameLog *game_log, PlayerDA players,
             Nob_String_Builder sb) {
  // Bot communication
  sb.count = 0;
  RunPlayerCycle(state, players, &sb);

  // Game logic
  AdvanceTurn(state);

  if (game_log) {
    // Save state to game log
    LogEntry entry = DeepCopyLogEntry((LogEntry){
        .remaining_players = state->remaining_players,
        .fleet_count = state->fleets.count,
        .fleets = state->fleets.items,
        .planet_count = state->planets.count,
        .planets = state->planets.items,
    });

    nob_da_append(game_log, entry);
  }

  return state->remaining_players > 1;
}

bool RunMatch(GameLog *game_log, GameState *state, PlayerDA players) {
  // Reusable string builder to hold messages sent and received between the bots
  // and the engine.
  Nob_String_Builder sb = {0};

  nob_da_foreach(Player, player, &players) {
    if (!StartPlayer(player))
      return false;
  }

  for (int sim_turn = 0; sim_turn < 1000; sim_turn++) {
    nob_log(NOB_INFO, "Turn %d", sim_turn);
    if (!RunTurn(state, game_log, players, sb))
      break;
  }
  nob_log(NOB_INFO, "Game ended!");
  int winning_bot = bit_index(state->player_bit_set);
  if (winning_bot == -1) {
    nob_log(NOB_INFO, "It's a draw!");
    game_log->draw = true;
  } else {
    game_log->winning_player = winning_bot;
    game_log->draw = false;
    nob_log(NOB_INFO, "Bot %d won!", winning_bot + 1);
  }

  nob_sb_free(sb);
  return true;
}
