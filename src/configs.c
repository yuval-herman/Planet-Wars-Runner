#include "configs.h"
#include "nob.h"
#include "utils.h"

#define INI_ALLOW_MULTILINE 0
#define INI_STOP_ON_FIRST_ERROR 1
#define INI_HANDLER_LINENO 1
#define INI_CALL_HANDLER_ON_NEW_SECTION 1
#define INI_MAX_LINE 1000
#include "ini.c"

#define FLAG_IMPLEMENTATION
#include "flag.h"

Configs MakeDefaultConfig() {
  Configs configs = {0};
  configs.mode = MODE_MENU;
  configs.write_save = true;
  nob_sb_append_cstr(&configs.save_file, "game.plws");
  return configs;
}

void FreeConfigs(Configs configs) {
  nob_sb_free(configs.map_file);
  nob_sb_free(configs.save_file);
  FreeInnerPlayerDA(configs.players);
}

static bool VerifyConfigs(Configs configs) {
#define REPORT_ERROR(msg)                                                      \
  nob_log(NOB_ERROR, "Configs error, pass -help to see usage.\n" msg)

  if (configs.mode == MODE_REPLAY) {
    if (configs.save_file.count == 0) {
      REPORT_ERROR("No file to replay was provided.");
      return false;
    }
    return true;
  }

  if (configs.mode == MODE_MENU) {
    return true;
  }

  if (configs.mode == MODE_TOURNAMENT) {
    if (configs.players.count < 3) {
      REPORT_ERROR("Tournament mode requires at least 3 players.");
      return false;
    }
  }

  if (configs.players.count == 0) {
    REPORT_ERROR("No players provided.");
    return false;
  }

  if (!configs.map_file.count) {
    REPORT_ERROR("No map file provided.");
    return false;
  }
  return true;
}

static int ini_parse_handler(void *user_data, const char *section,
                             const char *name, const char *value, int lineno) {
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

  Configs *configs = user_data;

  if (name == NULL && value == NULL) {
    // If we start parsing a new bot, reserve it's place
    if (MATCH(section, "bot")) {
      nob_da_append(&configs->players, (Player){0});
    }
    return 1;
  }

  if (MATCH(section, "application")) {

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
      nob_sb_append_cstr(&configs->map_file, value);
    } else {
      nob_log(NOB_ERROR, "Unknown simulation config. line %d.", lineno);
      return 0;
    }

  } else if (MATCH(section, "bot")) {

    if (MATCH(name, "name")) {
      nob_sb_append_cstr(&nob_da_last(&configs->players).name, value);
    } else if (MATCH(name, "command")) {
      nob_sb_append_cstr(&nob_da_last(&configs->players).as.bot.start_command,
                         value);
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
  if (ini_parse_file(ini_file, ini_parse_handler, configs) < 0) {
    nob_log(NOB_ERROR, "Failed parsing config file");
    return false;
  }

  return true;
}

static void Usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

bool ParseConfigsFromCLI(Configs *configs, int argc, char *argv[]) {
  bool help;
  flag_bool_var(&help, "help", false, "Show this help.");
  char *config_file;
  flag_str_var(&config_file, "config", NULL,
               "A path to a config file that can be used to run "
               "tournament or more complex environments. If a config file is "
               "used all other flags are ignored.");

  Flag_List_Mut bot_names = {0};
  flag_list_mut_var(
      &bot_names, "name",
      "List of bot names. Each -name has a corresponding -command, and entries "
      "are paired by position: the first name with the first command, the "
      "second with the second, regardless of where the flags appear on the "
      "command line.");

  Flag_List_Mut bot_commands = {0};
  flag_list_mut_var(
      &bot_commands, "command",
      "List of bot commands. Each -command has a corresponding -name, and "
      "entries are paired by position: the first command with the first name, "
      "the second with the second, regardless of where the flags appear on the "
      "command line.");

  char *mode;
  flag_str_var(
      &mode, "mode", NULL,
      "Which mode should the software launch at. Options are:\n"
      "single - Run a single match between all the supplied bots.\n"
      "tournament - Run a round robin tournament between all the supplied "
      "bots.\n"
#ifndef HEADLESS_MODE
      "replay - Replay a previously save plws file. This requires that the "
      "`save_file` option also be specified with the file to play.\n"
#endif // HEADLESS_MODE
  );

  char *map_file;
  flag_str_var(&map_file, "map", NULL, "The map file bots will play on.");

#ifndef HEADLESS_MODE
  char *save_file;
  flag_str_var(&save_file, "save_file", NULL,
               "A path to a .plws file to save the game to in match mode, or "
               "read a game from in replay mode.");
#endif // HEADLESS_MODE

  if (!flag_parse(argc, argv)) {
    Usage(stderr);
    flag_print_error(stderr);
    return false;
  } else if (help) {
    configs->mode = MODE_NULL;
    Usage(stderr);
    return true;
  }

  if (mode) {
    if (strcmp(mode, "single") == 0) {
      configs->mode = MODE_SINGLE_MATCH;
    } else if (strcmp(mode, "tournament") == 0) {
      configs->mode = MODE_TOURNAMENT;
    }
#ifndef HEADLESS_MODE
    else if (strcmp(mode, "replay") == 0) {
      configs->mode = MODE_REPLAY;
    }
#endif // HEADLESS_MODE
    else {
      nob_log(NOB_ERROR, "Unsupported mode passed. Try the -help flag to see a "
                         "list of supported modes.");
      return false;
    }
  }

  // We call free on this vairables later, so it's easier to just make sure they
  // are heap allocated then try and try whether they came from the CLI (stack)
  // or the config file (heap)
  if (map_file) {
    nob_sb_append_cstr(&configs->map_file, map_file);
  }
#ifndef HEADLESS_MODE
  if (save_file) {
    configs->save_file.count = 0;
    nob_sb_append_cstr(&configs->save_file, save_file);
  }
#endif // HEADLESS_MODE

  if (bot_names.count != bot_commands.count) {
    nob_log(NOB_ERROR,
            "The amount of -name and -command arguments is not equal.");
    nob_log(NOB_INFO, "You should pass bot names and commands like so:");
    nob_log(NOB_INFO,
            "%s -name \"bot 1 name\" -command \"bot 1 command\" -name \"bot 2 "
            "name\" "
            "-command \"bot 2 command\" ...",
            flag_program_name());
    return false;
  }

  for (unsigned i = 0; i < bot_names.count; i++) {
    Player player = {
        .type = PLAYER_BOT,
        .name = nob_sb_from_cstr(bot_names.items[i]),
        .as.bot = {
            .start_command = nob_sb_from_cstr(bot_commands.items[i]),
            .process = NULL,
        }};

    nob_da_append(&configs->players, player);
  }

  free(bot_names.items);
  free(bot_commands.items);

  if (config_file) {
    FILE *ini_file = fopen(config_file, "r");
    if (!ini_file) {
      nob_log(NOB_ERROR, "Could not open config file: %s", strerror(errno));
      return false;
    }
    ParseConfigsFromIni(configs, ini_file);
    fclose(ini_file);
  }
  return VerifyConfigs(*configs);
}
