#ifndef HEADLESS_MODE
#include "viewer.h"
#endif // HEADLESS_MODE

#include "configs.h"
#include "game.h"
#include "miniz.c"
#include "tournament.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

BotsDA MakeBotsDA(char **names, char **commands, unsigned count) {
  BotsDA bots = {
      .capacity = count,
      .count = count,
      .items = malloc(sizeof *bots.items * count),
  };
  for (unsigned i = 0; i < bots.count; i++) {
    // TODO should I dupe this?
    bots.items[i].name = names[i];
    bots.items[i].start_command = commands[i];
    bots.items[i].process = NULL;
  }
  return bots;
}

int main(int argc, char *argv[]) {
  Configs configs = MakeDefaultConfig();
  if (!ParseConfigsFromCLI(&configs, argc, argv)) {
    return 1;
  }

  if (configs.mode == MODE_REPLAY) {
    GameLog game_log = {0};
    FILE *save_file = fopen(configs.save_file, "rb");
    if (!save_file) {
      nob_log(NOB_ERROR, "Failed opening save file: %s", strerror(errno));
      return 1;
    }
    ReadGameLogFromFile(save_file, &game_log);
    RunViewerForGame(game_log);
    FreeInnerGameLog(game_log);

    fclose(save_file);
    return 0;
  } else if (configs.mode == MODE_TOURNAMENT) {
    BotsDA bots = MakeBotsDA(configs.bot_names, configs.bot_start_commands,
                             configs.bot_count);
    TournametData tournament = RunTournament(configs.map_file, bots);
    FreeInnerTournametData(tournament);
    FreeInnerBotsDA(bots);
  } else if (configs.mode == MODE_SINGLE_MATCH) {
    BotsDA bots = MakeBotsDA(configs.bot_names, configs.bot_start_commands,
                             configs.bot_count);
    GameState state = MakeGame(configs.map_file, bots);

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
    FreeInnerBotsDA(bots);
  }
  return 0;
}
