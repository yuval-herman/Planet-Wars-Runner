#include "human_game.h"

static GameState game_state = {0};

void SetGameState(GameState state) { game_state = DeepCopyGameState(state); }

static void HumanGameInit() {assert(game_state.planets.items != NULL && "Planets array is null. Did you forgot to set game state?");}

static void HumanGameDraw() {NOB_TODO("working on draw");}

static void HumanGameDestroy() {
  FreeInnerGameState(game_state);
  game_state = (GameState){0};
}

const UIScreen human_game_screen = {
    .init = &HumanGameInit,
    .draw = &HumanGameDraw,
    .destroy = &HumanGameDestroy,
};
