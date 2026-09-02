#include "components.h"

#include "human_game.h"
#include "stars_shader.h"

static GameState game_state = {0};
static GameSpace game_space = {0};

void SetGameState(GameState state) {
  game_state = DeepCopyGameState(state);
  game_space =
      ComputeGameSpace(game_state.planets.items, game_state.planets.count);
  printf("max x = %g\n",game_space.max_coords.x);
  printf("max y = %g\n",game_space.max_coords.y);
  printf("min x = %g\n",game_space.min_coords.x);
  printf("min y = %g\n",game_space.min_coords.y);
}

static void HumanGameInit() {
  assert(game_state.planets.items != NULL &&
         "Planets array is null. Did you forgot to set game state?");

  StarsShaderInit((StarsShaderConfig){
      .size = 0.5,
      .brightness = 0.4,
      .density = 0.5,
      .time_scale = 1,
      .seed = 1,
  });

  Clay_SetDebugModeEnabled(true);
}

static void HumanGameDraw() {
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
}

const UIScreen human_game_screen = {
    .init = &HumanGameInit,
    .draw = &HumanGameDraw,
    .destroy = &HumanGameDestroy,
};
