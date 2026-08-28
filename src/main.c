#ifndef HEADLESS_MODE
#include "ui/menu.h"
#include "ui/ui.h"
#include "ui/viewer.h"
#endif // HEADLESS_MODE

#include "configs.h"
#include "game.h"
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

  if (nob_da_last(&configs.map_file) != '\0') {
    nob_sb_append_null(&configs.map_file);
  }
  if (exit_code == 0 && configs.mode == MODE_TOURNAMENT) {
    TournametData tournament = {0};
    RunTournament(&tournament, configs.map_file.items, configs.players);
    FreeInnerTournametData(tournament);
  } else if (exit_code == 0 && configs.mode == MODE_SINGLE_MATCH) {
    GameState state = {0};
    GameLog game_log = {0};
    if (MakeGame(&state, configs.map_file.items, configs.players.count)) {
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
      SetGameLog(game_log);
      UIInit(SCREEN_VIEWER);
      UIRun();
      UIDestroy();
#endif // HEADLESS_MODE
    } else {
      exit_code = 1;
    }

    FreeInnerGameState(state);
    FreeInnerGameLog(game_log);
  }
#ifndef HEADLESS_MODE
  else if (exit_code == 0 && configs.mode == MODE_REPLAY) {
    if (nob_da_last(&configs.save_file) != '\0') {
      nob_sb_append_null(&configs.save_file);
    }
    GameLog game_log = {0};
    FILE *save_file = fopen(configs.save_file.items, "rb");
    if (!save_file) {
      nob_log(NOB_ERROR, "Failed opening save file \"%s\": %s.",
              configs.save_file.items, strerror(errno));
      exit_code = 1;
    } else {
      if (!ReadGameLogFromFile(save_file, &game_log)) {
        nob_log(NOB_ERROR, "Failed reading \"%s\".", configs.save_file.items);
      } else {
        SetGameLog(game_log);
        UIInit(SCREEN_VIEWER);
        UIRun();
        UIDestroy();
      }
      FreeInnerGameLog(game_log);
      fclose(save_file);
    }
  } else if (exit_code == 0 && configs.mode == MODE_MENU) {
    SetConfig(&configs);
    UIInit(SCREEN_MENU);
    UIRun();
    UIDestroy();
  }
#endif // HEADLESS_MODE
  FreeConfigs(configs);
  return exit_code;
}
