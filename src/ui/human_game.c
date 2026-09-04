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
static unsigned short bot_speed = 200;
static bool game_running = true;

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

  game_running = true;
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

static Rectangle GetSelectionRect() {
  static bool is_holding_select = false;
  static Vector2 selection_start = {0};
  Rectangle select_rect = {0};
  if (!is_holding_select && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
    is_holding_select = true;
    Vector2 mp = GetMousePosition();
    selection_start.x = mp.x;
    selection_start.y = mp.y;
  } else if (is_holding_select) {
    if (IsMouseButtonUp(MOUSE_LEFT_BUTTON)) {
      is_holding_select = false;
    } else {
      Vector2 mp = GetMousePosition();
      select_rect = (Rectangle){
          .x = selection_start.x,
          .y = selection_start.y,
          .width = mp.x - selection_start.x,
          .height = mp.y - selection_start.y,
      };
      if (select_rect.width < 0) {
        select_rect.x = mp.x;
        select_rect.width *= -1;
      }
      if (select_rect.height < 0) {
        select_rect.y = mp.y;
        select_rect.height *= -1;
      }
      DrawRectangleRec(select_rect, (Color){0, 121, 241, 125});
    }
  }
  return select_rect;
}

static void AddPlanetToSelection(unsigned planet_id) {
  const Planet planet = game_state.planets.items[planet_id];
  if (planet.owner == player_id + 1) {
    bool already_added = false;
    for (unsigned i = 0; i < selected_planets_ids.count; i++) {
      if (selected_planets_ids.items[i] == planet_id) {
        already_added = true;
        break;
      }
    }
    if (!already_added)
      nob_da_append(&selected_planets_ids, planet_id);
  }
}

static void RemovePlanetFromSelection(unsigned planet_id) {
  for (unsigned i = 0; i < selected_planets_ids.count; i++) {
    if (selected_planets_ids.items[i] == planet_id) {
      nob_da_remove_unordered(&selected_planets_ids, i);
      break;
    }
  }
}

static bool PlanetInRect(Clay_BoundingBox box, Rectangle rect, Planet planet) {
  Vector2 box_coords = Game2boxCoords(planet.coords, box, game_space);
  return (CheckCollisionCircleRec(box_coords, GetPlanetRadius(planet), rect));
}

static bool PointInPlanet(Clay_BoundingBox box, Planet planet, Vector2 point) {
  Vector2 box_coords = Game2boxCoords(planet.coords, box, game_space);
  return (
      CheckCollisionPointCircle(point, box_coords, GetPlanetRadius(planet)));
}

// Pass - for `num_ships` to attempt to send enough ships to take over the
// destination planet automatically. If the destionation planet is ours, 0 will
// send half the available ships.
static void SendSelectedPlanetShips(unsigned dst_planet_id,
                                    unsigned num_ships) {
  const Planet dst_planet = game_state.planets.items[dst_planet_id];
  bool enemy_planet_auto_conquer = num_ships == 0;

  if (dst_planet.owner == player_id + 1) {
    enemy_planet_auto_conquer = false;
  }

  uint8_t max_travel_time = 0;
  uint16_t selected_num_ships = 0;

  nob_da_foreach(unsigned, planet_id, &selected_planets_ids) {
    const Planet selected_planet = game_state.planets.items[*planet_id];
    if (enemy_planet_auto_conquer) {
      const uint8_t travel_time =
          GetTravelTime(selected_planet.coords, dst_planet.coords);
      max_travel_time = CLAY__MAX(max_travel_time, travel_time);
    }
    selected_num_ships += selected_planet.ships;
  }

  const uint16_t ships_when_last_fleet_arrives =
      1 + (!dst_planet.owner
               ? dst_planet.ships
               : dst_planet.ships + dst_planet.growth * max_travel_time);

  while (selected_planets_ids.count > 0) {
    const unsigned src_id = nob_da_pop(&selected_planets_ids);
    const Planet src_planet = game_state.planets.items[src_id];
    uint16_t ships_to_send;
    if (enemy_planet_auto_conquer) {
      ships_to_send =
          ceilf(src_planet.ships *
                ((float)ships_when_last_fleet_arrives / selected_num_ships));
    } else {
      ships_to_send = num_ships == 0 ? src_planet.ships / 2 : num_ships;
    }

    SendPlayerShips(&game_state, player_id, src_id, dst_planet_id,
                    ships_to_send);
  }
}

static void HandleMouse(Clay_BoundingBox box) {
  Rectangle select_rect = GetSelectionRect();
  if (select_rect.width || select_rect.height) {
    for (unsigned planet_id = 0; planet_id < game_state.planets.count;
         planet_id++) {
      if (PlanetInRect(box, select_rect, game_state.planets.items[planet_id])) {
        AddPlanetToSelection(planet_id);
      } else {
        RemovePlanetFromSelection(planet_id);
      }
    }
  } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    for (unsigned planet_id = 0; planet_id < game_state.planets.count;
         planet_id++) {
      if (PointInPlanet(box, game_state.planets.items[planet_id],
                        GetMousePosition())) {
        AddPlanetToSelection(planet_id);
      } else {
        RemovePlanetFromSelection(planet_id);
      }
    }
  }

  if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
    for (unsigned planet_idx = 0; planet_idx < game_state.planets.count;
         planet_idx++) {
      if (PointInPlanet(box, game_state.planets.items[planet_idx],
                        GetMousePosition())) {
        SendSelectedPlanetShips(planet_idx, 0);
        break;
      }
    }
  }
}

static void DrawPlanetHighlight(Planet planet, Clay_BoundingBox box) {
  Vector2 draw_coords = Game2boxCoords(planet.coords, box, game_space);

  const float draw_radius = GetPlanetRadius(planet);

  DrawCircleLinesV(draw_coords, draw_radius + 10, YELLOW);
}

static void HumanGameDraw() {
  static Nob_String_Builder winner_sb = {0};
  winner_sb.count = 0;
  if (game_state.remaining_players == 1) {
    game_running = false;
  }

  if (game_running) {
    if (bot_speed == 0 || GetFrame() % bot_speed == 0)
      RunPlayerCycle(&game_state, players, &sb);

    if (game_speed == 0 || GetFrame() % game_speed == 0)
      AdvanceTurn(&game_state);
  }

  // ====== DRAWING =======

  StarsShaderDraw((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});

  CLAY(CLAY_ID("OuterContainer"),
       {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .padding = CLAY_PADDING_ALL(10),
                   .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                      .y = CLAY_ALIGN_Y_CENTER},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = 32}}) {
    if (game_running) {
      Clay_BoundingBox box = Component_GameFrame((DrawGameFrameUserData){
          .game_space = game_space,
          .planets = game_state.planets.items,
          .planet_count = game_state.planets.count,
          .fleets = game_state.fleets.items,
          .fleet_count = game_state.fleets.count,
      });
      HandleMouse(box);
      nob_da_foreach(unsigned, planet_idx, &selected_planets_ids) {
        DrawPlanetHighlight(game_state.planets.items[*planet_idx], box);
      }
    } else {
      int winner_idx = bit_index(game_state.player_bit_set);
      if (winner_idx == -1) {
        nob_sb_appendf(&winner_sb, "It's a draw!");
      } else {
        nob_sb_appendf(&winner_sb, "%.*s won!",
                       (int)players.items[winner_idx].name.count,
                       players.items[winner_idx].name.items);
      }

      CLAY_TEXT(SB_TO_CLAY(winner_sb), {.fontId = 2, .fontSize = 64, .textColor = C_WHITE});
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
