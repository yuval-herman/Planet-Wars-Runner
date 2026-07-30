#include "tournament.h"
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
