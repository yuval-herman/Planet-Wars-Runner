#include "raylib.h"
#include "raymath.h"

#include "game.h"
#include "planet_wars.h"
#include <stdio.h>
#include <time.h>

#define FLAG_IMPLEMENTATION
#include "flag.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define BASE_PLANET_RADIUS 10.0f
#define PLANET_RADIUS_GROWTH_CURVE 20.0f
#define PLANET_RING_MAX_RADIUS 3.0F
#define MAP_MARGIN 50
#define CONTROLS_HEIGHT 70
#define PLAYER_LABELS_HEIGHT 30
#define SHIP_FONT_SIZE 20
// The name is confusing, but this value denotes the maximum value that the
// `game_speed` variable can hold, which controls the _lowest_ bound for game
// speed, i.e. how slow can the game run.
#define MAX_GAME_SPEED_VALUE 20

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
void ComputeGameSpace(PlanetDA planets) {
  nob_da_foreach(Planet, planet, &planets) {
    game_space.min_coords = Vector2Min(planet->coords, game_space.min_coords);
    game_space.max_coords = Vector2Max(planet->coords, game_space.max_coords);
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
  const float spacing = 1;
  const char *ships_text = TextFormat("%d", planet.ships);

  const Vector2 text_measurements =
      MeasureTextEx(font, ships_text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(draw_coords, Vector2Scale(text_measurements, 0.5));

  DrawTextEx(font, ships_text, text_coords, font_size, spacing, BLACK);
}

void DrawFleet(GameState state, Fleet fleet) {
  Vector2 draw_coords =
      Game2ScreenCoords(Vector2Lerp(state.planets.items[fleet.src_id].coords,
                                    state.planets.items[fleet.dst_id].coords,
                                    1 - (float)fleet.remaining / fleet.total));

  const char *ships_text = TextFormat("%d", fleet.ships);
  const float font_size = SHIP_FONT_SIZE;
  const float spacing = 1;

  const Vector2 text_measurements =
      MeasureTextEx(font, ships_text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(draw_coords, Vector2Scale(text_measurements, 0.5));
  DrawTextEx(font, ships_text, text_coords, font_size, spacing,
             GetOwnerColor(fleet.owner));
}

void DrawControls(GameState *state) {
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
      .x = Remap(state->turn, 0, state->game_log.count - 1,
                 scrubber.x - bar_width / 2,
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
      state->turn = Remap(fmin(scrubber.x + scrubber.width,
                               fmax(scrubber.x, GetMousePosition().x)),
                          scrubber.x, scrubber.x + scrubber.width, 0,
                          state->game_log.count - 1);
      UpdateStateFromLogEntry(state, state->turn);
    }
  }

  const float font_size = 20;
  const float spacing = 1;
  const char *turn_text = TextFormat("%d", state->turn);

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
  HandleMousePress(if (state->turn > 0) {
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
  HandleMousePress(if ((size_t)state->turn < state->game_log.count) {
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
                                 state->bot_processes.count -
                             margin_between_players;

  Rectangle color_indicator = {.x = (GetScreenWidth() - total_labels_width) / 2,
                               .y = 25,
                               .width = indicator_size,
                               .height = indicator_size};

  for (size_t i = 0; i < state->bot_processes.count; i++) {
    Color player_color = GetOwnerColor(i + 1);

    DrawRectangleRec(color_indicator, player_color);
    // Indicator border
    DrawRectangleLinesEx(color_indicator, 1.0f, RAYWHITE);

    const char *player_text = TextFormat("P%zu", i + 1);
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

void RunTournament(const char *map_file_path,
                   const char *const bot_start_commands[], int bot_count) {
  char const *playing_bot_commands[2];
  for (int p1_idx = 0; p1_idx < bot_count - 1; p1_idx++) {
    for (int p2_idx = p1_idx + 1; p2_idx < bot_count; p2_idx++) {
      playing_bot_commands[0] = bot_start_commands[p1_idx];
      playing_bot_commands[1] = bot_start_commands[p2_idx];
      GameState state = MakeGame(map_file_path, playing_bot_commands, 2, false);
      RunGame(&state);
      FreeState(&state);
    }
  }
}

void Usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS] [--] [ARGS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

typedef struct {
  bool *help;
  bool *tournament;
  char **map_file;
  char **bot_commands;
  int bot_commands_count;
} CLIArguments;

CLIArguments RegisterFlagArguments() {
  CLIArguments args;
  args.help = flag_bool("help", false, "Show this help.");
  args.tournament =
      flag_bool("tournament", false,
                "Activate tournament mode. Tournament mode runs a round-robin "
                "tournamet between all bots and ranks them based on winnings.");
  args.map_file = flag_str("map", NULL, "The map file bots will play on.");
  args.bot_commands_count = 0;
  args.bot_commands = NULL;
  return args;
}

int main(int argc, char *argv[]) {
  CLIArguments args = RegisterFlagArguments();
  if (!flag_parse(argc, argv)) {
    Usage(stderr);
    flag_print_error(stderr);
    exit(1);
  } else if (*args.help) {
    Usage(stderr);
    return 0;
  } else if (!*args.map_file) {
    nob_log(NOB_ERROR, "A map file must be provided.");
    return 1;
  }

  args.bot_commands = flag_rest_argv();
  args.bot_commands_count = flag_rest_argc();

  if (*args.tournament) {
    RunTournament(*args.map_file, (const char *const *)(args.bot_commands),
                  args.bot_commands_count);
    return 0;
  }
  GameState state =
      MakeGame(*args.map_file, (const char *const *)(args.bot_commands),
               args.bot_commands_count);
  RunGame(&state);

  ComputeGameSpace(state.planets);

  const int screenWidth = 800;
  const int screenHeight = 450;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Planet Wars Viewer");
  font = GetFontDefault();

  SetTargetFPS(60);

  // Set the game to the first turn
  UpdateStateFromLogEntry(&state, state.turn = 0);

  while (!WindowShouldClose()) {
    frame_counter++;
    if (game_running &&
        (game_speed == 0 || frame_counter % abs(game_speed) == 0)) {
      UpdateStateFromLogEntry(&state, state.turn);
      if (playing_forewards && (size_t)state.turn < state.game_log.count)
        state.turn++;
      else if (state.turn > 0)
        state.turn--;
      if ((size_t)state.turn >= state.game_log.count - 1 || state.turn == 0)
        game_running = false;
    }

    if (IsKeyPressed(KEY_RIGHT)) {
      game_running = false;
      state.turn++;
      if ((size_t)state.turn >= state.game_log.count)
        state.turn = state.game_log.count - 1;
      UpdateStateFromLogEntry(&state, state.turn);
    } else if (IsKeyPressed(KEY_LEFT)) {
      game_running = false;
      state.turn--;
      if (state.turn < 0)
        state.turn = 0;
      UpdateStateFromLogEntry(&state, state.turn);
    } else if (IsKeyPressed(KEY_SPACE)) {
      game_running = !game_running;
    }
    if (IsKeyPressed(KEY_UP) && game_speed > 0) {
      game_speed--;
    } else if (IsKeyPressed(KEY_DOWN) && game_speed < MAX_GAME_SPEED_VALUE) {
      game_speed++;
    }

    BeginDrawing();

    ClearBackground(BLACK);

    nob_da_foreach(Planet, planet, &state.planets) { DrawPlanet(*planet); }

    nob_da_foreach(Fleet, fleet, &state.fleets) { DrawFleet(state, *fleet); }

    DrawControls(&state);
    EndDrawing();
  }

  CloseWindow();

  FreeState(&state);
  return 0;
}
