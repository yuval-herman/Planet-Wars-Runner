#include "components.h"

#include "../runner.h"
#include "human_game.h"
#include "stars_shader.h"

static uint8_t player_id;
static GameState game_state = {0};
static GameSpace game_space = {0};
static PlayerDA players = {0};
static Nob_String_Builder sb = {0};
// 0 is realtime, higher is slower.
static unsigned short game_speed = 10;
static unsigned short bot_speed = 150;

struct {
  unsigned *items;
  unsigned capacity;
  unsigned count;
} selected_planets_ids = {0};

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

  sb.count = 0;
  selected_planets_ids.count = 0;

  StarsShaderInit((StarsShaderConfig){
      .size = 0.5,
      .brightness = 0.4,
      .density = 0.5,
      .time_scale = 1,
      .seed = 1,
  });

  for (unsigned i = 0; i < players.count; i++) {
    if (!StartPlayer(players.items + i))
      NOB_TODO("Can't handle errors yet");
    if (players.items[i].type == PLAYER_HUMAN) {
      player_id = i;
    }
  }

  // Clay_SetDebugModeEnabled(true);
}

static void HandlePlanetSelection(Clay_BoundingBox box) {
  // selected_planets_ids.count = 0;
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    for (unsigned planet_idx = 0; planet_idx < game_state.planets.count;
         planet_idx++) {
      const Planet planet = game_state.planets.items[planet_idx];
      Vector2 box_coords = Game2boxCoords(planet.coords, box, game_space);
      if (CheckCollisionPointCircle(GetMousePosition(), box_coords,
                                    GetPlanetRadius(planet))) {
        if (planet.owner == player_id + 1) {
          bool already_added = false;
          for (unsigned i = 0; i < selected_planets_ids.count; i++) {
            if (selected_planets_ids.items[i] == planet_idx) {
              nob_da_remove_unordered(&selected_planets_ids, i);
              already_added = true;
              break;
            }
          }
          if (!already_added)
            nob_da_append(&selected_planets_ids, planet_idx);
        } else {
          const unsigned src_id = nob_da_pop(&selected_planets_ids);
          SendPlayerShips(&game_state, player_id, src_id, planet_idx,
                          game_state.planets.items[src_id].ships / 2);
        }
      }
    }
  }
  printf("selected count = %u\n", selected_planets_ids.count);
}

static void DrawPlanetHighlight(Planet planet, Clay_BoundingBox box) {
  Vector2 draw_coords = Game2boxCoords(planet.coords, box, game_space);

  const float draw_radius = GetPlanetRadius(planet);

  DrawCircleLinesV(draw_coords, draw_radius + 10, YELLOW);
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
    Clay_BoundingBox box = Component_GameFrame((DrawGameFrameUserData){
        .game_space = game_space,
        .planets = game_state.planets.items,
        .planet_count = game_state.planets.count,
        .fleets = game_state.fleets.items,
        .fleet_count = game_state.fleets.count,
    });
    HandlePlanetSelection(box);
    nob_da_foreach(unsigned, planet_idx, &selected_planets_ids) {
      DrawPlanetHighlight(game_state.planets.items[*planet_idx], box);
    }
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
