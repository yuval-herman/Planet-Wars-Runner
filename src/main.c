#ifndef HEADLESS_MODE
#include "viewer.h"
#endif // HEADLESS_MODE

#include "configs.h"
#include "game.h"
#include "miniz.c"
#include "runner.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
  Configs configs = MakeDefaultConfig();
  int exit_code = 0;
  if (!ParseConfigsFromCLI(&configs, argc, argv)) {
    exit_code = 1;
  }

  if (exit_code == 0 && configs.mode == MODE_REPLAY) {
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
  } else if (exit_code == 0 && configs.mode == MODE_TOURNAMENT) {
    TournametData tournament = RunTournament(configs.map_file, configs.players);
    FreeInnerTournametData(tournament);
  } else if (exit_code == 0 && configs.mode == MODE_SINGLE_MATCH) {
    GameState state = {0};
    GameLog game_log = {0};
    if (MakeGame(&state, configs.map_file, configs.players.count)) {
      if (!RunMatch(&game_log, &state, configs.players)) {
        nob_log(NOB_ERROR, "Failed running match.");
        exit_code = 1;
      }
      if (configs.write_save) {
        FILE *file = fopen("game.plws", "wb");
        WriteGameLogToFile(file, game_log);
        fclose(file);
      }

#ifndef HEADLESS_MODE
      RunViewerForGame(game_log);
#endif // HEADLESS_MODE
    } else {
      exit_code = 1;
    }

    FreeInnerGameState(state);
    FreeInnerGameLog(game_log);
  }
  FreeConfigs(configs);
  return 0;
}
