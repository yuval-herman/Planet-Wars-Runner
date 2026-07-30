#include "viewer.h"

#include "game.h"
#include "planet_wars.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  GameLog *items;
  size_t capacity;
  size_t count;
} TournametData;

TournametData tournament = {0};

void RunTournament(const char *map_file_path,
                   const char *const bot_start_commands[], int bot_count) {
  char const *playing_bot_commands[2];
  for (int p1_idx = 0; p1_idx < bot_count - 1; p1_idx++) {
    for (int p2_idx = p1_idx + 1; p2_idx < bot_count; p2_idx++) {
      playing_bot_commands[0] = bot_start_commands[p1_idx];
      playing_bot_commands[1] = bot_start_commands[p2_idx];
      GameState state = MakeGame(map_file_path, playing_bot_commands, 2, false);
      nob_minimal_log_level = NOB_WARNING;
      RunGame(&state);
      nob_minimal_log_level = NOB_INFO;
      GameLog game_log_copy = state.game_log;
      size_t bot_commands_length = 0;
      for (int i = 0; i < game_log_copy.bot_amount; i++) {
        // +1 for null terminator
        bot_commands_length += strlen(state.game_log.bot_commands[i]) + 1;
      }
      size_t bot_commands_array_length =
          game_log_copy.bot_amount * sizeof(char *) +
          bot_commands_length * sizeof(char);
      game_log_copy.bot_commands = malloc(bot_commands_array_length);
      memcpy(game_log_copy.bot_commands, state.game_log.bot_commands,
             bot_commands_array_length);

      char *str_dest =
          (char *)(game_log_copy.bot_commands + game_log_copy.bot_amount);
      for (int i = 0; i < game_log_copy.bot_amount; i++) {
        game_log_copy.bot_commands[i] = str_dest;
        str_dest += strlen(str_dest) + 1;
      }

      game_log_copy.items =
          malloc(sizeof *game_log_copy.items * game_log_copy.count);
      game_log_copy.capacity = game_log_copy.count;
      memcpy(game_log_copy.items, state.game_log.items,
             game_log_copy.count * sizeof *game_log_copy.items);
      for (size_t i = 0; i < state.game_log.count; i++) {
        LogEntry *copy_entry = &game_log_copy.items[i];
        size_t planets_size =
            copy_entry->planet_count * sizeof *copy_entry->planets;
        copy_entry->planets = malloc(planets_size);
        memcpy(copy_entry->planets, state.game_log.items[i].planets,
               planets_size);
        size_t fleets_size =
            copy_entry->fleet_count * sizeof *copy_entry->fleets;
        copy_entry->fleets = malloc(fleets_size);
        memcpy(copy_entry->fleets, state.game_log.items[i].fleets, fleets_size);
      }
      // TODO This entire thing is leaking, add a freeing mechanism

      nob_da_append(&tournament, game_log_copy);
      FreeInnerGameState(state);
    }
  }
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
    RunTournament(*args.map_file, (const char *const *)(args.bot_commands),
                  args.bot_commands_count);
    nob_da_free(tournament);
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
