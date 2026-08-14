#include "configs.h"
#include "bot.h"
#include "nob.h"

#define INI_ALLOW_MULTILINE 0
#define INI_STOP_ON_FIRST_ERROR 1
#define INI_HANDLER_LINENO 1
#define INI_CALL_HANDLER_ON_NEW_SECTION 1
#define INI_MAX_LINE 1000
#include "ini.c"

#define FLAG_IMPLEMENTATION
#include "flag.h"

// A structure to hold some extra data that helps while parsing the config file
struct ConfigsMeta {
  Configs *configs;
  BotsDA bots_da;
};

Configs MakeDefaultConfig() {
  Configs configs = {0};
  configs.mode = MODE_SINGLE_MATCH;
  configs.write_save = true;
  return configs;
}

static bool VerifyConfigs(Configs configs) {
  if (configs.mode == MODE_REPLAY) {
    return configs.save_file != NULL;
  } else {
    if (configs.bot_count == 0) {
      nob_log(NOB_ERROR, "No bots provided.");
      return false;
    }
    size_t mark = nob_temp_save();
    for (unsigned i = 0; i < configs.bot_count; i++) {
      if (configs.bot_start_commands[i] == NULL) {
        nob_log(NOB_ERROR,
                "A bot was provided%s without a command. Please supply a "
                "command, name is potional.",
                configs.bot_names[i] == NULL
                    ? ""
                    : nob_temp_sprintf(" with the name %s but",
                                       configs.bot_names[i]));
        return false;
      }
    }
    nob_temp_rewind(mark);

    if (!configs.map_file) {
      nob_log(NOB_ERROR, "A map file must be provided.");
      return false;
    }
    return true;
  }
}

static int handler(void *user_data, const char *section, const char *name,
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

  struct ConfigsMeta *configs_meta = (struct ConfigsMeta *)user_data;
  Configs *configs = configs_meta->configs;

  if (name == NULL && value == NULL) {
    // If we start parsing a new bot, reserve it's place
    if (MATCH(section, "bot")) {
      nob_da_append(&configs_meta->bots_da, (Bot){0});
    }
    return 1;
  }

  if (MATCH(section, "application")) {

    if (MATCH(name, "write_log")) {
      SET_BOOL(configs->write_log);
    }
    if (MATCH(name, "write_save")) {
      SET_BOOL(configs->write_save);
    }
    if (MATCH(name, "tournament")) {
      bool tournament;
      SET_BOOL(tournament);
      if (tournament)
        configs->mode = MODE_TOURNAMENT;
    }

  } else if (MATCH(section, "simulation")) {

    if (MATCH(name, "map")) {
      configs->map_file = DupeString(value);
    } else {
      nob_log(NOB_ERROR, "Unknown simulation config. line %d.", lineno);
      return 0;
    }

  } else if (MATCH(section, "bot")) {

    if (MATCH(name, "name")) {
      nob_da_last(&configs_meta->bots_da).name = DupeString(value);
    } else if (MATCH(name, "command")) {
      nob_da_last(&configs_meta->bots_da).start_command = DupeString(value);
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

bool ParseConfigsFromIni(Configs *configs, FILE *ini_file) {
  struct ConfigsMeta configs_meta = {.configs = configs, .bots_da = {0}};
  if (ini_parse_file(ini_file, handler, &configs_meta) < 0) {
    nob_log(NOB_ERROR, "Failed parsing config file");
    return false;
  }
  configs->bot_count = configs_meta.bots_da.count;
  configs->bot_names =
      malloc(sizeof *configs->bot_names * configs_meta.bots_da.count);
  configs->bot_start_commands =
      malloc(sizeof *configs->bot_names * configs_meta.bots_da.count);

  for (unsigned i = 0; i < configs_meta.bots_da.count; i++) {
    configs->bot_names[i] = configs_meta.bots_da.items[i].name;
    configs->bot_start_commands[i] =
        configs_meta.bots_da.items[i].start_command;
  }
  return VerifyConfigs(*configs);
}

typedef struct {
  bool *help;
  bool *write_log;
  char **config_file;
  char **map_file;
#ifndef HEADLESS_MODE
  char **from_save_file;
#endif // HEADLESS_MODE
  char **bot_commands;
  int bot_commands_count;
} CLIArguments;

static CLIArguments RegisterFlagArguments() {
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

static void Usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS] [--] [BOTS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

static bool FillConfigsWithArgs(CLIArguments args, Configs *configs) {
  if (!*args.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    Usage(stderr);
    return false;
  }
  configs->write_log = *args.write_log;
  configs->map_file = DupeString(*args.map_file);

  char *const *argv = flag_rest_argv();
  const int argc = flag_rest_argc();

  configs->bot_count = argc;
  // Initialize the array to null. This is important so the game knows bot's
  // have no name set.
  configs->bot_names = calloc(argc, sizeof *configs->bot_names);
  configs->bot_start_commands =
      malloc(argc * sizeof *configs->bot_start_commands);

  for (int i = 0; i < argc; i++) {
    configs->bot_start_commands[i] = DupeString(argv[i]);
  }

  return true;
}

bool ParseConfigsFromCLI(Configs *configs, int argc, char *argv[]) {
  CLIArguments args = RegisterFlagArguments();
  if (!flag_parse(argc, argv)) {
    Usage(stderr);
    flag_print_error(stderr);
    return false;
  } else if (*args.help) {
    configs->mode = MODE_NULL;
    Usage(stderr);
    return true;
  }
#ifndef HEADLESS_MODE
  else if (*args.from_save_file != NULL) {
    configs->save_file = *args.from_save_file;
    configs->mode = MODE_REPLAY;
  }
#endif // HEADLESS_MODE

  else if (*args.config_file) {
    FILE *ini_file = fopen(*args.config_file, "r");
    if (!ini_file) {
      nob_log(NOB_ERROR, "Could not open config file: %s", strerror(errno));
      return false;
    }
    ParseConfigsFromIni(configs, ini_file);
    fclose(ini_file);
  } else if (!FillConfigsWithArgs(args, configs))
    return false;
  return VerifyConfigs(*configs);
}
