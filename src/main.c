#ifndef HEADLESS_MODE
#include "ui/menu.h"
#include "ui/ui.h"
#include "ui/viewer.h"
#endif // HEADLESS_MODE

#include <lauxlib.h>
#include <lualib.h>

#include "configs.h"
#include "game.h"
#include "runner.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

lua_State *L;

int main(int argc, char *argv[]) {
  /* Create a Lua state */
  L = luaL_newstate();

  /* Check the return value */
  if (L == NULL) {
    fprintf(stderr, "Lua: cannot initialize\n");
    return -1;
  }

  /* Provide the Lua standard libraries to the Lua state */
  luaL_openlibs(L);

  /* Execute a Lua program in script.lua */
  luaL_dofile(L, "script.lua");

  /* Close the Lua state */
  lua_close(L);
  printf("I am a test!\n");

  return 0;
  Configs configs = MakeDefaultConfig();
  int exit_code = 0;
  if (!ParseConfigsFromCLI(&configs, argc, argv)) {
    exit_code = 1;
  }

  if (exit_code == 0 && configs.mode == MODE_TOURNAMENT) {
    TournametData tournament = {0};
    EnsureNullTerminated(&configs.map_file);
    RunTournament(&tournament, configs.map_file.items, configs.players);
    FreeInnerTournametData(tournament);
  } else if (exit_code == 0 && configs.mode == MODE_SINGLE_MATCH) {
    GameState state = {0};
    GameLog game_log = {0};
    EnsureNullTerminated(&configs.map_file);
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
    GameLog game_log = {0};
    EnsureNullTerminated(&configs.save_file);
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
