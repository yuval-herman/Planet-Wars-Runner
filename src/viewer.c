#include "raylib.h"

#include "nob.h"
#include "planet_wars.h"
#include "viewer.h"

typedef struct {
  Vector2 min_coords;
  Vector2 max_coords;
} GameSpace;

GameSpace game_space = {.min_coords = {INFINITY, INFINITY},
                        .max_coords = {-INFINITY, -INFINITY}};
Font font;
unsigned int frame_counter = 0;
// Game run speed in viewer. 0 is realtime, higher is slower.
int game_speed = 5;
bool game_running = false;
bool playing_forewards = true;

// Calculates the minimum and maximum coordinates of all planets, used to space
// planets across the entire screen.
void ComputeGameSpace(Planet *planets, unsigned p_count) {
  for (unsigned i = 0; i < p_count; i++) {
    game_space.min_coords =
        Vector2Min(planets[i].coords, game_space.min_coords);
    game_space.max_coords =
        Vector2Max(planets[i].coords, game_space.max_coords);
  }
}

Vector2 Game2ScreenCoords(Vector2 coords) {
  return (Vector2){
      .x = Remap(coords.x, game_space.min_coords.x, game_space.max_coords.x,
                 MAP_MARGIN, GetScreenWidth() - MAP_MARGIN),
      .y = GetScreenHeight() -
           Remap(coords.y, game_space.min_coords.y, game_space.max_coords.y,
                 MAP_MARGIN + CONTROLS_HEIGHT,
                 GetScreenHeight() - MAP_MARGIN - PLAYER_LABELS_HEIGHT)};
}

static inline Color GetOwnerColor(int owner) {
  return owner == 0 ? GRAY
                    : ColorFromHSV((((owner - 1) * 7) % MAX_BOT_AMOUNT) *
                                       360.0f / MAX_BOT_AMOUNT,
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
void DrawPlanet(Planet planet) {
  Vector2 draw_coords = Game2ScreenCoords(planet.coords);

  const float draw_radius =
      BASE_PLANET_RADIUS + PLANET_RADIUS_GROWTH_CURVE -
      PLANET_RADIUS_GROWTH_CURVE / fmaxf(planet.growth, 1);

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

void DrawFleet(Planet *planets, Fleet fleet) {
  Vector2 draw_coords = Game2ScreenCoords(
      Vector2Lerp(planets[fleet.src_id].coords, planets[fleet.dst_id].coords,
                  1 - (float)fleet.remaining / fleet.total));

  const char *ships_text = TextFormat("%d", fleet.ships);
  const float font_size = SHIP_FONT_SIZE;

  DrawTextCenteredOnPoint(draw_coords, ships_text, font_size, 1,
                          GetOwnerColor(fleet.owner));
}

void DrawControls(GameLog game_log, unsigned *turn) {
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
      .x = Remap(*turn, 0, game_log.count - 1, scrubber.x - bar_width / 2,
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
      *turn =
          Remap(fmin(scrubber.x + scrubber.width,
                     fmax(scrubber.x, GetMousePosition().x)),
                scrubber.x, scrubber.x + scrubber.width, 0, game_log.count - 1);
    }
  }

  const float font_size = 20;
  const float spacing = 1;
  const char *turn_text = TextFormat("%d", *turn);

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
  HandleMousePress(if (*turn > 0) {
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
  HandleMousePress(if (*turn < game_log.count) {
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

  const char *player_text = "P99";
  text_measurements = MeasureTextEx(font, player_text, font_size, spacing);
  float total_labels_width = (text_measurements.x + indicator_size +
                              text_margin + margin_between_players) *
                                 game_log.bots.count -
                             margin_between_players;

  Rectangle color_indicator = {.x = (GetScreenWidth() - total_labels_width) / 2,
                               .y = 25,
                               .width = indicator_size,
                               .height = indicator_size};

  for (unsigned i = 0; i < game_log.bots.count; i++) {
    Color player_color = GetOwnerColor(i + 1);

    DrawRectangleRec(color_indicator, player_color);
    // Indicator border
    DrawRectangleLinesEx(color_indicator, 1.0f, RAYWHITE);

    const char *player_text = TextFormat("P%u", i + 1);
    Vector2 text_measurements =
        MeasureTextEx(font, player_text, font_size, spacing);

    Vector2 text_pos = {
        .x = color_indicator.x + color_indicator.width + text_margin,
        .y = color_indicator.y +
             (color_indicator.height - text_measurements.y) / 2};
    DrawTextEx(font, player_text, text_pos, font_size, spacing, WHITE);

    color_indicator.x += color_indicator.width + text_margin +
                         text_measurements.x + margin_between_players;
  }
}

Shader SetupStarsShader(int screenWidth, int screenHeight) {
  Shader stars_shader = LoadShader(0, "shaders/stars.fs");

  int resLoc = GetShaderLocation(stars_shader, "uResolution");
  int starSizeLoc = GetShaderLocation(stars_shader, "uStarSize");
  int starBrightnessLoc = GetShaderLocation(stars_shader, "uStarBrightness");
  int starDensityLoc = GetShaderLocation(stars_shader, "uStarDensity");
  int seedLoc = GetShaderLocation(stars_shader, "uSeed");

  float resolution[2] = {(float)screenWidth, (float)screenHeight};
  SetShaderValue(stars_shader, resLoc, resolution, SHADER_UNIFORM_VEC2);

  float starSize = 0.5f;       // 1.0 is default, >1.0 makes stars bigger
  float starBrightness = 0.4f; // 1.0 is default, >1.0 boosts brightness
  float starDensity = 0.5f; // 1.0 is default, >1.0 creates more star clusters
  float starsSeed = 1;

  SetShaderValue(stars_shader, starSizeLoc, &starSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(stars_shader, starBrightnessLoc, &starBrightness,
                 SHADER_UNIFORM_FLOAT);
  SetShaderValue(stars_shader, starDensityLoc, &starDensity,
                 SHADER_UNIFORM_FLOAT);
  SetShaderValue(stars_shader, seedLoc, &starsSeed, SHADER_UNIFORM_FLOAT);

  return stars_shader;
}

void RunViewerForGame(GameLog game_log) {
  // Set the game to the first turn
  unsigned turn = 0;

  ComputeGameSpace(game_log.items[0].planets, game_log.items[0].planet_count);

  const int screenWidth = 800;
  const int screenHeight = 450;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Planet Wars Viewer");
  font = GetFontDefault();

  SetTargetFPS(60);

  Shader stars_shader = SetupStarsShader(screenWidth, screenHeight);

  int timeLoc = GetShaderLocation(stars_shader, "uTime");

  while (!WindowShouldClose()) {
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

    float time = (float)GetTime();
    SetShaderValue(stars_shader, timeLoc, &time, SHADER_UNIFORM_FLOAT);

    BeginDrawing();

    ClearBackground(BLACK);
    // 5. Activate the shader to affect the canvas drawings
    BeginShaderMode(stars_shader);
    // Draw a blank canvas area covering your screen size
    // The fragment shader fills this rectangle area
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
    EndShaderMode();

    for (unsigned i = 0; i < game_log.items[turn].planet_count; i++) {
      DrawPlanet(game_log.items[turn].planets[i]);
    }

    for (unsigned i = 0; i < game_log.items[turn].fleet_count; i++) {
      DrawFleet(game_log.items[turn].planets, game_log.items[turn].fleets[i]);
    }

    DrawControls(game_log, &turn);
    EndDrawing();
  }

  UnloadShader(stars_shader);
  CloseWindow();
}
