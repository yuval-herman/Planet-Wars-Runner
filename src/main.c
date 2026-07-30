#include "viewer.h"

#include "game.h"
#include <src/utils.h>

#define FLAG_IMPLEMENTATION
#include "flag.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

DefineComplexStruct(TournametData, {
  GameLog *items;
  size_t capacity;
  size_t count;
});

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

      // Run a full game, slilence normal logging to not clog the terminal
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

void Usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS] [--] [ARGS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

typedef struct {
  bool *help;
  bool *tournament;
  bool *write_log;
  char **map_file;
  char **bot_commands;
  int bot_commands_count;
} CLIArguments;

CLIArguments RegisterFlagArguments() {
  CLIArguments args;
  args.help = flag_bool("help", false, "Show this help.");
  args.tournament =
      flag_bool("tournament", false,
                "Activate tournament mode. Tournament mode runs a round-robin "
                "tournamet between all bots and ranks them based on winnings.");
  args.write_log =
      flag_bool("write_log", false, "Write a log of the game to log.txt");
  args.map_file = flag_str("map", NULL, "The map file bots will play on.");
  args.bot_commands_count = 0;
  args.bot_commands = NULL;
  return args;
}

int main(int argc, char *argv[]) {
  CLIArguments args = RegisterFlagArguments();
  if (!flag_parse(argc, argv)) {
    Usage(stderr);
    flag_print_error(stderr);
    exit(1);
  } else if (*args.help) {
    Usage(stderr);
    return 0;
  } else if (!*args.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    return 1;
  }

  args.bot_commands = flag_rest_argv();
  args.bot_commands_count = flag_rest_argc();

  if (*args.tournament) {
    TournametData tournament =
        RunTournament(*args.map_file, (const char *const *)(args.bot_commands),
                      args.bot_commands_count);

    FreeInnerTournametData(tournament);
    return 0;
  }

  GameState state =
      MakeGame(*args.map_file, (const char *const *)(args.bot_commands),
               args.bot_commands_count, true);
  RunGame(&state);

  RunViewerForGame(state);

  FreeInnerGameState(state);
  return 0;
}
