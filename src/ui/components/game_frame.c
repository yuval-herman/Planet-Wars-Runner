#include "clay.h"
#include "nob.h"
#include "raylib.h"

#include "../ui_utils.h"

#include "../../planet_wars.h"
#include "../ui.h"

typedef struct {
  // Planets min and max coordinates
  Vector2 min_coords;
  Vector2 max_coords;
  // Planets max radius
  float max_radius;
} GameSpace;

typedef struct {
  Planet *planets;
  unsigned planet_count;
  Fleet *fleets;
  unsigned fleet_count;
  GameSpace game_space;
} DrawGameFrameUserData;

// Calculates the minimum and maximum coordinates of all planets, used to space
// planets across the entire screen.
GameSpace ComputeGameSpace(Planet *planets, unsigned p_count);
Vector2 Game2boxCoords(Vector2 coords, Clay_BoundingBox bounding_box,
                       GameSpace game_space);
Vector2 box2GameCoords(Vector2 coords, Clay_BoundingBox bounding_box,
                       GameSpace game_space);
float GetPlanetRadius(Planet planet);

Clay_BoundingBox Component_GameFrame(DrawGameFrameUserData user_data);

// Don't include implementation when included from the components header file
#ifndef COMPONENTS_H

#define BASE_PLANET_RADIUS 10.0f
#define PLANET_RADIUS_GROWTH_CURVE 20.0f
#define PLANET_RING_MAX_RADIUS 3.0F
#define SHIP_FONT_SIZE 20

float GetPlanetRadius(Planet planet) {
  return BASE_PLANET_RADIUS + PLANET_RADIUS_GROWTH_CURVE -
         PLANET_RADIUS_GROWTH_CURVE / fmaxf(planet.growth, 1);
}

// Calculates the minimum and maximum coordinates of all planets, used to space
// planets across the entire screen.
GameSpace ComputeGameSpace(Planet *planets, unsigned p_count) {
  GameSpace game_space = {
      .min_coords = {INFINITY, INFINITY},
      .max_coords = {-INFINITY, -INFINITY},
      .max_radius = 0,
  };

  for (unsigned i = 0; i < p_count; i++) {
    game_space.min_coords.x =
        CLAY__MIN(game_space.min_coords.x, planets[i].coords.x);
    game_space.min_coords.y =
        CLAY__MIN(game_space.min_coords.y, planets[i].coords.y);
    game_space.max_coords.x =
        CLAY__MAX(game_space.max_coords.x, planets[i].coords.x);
    game_space.max_coords.y =
        CLAY__MAX(game_space.max_coords.y, planets[i].coords.y);

    game_space.max_radius =
        CLAY__MAX(game_space.max_radius, GetPlanetRadius(planets[i]));
  }

  return game_space;
}

Vector2 Game2boxCoords(Vector2 coords, Clay_BoundingBox bounding_box,
                       GameSpace game_space) {
  return (Vector2){
      .x = Remap(coords.x, game_space.min_coords.x, game_space.max_coords.x,
                 bounding_box.x + game_space.max_radius,
                 bounding_box.x + bounding_box.width - game_space.max_radius),
      .y = Remap(coords.y, game_space.min_coords.y, game_space.max_coords.y,
                 bounding_box.y + game_space.max_radius,
                 bounding_box.y + bounding_box.height - game_space.max_radius)};
}

Vector2 box2GameCoords(Vector2 coords, Clay_BoundingBox bounding_box,
                       GameSpace game_space) {
  return (Vector2){
      .x = Remap(coords.x, bounding_box.x + game_space.max_radius,
                 bounding_box.x + bounding_box.width - game_space.max_radius,
                 game_space.min_coords.x, game_space.max_coords.x),
      .y = Remap(coords.y, bounding_box.y + game_space.max_radius,
                 bounding_box.y + bounding_box.height - game_space.max_radius,
                 game_space.min_coords.y, game_space.max_coords.y)};
}

static inline Color GetOwnerColor(int owner) {
  return owner == 0 ? GRAY
                    : ColorFromHSV((((owner - 1) * 7) % MAX_PLAYER_AMOUNT) *
                                       360.0f / MAX_PLAYER_AMOUNT,
                                   1.0f, 1.0f);
}

static inline void DrawTextCenteredOnPoint(Vector2 center, const char *text,
                                           float font_size, float spacing,
                                           Color tint, Font font) {

  const Vector2 text_measurements =
      MeasureTextEx(font, text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(center, Vector2Scale(text_measurements, 0.5));

  DrawTextEx(font, text, text_coords, font_size, spacing, tint);
}

static void DrawPlanet(Planet planet, Clay_BoundingBox bounding_box,
                       GameSpace game_space) {
  Vector2 draw_coords = Game2boxCoords(planet.coords, bounding_box, game_space);

  const float draw_radius = GetPlanetRadius(planet);

  // Wrapped time for use in repeating functions
  float r_time = fmodf(GetTime(), PLANET_RING_MAX_RADIUS);
  float ring_radius = r_time;

  DrawCircleV(draw_coords, draw_radius, GetOwnerColor(planet.owner));
  DrawCircleLinesV(
      draw_coords, draw_radius + ring_radius,
      ColorAlpha(GetOwnerColor(planet.owner),
                 (4 * r_time * (PLANET_RING_MAX_RADIUS - r_time)) /
                     (PLANET_RING_MAX_RADIUS * PLANET_RING_MAX_RADIUS)));

  const float font_size = draw_radius;
  const char *ships_text = TextFormat("%d", planet.ships);
  DrawTextCenteredOnPoint(draw_coords, ships_text, font_size, 1, BLACK,
                          GetFontDefault());
}

static void DrawFleet(Planet *planets, Fleet fleet,
                      Clay_BoundingBox bounding_box, GameSpace game_space) {
  Vector2 draw_coords = Game2boxCoords(
      Vector2Lerp(planets[fleet.src_id].coords, planets[fleet.dst_id].coords,
                  1 - (float)fleet.remaining / fleet.total),
      bounding_box, game_space);

  const char *ships_text = TextFormat("%d", fleet.ships);
  const float font_size = SHIP_FONT_SIZE;

  DrawTextCenteredOnPoint(draw_coords, ships_text, font_size, 1,
                          GetOwnerColor(fleet.owner), GetFontDefault());
}

static void DrawGameFrame(Clay_BoundingBox bounding_box, void *user_data) {
  DrawGameFrameUserData *data = user_data;
  for (unsigned i = 0; i < data->planet_count; i++) {
    DrawPlanet(data->planets[i], bounding_box, data->game_space);
  }

  for (unsigned i = 0; i < data->fleet_count; i++) {
    DrawFleet(data->planets, data->fleets[i], bounding_box, data->game_space);
  }
}

struct DrawGameFrameArenaItem {
  DrawGameFrameUserData user_data;
  CustomElementData custom_element_data;
};

static struct GameFrameMemoryArena {
  struct DrawGameFrameArenaItem *items;
  size_t count;
  size_t capacity;
} frame_arena = {0};

// Used to make the first frame behave like any other frame.
static unsigned last_frame = UINT_MAX;

Clay_BoundingBox Component_GameFrame(DrawGameFrameUserData user_data) {
  if (last_frame != GetFrame()) {
    last_frame = GetFrame();
    frame_arena.count = 0;
  }
  nob_da_append(&frame_arena, (struct DrawGameFrameArenaItem){0});
  struct DrawGameFrameArenaItem *data = &nob_da_last(&frame_arena);
  data->user_data = user_data;
  data->custom_element_data = (CustomElementData){
      .type = CUSTOM_ELEMENT_TYPE_FUNCTION,
      .as.function = {.user_data = &data->user_data, .fn = &DrawGameFrame},
  };

  Clay_BoundingBox box = {0};
  CLAY_AUTO_ID({
      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
      .custom = {.customData = &data->custom_element_data},
  }) {
    Clay_ElementData e_data =
        Clay_GetElementData((Clay_ElementId){.id = Clay_GetOpenElementId()});
    if (e_data.found)
      box = e_data.boundingBox;
  }
  return box;
}

#endif // COMPONENTS_H
