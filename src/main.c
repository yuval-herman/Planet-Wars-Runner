#include "tournament.h"
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

void Usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS] [--] [ARGS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

typedef struct {
  bool *help;
  bool *write_log;
  char **config_file;
  char **map_file;
  char **bot_commands;
  int bot_commands_count;
} CLIArguments;

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

typedef struct {
  char *name;
  char *start_command;
} Bot;

typedef struct {
  Bot *items;
  size_t count;
  size_t capacity;
} BotsDA;

DefineComplexStruct(ConfigArgs, {
  bool write_log;
  char *map_file;
  BotsDA bots;
});

ConfigArgs DeepCopyConfigArgs(ConfigArgs config) {
  NOB_UNREACHABLE(
      "You shouldn't try to copy Config args. If you really need to feel fre "
      "to implement this, but your'e probably doing something wrong.");
}

void FreeInnerConfigArgs(ConfigArgs config) {
  free(config.map_file);
  nob_da_foreach(Bot, bot, &config.bots) {
    free(bot->name);
    free(bot->start_command);
  }
  nob_da_free(config.bots);
}

int handler(void *user_data, const char *section, const char *name,
            const char *value, int lineno) {
  /* In some places in this function we cast away the const on `value`. This
   const removal is okay, see ini.h file in the comment above INI_HANDLER_LINENO
  */

#define MATCH(l, r) strcmp(l, r) == 0
  ConfigArgs *config = (ConfigArgs *)user_data;

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

int main(int argc, char *argv[]) {
  CLIArguments args = RegisterFlagArguments();
  ConfigArgs config_args = {0};
  if (!flag_parse(argc, argv)) {
    Usage(stderr);
    flag_print_error(stderr);
    exit(1);
  } else if (*args.help) {
    Usage(stderr);
    return 0;
  } else if (*args.config_file) {
    if (ini_parse(*args.config_file, handler, &config_args) < 0) {
      nob_log(NOB_ERROR, "Failed parsing config file");
      return 1;
    }
    if (config_args.bots.count == 0 && args.bot_commands_count == 0) {
      nob_log(
          NOB_ERROR,
          "Bots must be provided through the config file or CLI arguments.");
      return 1;
    }
    nob_da_foreach(Bot, bot, &config_args.bots) {
      if (bot->start_command == NULL) {
        nob_log(NOB_ERROR,
                "A bot was provided %s without a command in the config "
                "file. Please supply a command, name is potional.",
                bot->name == NULL
                    ? ""
                    : nob_temp_sprintf("with the name %s", bot->name));
        return 1;
      }
    }
  }

  if ((*args.config_file && !config_args.map_file) || *args.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    return 1;
  }

  GameState state;
  if (!*args.config_file) {

    args.bot_commands = flag_rest_argv();
    args.bot_commands_count = flag_rest_argc();
    state = MakeGame(*args.map_file, (const char *const *)(args.bot_commands),
                     args.bot_commands_count, args.write_log);
  } else {
    // TODO use the Bot struct all over the game to use name and other possible
    // future metadata where necessary.
    Nob_Cmd bot_commands = {0};
    nob_da_foreach(Bot, bot, &config_args.bots) {
      nob_cmd_append(&bot_commands, bot->start_command);
    }

    state = MakeGame(config_args.map_file, bot_commands.items,
                     bot_commands.count, args.write_log);

    nob_cmd_free(bot_commands);
    FreeInnerConfigArgs(config_args);
  }

  RunGame(&state);

  RunViewerForGame(state);

  FreeInnerGameState(state);
  return 0;
}
