#include "components.h"

#include "../runner.h"
#include "human_game.h"
#include "stars_shader.h"

static GameState game_state = {0};
static GameSpace game_space = {0};
static PlayerDA players = {0};
static Nob_String_Builder sb = {0};
// 0 is realtime, higher is slower.
static unsigned short game_speed = 10;
static unsigned short bot_speed = 150;

void SetGameState(GameState state) {
  FreeInnerGameState(game_state);
  game_state = DeepCopyGameState(state);
  game_space =
      ComputeGameSpace(game_state.planets.items, game_state.planets.count);
}

void SetPlayers(PlayerDA new_players) {
  FreeInnerPlayerDA(players);
  players = DeepCopyPlayerDA(new_players);
}

static void HumanGameInit() {
  assert(game_state.planets.items != NULL &&
         "Planets array is null. Did you forgot to set game state?");

  sb = (Nob_String_Builder){0};

  StarsShaderInit((StarsShaderConfig){
      .size = 0.5,
      .brightness = 0.4,
      .density = 0.5,
      .time_scale = 1,
      .seed = 1,
  });

  nob_da_foreach(Player, player, &players) {
    if (!StartPlayer(player))
      NOB_TODO("Can't handle errors yet");
  }

  Clay_SetDebugModeEnabled(true);
}

static void HumanGameDraw() {
  if (bot_speed == 0 || GetFrame() % bot_speed == 0)
    RunPlayerCycle(&game_state, players, &sb);

  if (game_speed == 0 || GetFrame() % game_speed == 0)
    AdvanceTurn(&game_state);

  // ====== DRAWING =======

  StarsShaderDraw((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});
  CLAY(CLAY_ID("OuterContainer"),
       {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .padding = CLAY_PADDING_ALL(10),
                   .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                      .y = CLAY_ALIGN_Y_CENTER},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = 32}}) {
    Component_GameFrame(game_space, game_state.planets.items,
                        game_state.planets.count, game_state.fleets.items,
                        game_state.fleets.count);
  }
}

static void HumanGameDestroy() {
  FreeInnerGameState(game_state);
  game_state = (GameState){0};
  StarsShaderDestroy();
  nob_sb_free(sb);
  sb = (Nob_String_Builder){0};
}

const UIScreen human_game_screen = {
    .init = &HumanGameInit,
    .draw = &HumanGameDraw,
    .destroy = &HumanGameDestroy,
};
