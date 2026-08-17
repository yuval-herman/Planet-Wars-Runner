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
    TournametData tournament = RunTournament(configs.map_file, configs.players);
    FreeInnerTournametData(tournament);
  } else if (configs.mode == MODE_SINGLE_MATCH) {
    GameState state = MakeGame(configs.map_file, configs.players.count);

    GameLog game_log = RunMatch(&state, configs.players);

    if (configs.write_save) {
      FILE *file = fopen("game.plws", "wb");
      WriteGameLogToFile(file, game_log);
      fclose(file);
    }

#ifndef HEADLESS_MODE
    RunViewerForGame(game_log);
#endif // HEADLESS_MODE

    FreeInnerGameState(state);
  }
  return 0;
}
