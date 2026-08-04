#ifndef HEADLESS_MODE
#include "viewer.h"
#endif // HEADLESS_MODE

#include "game.h"
#include "ini.h"
#include "src/tournament.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  bool *help;
  bool *write_log;
  bool *write_save;
  char **config_file;
  char **map_file;
#ifndef HEADLESS_MODE
  char **from_save_file;
#endif // HEADLESS_MODE
  char **bot_commands;
  int bot_commands_count;
} CLIArguments;

// Configuration for the entire system.
DefineComplexStruct(Configs, {
  BotsDA bots;
  char *map_file;
  bool tournament;
  bool write_log;
  bool write_save;
});

void Usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS] [--] [ARGS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

CLIArguments RegisterFlagArguments() {
  CLIArguments args;
  args.help = flag_bool("help", false, "Show this help.");
  args.write_log =
      flag_bool("write_log", false, "Write a log of the game to log.txt");
  args.write_save = flag_bool("write_save", false,
                              "Write a save file of the game to game.plws");
  args.config_file =
      flag_str("config", NULL,
               "A path to a config file that can be used to run "
               "tournament or more complex environments. If a config file is "
               "used all other flags are ignored.");
  args.map_file = flag_str("map", NULL, "The map file bots will play on.");
#ifndef HEADLESS_MODE
  args.from_save_file =
      flag_str("load_from", NULL,
               "A path to a .plws file to read a game from. If this option is "
               "given, the rest of the options are ignored.");
#endif // HEADLESS_MODE
  args.bot_commands_count = 0;
  args.bot_commands = NULL;
  return args;
}

Configs DeepCopyConfigs(Configs config) {
  NOB_UNUSED(config);
  NOB_UNREACHABLE(
      "You shouldn't try to copy configs. If you really need to feel free "
      "to implement this, but your'e probably doing something wrong.");
}

void FreeInnerConfigs(Configs config) {
  free(config.map_file);
  FreeInnerBotsDA(config.bots);
}

int handler(void *user_data, const char *section, const char *name,
            const char *value, int lineno) {
  /* In some places in this function we cast away the const on `value`. This
   const removal is okay, see ini.h file in the comment above INI_HANDLER_LINENO
  */

#define MATCH(l, r) strcmp(l, r) == 0
#define SET_BOOL(var)                                                          \
  int p_bool = ParseBool(value);                                               \
  if (p_bool < 0) {                                                            \
    nob_log(NOB_ERROR, #var " may only be set to `true` or `false`. line %d.", \
            lineno);                                                           \
    return 0;                                                                  \
  }                                                                            \
  var = p_bool;

  Configs *config = (Configs *)user_data;

  if (name == NULL && value == NULL) {
    // If we start parsing a new bot, reserve it's place
    if (MATCH(section, "bot")) {
      nob_da_append(&config->bots, (Bot){0});
    }
    return 1;
  }

  if (MATCH(section, "application")) {

    if (MATCH(name, "write_log")) {
      SET_BOOL(config->write_log);
    }
    if (MATCH(name, "write_save")) {
      SET_BOOL(config->write_save);
    }
    if (MATCH(name, "tournament")) {
      SET_BOOL(config->tournament);
    }

  } else if (MATCH(section, "simulation")) {

    if (MATCH(name, "map")) {
      config->map_file = DupeString(value);
    } else {
      nob_log(NOB_ERROR, "Unknown simulation config. line %d.", lineno);
      return 0;
    }

  } else if (MATCH(section, "bot")) {

    if (MATCH(name, "name")) {
      nob_da_last(&config->bots).name = DupeString(value);
    } else if (MATCH(name, "command")) {
      nob_da_last(&config->bots).start_command = DupeString(value);
    } else {
      nob_log(NOB_ERROR, "Unknown bot config. line %d.", lineno);
      return 0;
    }

  } else {
    nob_log(NOB_ERROR, "Unknown section. line %d.", lineno);
    return 0;
  }
  return 1;
#undef MATCH
}

int VerifyConfigs(Configs configs) {
  if (configs.bots.count == 0) {
    nob_log(NOB_ERROR, "No bots provided.");
    return 1;
  }
  size_t mark = nob_temp_save();
  nob_da_foreach(Bot, bot, &configs.bots) {
    if (bot->start_command == NULL) {
      nob_log(NOB_ERROR,
              "A bot was provided%s without a command. Please supply a "
              "command, name is potional.",
              bot->name == NULL
                  ? ""
                  : nob_temp_sprintf(" with the name %s but", bot->name));
      return 1;
    }
  }
  nob_temp_rewind(mark);

  if (!configs.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    return 1;
  }
  return 0;
}

bool FillConfigsWithArgs(CLIArguments args, Configs *configs) {
  if (!*args.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    Usage(stderr);
    return false;
  }
  configs->write_log = *args.write_log;
  configs->map_file = DupeString(*args.map_file);
  configs->write_save = *args.write_save;

  char *const *argv = flag_rest_argv();
  const int argc = flag_rest_argc();
  for (int i = 0; i < argc; i++) {
    Bot bot = {0};
    bot.start_command = DupeString(argv[i]);
    nob_da_append(&configs->bots, bot);
  }

  return true;
}

int main(int argc, char *argv[]) {
  Configs configs = {0};
  CLIArguments args = RegisterFlagArguments();
  if (!flag_parse(argc, argv)) {
    Usage(stderr);
    flag_print_error(stderr);
    return 1;
  } else if (*args.help) {
    Usage(stderr);
    return 0;
  }
#ifndef HEADLESS_MODE
  else if (*args.from_save_file != NULL) {
    GameLog game_log = {0};

    FILE *file = fopen(*args.from_save_file, "rb");
    if (!file) {
      perror("Failed loading save file");
      exit(1);
    }

    if (!ReadGameLogFromFile(file, &game_log))
      return 1;
    fclose(file);
    RunViewerForGame(game_log);
    FreeInnerGameLog(game_log);
    return 0;
  }
#endif // HEADLESS_MODE
  else if (*args.config_file) {
    if (ini_parse(*args.config_file, handler, &configs) < 0) {
      nob_log(NOB_ERROR, "Failed parsing config file");
      return 1;
    }
  } else {
    if (!FillConfigsWithArgs(args, &configs))
      return 1;
  }

  if (configs.tournament) {
    TournametData tournament = RunTournament(configs.map_file, configs.bots);
    FreeInnerTournametData(tournament);
  } else {
    GameState state =
        MakeGame(configs.map_file, configs.bots, configs.write_log);

    RunGame(&state);

    if (configs.write_save) {
      FILE *file = fopen("game.plws", "wb");
      WriteGameLogToFile(file, state.game_log);
      fclose(file);
    }

#ifndef HEADLESS_MODE
    RunViewerForGame(state.game_log);
#endif // HEADLESS_MODE

    FreeInnerGameState(state);
  }
  FreeInnerConfigs(configs);
  return 0;
}
