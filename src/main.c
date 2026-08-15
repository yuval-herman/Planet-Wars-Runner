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

PlayerDA MakePlayerDA(char **names, char **commands, unsigned count) {
  PlayerDA players = {
      .capacity = count,
      .count = count,
      .items = malloc(sizeof *players.items * count),
  };
  for (unsigned i = 0; i < players.count; i++) {
    // TODO should I dupe this?
    players.items[i].name = names[i];
    players.items[i].type = PLAYER_BOT;
    players.items[i].as.bot.start_command = commands[i];
    players.items[i].as.bot.process = NULL;
  }
  return players;
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
    PlayerDA players = MakePlayerDA(
        configs.bot_names, configs.bot_start_commands, configs.bot_count);
    TournametData tournament = RunTournament(configs.map_file, players);
    FreeInnerTournametData(tournament);
    FreeInnerPlayerDA(players);
  } else if (configs.mode == MODE_SINGLE_MATCH) {
    PlayerDA players = MakePlayerDA(
        configs.bot_names, configs.bot_start_commands, configs.bot_count);
    GameState state = MakeGame(configs.map_file, players);

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
    FreeInnerPlayerDA(players);
  }
  return 0;
}
