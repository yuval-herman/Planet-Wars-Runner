#include "viewer.h"

#include "game.h"
#include "ini.h"
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

typedef struct {
  bool *help;
  bool *write_log;
  char **config_file;
  char **map_file;
  char **bot_commands;
  int bot_commands_count;
} CLIArguments;

// Configuration for the entire system.
DefineComplexStruct(Configs, {
  bool write_log;
  char *map_file;
  BotsDA bots;
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
  args.config_file =
      flag_str("config", NULL,
               "A path to a config file that can be used to run "
               "tournament or more complex environments. If a config file is "
               "used all other flags are ignored.");
  args.map_file = flag_str("map", NULL, "The map file bots will play on.");
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
  nob_da_foreach(Bot, bot, &config.bots) { FreeInnerBot(*bot); }
  nob_da_free(config.bots);
}

int handler(void *user_data, const char *section, const char *name,
            const char *value, int lineno) {
  /* In some places in this function we cast away the const on `value`. This
   const removal is okay, see ini.h file in the comment above INI_HANDLER_LINENO
  */

#define MATCH(l, r) strcmp(l, r) == 0
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
      if (MATCH(value, "true")) {
        config->write_log = true;
      } else if (MATCH(value, "false")) {
        config->write_log = false;
      } else {
        nob_log(NOB_ERROR,
                "write_log may only be set to `true` or `false`. line %d.",
                lineno);
        return 0;
      }
    }

  } else if (MATCH(section, "simulation")) {

    if (MATCH(name, "map")) {
      config->map_file = strdup(value);
    } else {
      nob_log(NOB_ERROR, "Unknown simulation config. line %d.", lineno);
      return 0;
    }

  } else if (MATCH(section, "bot")) {

    if (MATCH(name, "name")) {
      nob_da_last(&config->bots).name = strdup(value);
    } else if (MATCH(name, "command")) {
      nob_da_last(&config->bots).start_command = strdup(value);
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

  if (!configs.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    return 1;
  }
  return 0;
}

bool FillConfigsWithArgs(CLIArguments args, Configs *configs) {
  if (!*args.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    return false;
  }
  configs->write_log = *args.write_log;
  configs->map_file = strdup(*args.map_file);

  char *const *argv = flag_rest_argv();
  const int argc = flag_rest_argc();
  for (int i = 0; i < argc; i++) {
    Bot bot = {0};
    bot.start_command = strdup(argv[i]);
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
  } else if (*args.config_file) {
    if (ini_parse(*args.config_file, handler, &configs) < 0) {
      nob_log(NOB_ERROR, "Failed parsing config file");
      return 1;
    }
  } else {
    if (!FillConfigsWithArgs(args, &configs))
      return 1;
  }

  GameState state = MakeGame(configs.map_file, configs.bots, configs.write_log);

  RunGame(&state);

  RunViewerForGame(state);

  FreeInnerGameState(state);
  FreeInnerConfigs(configs);
  return 0;
}
