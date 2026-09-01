#include "raylib.h"

#include "../game_log.h"

#include "clay.h"
#include "components.h"
#include "nob.h"
#include "stars_shader.h"
#include "ui_utils.h"
#include "viewer.h"

typedef struct {
  Vector2 min_coords;
  Vector2 max_coords;
  // Planets max radius
  uint8_t max_radius;
} GameSpace;

static void DrawGameFrame(Clay_BoundingBox bounding_box);

static CustomElementData game_frame_data = {
    .type = CUSTOM_ELEMENT_TYPE_FUNCTION, .as.function = &DrawGameFrame};

static GameSpace game_space = {0};
static Font font;
static unsigned int frame_counter;
// Game run speed in viewer. 0 is realtime, higher is slower.
static int game_speed;
static bool game_running;
static bool playing_forewards;

static GameLog game_log = {0};
static unsigned turn;

static inline float GetPlanetRadius(Planet planet) {
  return BASE_PLANET_RADIUS + PLANET_RADIUS_GROWTH_CURVE -
         PLANET_RADIUS_GROWTH_CURVE / fmaxf(planet.growth, 1);
}

// Calculates the minimum and maximum coordinates of all planets, used to space
// planets across the entire screen.
void ComputeGameSpace(Planet *planets, unsigned p_count) {
  game_space = (GameSpace){
      .min_coords = {INFINITY, INFINITY},
      .max_coords = {-INFINITY, -INFINITY},
      .max_radius = -INFINITY,
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
}

Vector2 Game2ScreenCoords(Vector2 coords, Clay_BoundingBox bounding_box) {
  return (Vector2){
      .x = Remap(coords.x, game_space.min_coords.x, game_space.max_coords.x,
                 bounding_box.x + game_space.max_radius,
                 bounding_box.x + bounding_box.width - game_space.max_radius),
      .y = Remap(coords.y, game_space.min_coords.y, game_space.max_coords.y,
                 bounding_box.y + game_space.max_radius,
                 bounding_box.y + bounding_box.height - game_space.max_radius)};
}

static inline Color GetOwnerColor(int owner) {
  return owner == 0 ? GRAY
                    : ColorFromHSV((((owner - 1) * 7) % MAX_PLAYER_AMOUNT) *
                                       360.0f / MAX_PLAYER_AMOUNT,
                                   1.0f, 1.0f);
}

void DrawTextCenteredOnPoint(Vector2 center, const char *text, float font_size,
                             float spacing, Color tint) {

  const Vector2 text_measurements =
      MeasureTextEx(font, text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(center, Vector2Scale(text_measurements, 0.5));

  DrawTextEx(font, text, text_coords, font_size, spacing, tint);
}

// Draws a Planet on the screen.
void DrawPlanet(Planet planet, Clay_BoundingBox bounding_box) {
  Vector2 draw_coords = Game2ScreenCoords(planet.coords, bounding_box);

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
  DrawTextCenteredOnPoint(draw_coords, ships_text, font_size, 1, BLACK);
}

void DrawFleet(Planet *planets, Fleet fleet, Clay_BoundingBox bounding_box) {
  Vector2 draw_coords = Game2ScreenCoords(
      Vector2Lerp(planets[fleet.src_id].coords, planets[fleet.dst_id].coords,
                  1 - (float)fleet.remaining / fleet.total),
      bounding_box);

  const char *ships_text = TextFormat("%d", fleet.ships);
  const float font_size = SHIP_FONT_SIZE;

  DrawTextCenteredOnPoint(draw_coords, ships_text, font_size, 1,
                          GetOwnerColor(fleet.owner));
}

static void HandleScrubberHover() {
  static bool is_scrubber_held = false;

  if (Clay_Hovered() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    is_scrubber_held = true;
    game_running = false;
  }

  if (!is_scrubber_held)
    return;

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    is_scrubber_held = false;
  } else {
    Clay_ElementData e_data =
        Clay_GetElementData((Clay_ElementId){.id = Clay_GetOpenElementId()});
    if (!e_data.found)
      return;

    Clay_BoundingBox box = e_data.boundingBox;

    unsigned clmp_mouse = Clamp(GetMousePosition().x, box.x, box.x + box.width);
    turn = Remap(clmp_mouse, box.x, box.x + box.width, 0, game_log.count - 1);
  }
}

static void ScrubberTrackComponent() {
  static Nob_String_Builder scrubber_text = {0};
  // clang-format off
  CLAY(CLAY_ID("ScrubberTrack"), {
  .layout = {
    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(12)},
    .childAlignment = {CLAY_ALIGN_X_LEFT,CLAY_ALIGN_Y_CENTER}
  },
  .backgroundColor = C_GRAY,
  .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
  }) {
    HandleScrubberHover();

    CLAY(CLAY_ID("PlayedTrack"), {
    .layout = {
      .sizing = {.width = CLAY_SIZING_PERCENT((float)turn/(game_log.count-1)), .height = CLAY_SIZING_GROW(0)},
    },
    .backgroundColor = C_LIGHTGRAY,
    .cornerRadius = {.topLeft = FLT_MAX, .bottomLeft = FLT_MAX},
    }) {
      CLAY(CLAY_ID("ScrubberThumb"), {
      .floating = {
        .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        .attachTo = CLAY_ATTACH_TO_PARENT,
        .attachPoints = {
          .parent = CLAY_ATTACH_POINT_RIGHT_CENTER,
          .element = CLAY_ATTACH_POINT_CENTER_CENTER,
        }
      },
      .layout = {
        .sizing = {.width = CLAY_SIZING_FIT(48), .height = CLAY_SIZING_FIXED(24)},
        .childAlignment = {CLAY_ALIGN_X_CENTER,CLAY_ALIGN_Y_CENTER},
      },
      .backgroundColor = C_WHITE,
      .cornerRadius = CLAY_CORNER_RADIUS(4),
      }) {
        scrubber_text.count = 0;
        nob_sb_appendf(&scrubber_text, "%u", turn);
        CLAY_TEXT(SB_TO_CLAY(scrubber_text), {.fontId = 2, .fontSize = 24, .textColor = C_BLACK});
      }
    }
  }
  // clang-format on
}

static void PlaybackControlsComponent() {
  CLAY(CLAY_ID("PlaybackControls"), {.layout = {.childGap = 8}}) {
    if (Component_Button(CLAY_STRING("|<"), BUTTON_STYLE_CONTROLLER) &&
        turn > 0) {
      playing_forewards = false;
      game_running = true;
    }
    if (Component_Button(CLAY_STRING("||"), BUTTON_STYLE_CONTROLLER)) {
      game_running = !game_running;
    }
    if (Component_Button(CLAY_STRING(">|"), BUTTON_STYLE_CONTROLLER) &&
        turn < game_log.count) {
      playing_forewards = true;
      game_running = true;
    }
  }
}

static void ControlsComponent() {
  // clang-format off
  CLAY(CLAY_ID("ControlsContainer"), {
    .layout = {
      .sizing = {.width = CLAY_SIZING_GROW(0), .height=CLAY_SIZING_FIT(0)},
      .padding = {32,32,24,24},
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .childGap = 24,
    },
    .backgroundColor = (Clay_Color){255,255,255,15},
    .border = {.width = CLAY_BORDER_OUTSIDE(1), .color = C_WHITE},
    .cornerRadius = CLAY_CORNER_RADIUS(8)
  }) {

    ScrubberTrackComponent();

    CLAY(CLAY_ID("PlaybackControlsContainer")) {
      PlaybackControlsComponent();
      CLAY(CLAY_ID("SpeedControls"));
    }
  }
  // clang-format on
}

void DrawControls() {
  static bool is_scrubber_pressed = false;
  // Values for round rectanlges
  const int segments = 6;
  const float roundness = 0.4;

  // Margin between UI elements.
  const float ui_margin = 10;
  const float button_edge = 40;
  const float scrubber_height = 10;
  const int button_amount = 3;

  const Color button_color = WHITE;
  const Color button_pressed_color = GRAY;

  const Rectangle scrubber = {
      .x = MAP_MARGIN,
      .y = GetScreenHeight() - button_edge - ui_margin * 2 - scrubber_height,
      .width = GetScreenWidth() - MAP_MARGIN * 2,
      .height = scrubber_height,
  };
  DrawRectangleRoundedLines(scrubber, roundness, segments, WHITE);

  const float bar_height = scrubber.height * 2;
  const float bar_width = 40;

  Rectangle bar = {
      .x = Remap(turn, 0, game_log.count - 1, scrubber.x - bar_width / 2,
                 scrubber.x + scrubber.width - bar_width / 2),
      .y = scrubber.y - bar_height / 2 + scrubber.height / 2,
      .width = bar_width,
      .height = bar_height,
  };

  if (CheckCollisionPointRec(GetMousePosition(), scrubber) &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    is_scrubber_pressed = true;
    game_running = false;
  }

  if (is_scrubber_pressed) {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      is_scrubber_pressed = false;
    } else {
      turn =
          Remap(fmin(scrubber.x + scrubber.width,
                     fmax(scrubber.x, GetMousePosition().x)),
                scrubber.x, scrubber.x + scrubber.width, 0, game_log.count - 1);
    }
  }

  const float font_size = 20;
  const float spacing = 1;
  const char *turn_text = TextFormat("%d", turn);

  Vector2 text_measurements =
      MeasureTextEx(font, turn_text, font_size, spacing);
  const Vector2 text_coords = {
      .x = bar.x + (bar_width - text_measurements.x) / 2,
      .y = bar.y + (bar_height - text_measurements.y) / 2};

  DrawRectangleRounded(bar, roundness, segments, WHITE);
  DrawTextEx(font, turn_text, text_coords, font_size, spacing, BLACK);

  Rectangle button = {.y = scrubber.y + scrubber.height + ui_margin,
                      .width = button_edge,
                      .height = button_edge};
  Color button_computed_color;
  const int button_row_width =
      button_edge * button_amount + ui_margin * (button_amount - 1);

#define HandleMousePress(action)                                               \
  do {                                                                         \
    if (CheckCollisionPointRec(GetMousePosition(), button)) {                  \
      button_computed_color =                                                  \
          ColorLerp(button_color, button_pressed_color,                        \
                    IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1 : 0.3);           \
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {                          \
        action;                                                                \
      }                                                                        \
    } else {                                                                   \
      button_computed_color = button_color;                                    \
    }                                                                          \
  } while (0)

  // Play backwards
  button.x = (GetScreenWidth() - button_row_width) / 2.0f;
  HandleMousePress(if (turn > 0) {
    playing_forewards = false;
    game_running = true;
  });
  DrawRectangleRounded(button, roundness, segments, button_computed_color);
  DrawTriangle((Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge * 4 / 5},
               (Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge / 5},
               (Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge / 2},
               BLACK);

  // Play/Pause toggle
  button.x += button_edge + ui_margin;
  HandleMousePress(game_running = !game_running);
  DrawRectangleRounded(button, roundness, segments, button_computed_color);
  bar.width = button_edge / 5;
  bar.height = button_edge * 3 / 5;
  bar.y = button.y + button_edge / 5;
  bar.x = button.x + button_edge / 5;
  DrawRectangleRounded(bar, 0.75, segments, BLACK);
  bar.x = button.x + button_edge * 3 / 5;
  DrawRectangleRounded(bar, 0.75, segments, BLACK);

  // Play forewards
  button.x += button_edge + ui_margin;
  HandleMousePress(if (turn < game_log.count) {
    playing_forewards = true;
    game_running = true;
  });
  DrawRectangleRounded(button, roundness, segments, button_computed_color);
  DrawTriangle((Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge * 4 / 5},
               (Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge / 2},
               (Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge / 5},
               BLACK);

// TODO add slider or other UI to control replay speed
#undef HandleMousePress

  const float indicator_size = 15;
  const float margin_between_players = 25;
  const float text_margin = 8;

  text_measurements = MeasureTextEx(font, "P99", font_size, spacing);
  float total_labels_width = (text_measurements.x + indicator_size +
                              text_margin + margin_between_players) *
                                 game_log.players.count -
                             margin_between_players;

  Rectangle color_indicator = {.x = (GetScreenWidth() - total_labels_width) / 2,
                               .y = 25,
                               .width = indicator_size,
                               .height = indicator_size};

  for (unsigned i = 0; i < game_log.players.count; i++) {
    Color player_color = GetOwnerColor(i + 1);

    DrawRectangleRec(color_indicator, player_color);
    // Indicator border
    DrawRectangleLinesEx(color_indicator, 1.0f, RAYWHITE);

    const char *player_text = TextFormat("P%u", i + 1);
    text_measurements = MeasureTextEx(font, player_text, font_size, spacing);

    Vector2 text_pos = {
        .x = color_indicator.x + color_indicator.width + text_margin,
        .y = color_indicator.y +
             (color_indicator.height - text_measurements.y) / 2};
    DrawTextEx(font, player_text, text_pos, font_size, spacing, WHITE);

    color_indicator.x += color_indicator.width + text_margin +
                         text_measurements.x + margin_between_players;
  }
}

void ViewerInit() {
  assert(game_log.count > 0);
  Clay_SetDebugModeEnabled(true);

  turn = 0;
  frame_counter = 0;
  game_speed = 5;
  game_running = false;
  playing_forewards = true;

  ComputeGameSpace(game_log.items[0].planets, game_log.items[0].planet_count);

  font = GetFontDefault();

  StarsShaderInit((StarsShaderConfig){
      .size = 0.5,
      .brightness = 0.4,
      .density = 0.5,
      .time_scale = 1,
      .seed = 1,
  });
}

static void DrawGameFrame(Clay_BoundingBox bounding_box) {
  NOB_UNUSED(bounding_box);
  StarsShaderDraw((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});

  for (unsigned i = 0; i < game_log.items[turn].planet_count; i++) {
    DrawPlanet(game_log.items[turn].planets[i], bounding_box);
  }

  for (unsigned i = 0; i < game_log.items[turn].fleet_count; i++) {
    DrawFleet(game_log.items[turn].planets, game_log.items[turn].fleets[i],
              bounding_box);
  }
}

void ViewerDraw() {
  frame_counter++;
  if (game_running &&
      (game_speed == 0 || frame_counter % abs(game_speed) == 0)) {
    if (playing_forewards && turn < game_log.count - 1)
      turn++;
    else if (turn > 0)
      turn--;
    if (turn >= game_log.count - 1 || turn == 0)
      game_running = false;
  }

  if (IsKeyPressed(KEY_RIGHT)) {
    game_running = false;
    turn++;
    if (turn >= game_log.count)
      turn = game_log.count - 1;
  } else if (IsKeyPressed(KEY_LEFT)) {
    game_running = false;
    if (turn != 0)
      turn--;
  } else if (IsKeyPressed(KEY_SPACE)) {
    game_running = !game_running;
  }
  if (IsKeyPressed(KEY_UP) && game_speed > 0) {
    game_speed--;
  } else if (IsKeyPressed(KEY_DOWN) && game_speed < MAX_GAME_SPEED_VALUE) {
    game_speed++;
  }

  // ============= START DRAWING =============
  ClearBackground(BLACK);
  // clang-format off
  CLAY(CLAY_ID("OuterContainer"), {
       .layout = {
         .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
         .padding = CLAY_PADDING_ALL(MAP_MARGIN),
         .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
         .layoutDirection = CLAY_TOP_TO_BOTTOM,
         .childGap = 32,
       },
   }) {
    CLAY(CLAY_ID("GameFrame"), {
       .layout = {
         .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
       },
       .custom = {.customData = &game_frame_data},
       });
    ControlsComponent();
  }
  // clang-format on
}

void ViewerDestroy() {
  StarsShaderDestroy();
  FreeInnerGameLog(game_log);
  game_log = (GameLog){0};
}

void SetGameLog(GameLog new_game_log) {
  game_log = DeepCopyGameLog(new_game_log);
}

const UIScreen viewer_screen = {
    .init = ViewerInit,
    .draw = ViewerDraw,
    .destroy = ViewerDestroy,
};
