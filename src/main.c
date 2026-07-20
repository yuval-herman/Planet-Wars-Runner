#include "../nob.h"
#include "raylib.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define BASE_PLANET_RADIUS 10.0f
#define PLANET_RADIUS_GROWTH_CURVE 20.0f
#define MAP_MARGIN 50

const Color planet_colors[] = {GRAY, RED, GREEN, BLUE, YELLOW};

typedef struct {
  Vector2 coords;
  int owner;
  int ships;
  int growth;
} Planet;

typedef struct {
  int owner;
  int ships;
  int src_id;
  int dst_id;
  int total;
  int remaining;
} Fleet;

typedef struct {
  Planet *items;
  size_t count;
  size_t capacity;
} PlanetDA;

typedef struct {
  Fleet *items;
  size_t count;
  size_t capacity;
} FleetDA;

Font font;

void PrintPlanet(Planet planet) {
  printf("P %f %f %d %d %d\n", planet.coords.x, planet.coords.y, planet.owner,
         planet.ships, planet.growth);
}

// Draws a Planet on the screen.
// `min_coords` and `max_corrds` are the smallest and largest (x, y) coordinates
// on the map. This values are used to make sure planets are spaced out to fill
// the entire screen.
void DrawPlanet(Planet planet, Vector2 min_coords, Vector2 max_coords) {
  Vector2 draw_coords = {
      .x = Remap(planet.coords.x, min_coords.x, max_coords.x, MAP_MARGIN,
                 GetScreenWidth() - MAP_MARGIN),
      .y =
          GetScreenHeight() - Remap(planet.coords.y, min_coords.y, max_coords.y,
                                    MAP_MARGIN, GetScreenHeight() - MAP_MARGIN),
  };

  const float draw_radius = BASE_PLANET_RADIUS + PLANET_RADIUS_GROWTH_CURVE -
                            PLANET_RADIUS_GROWTH_CURVE / planet.growth;

  DrawCircleV(draw_coords, draw_radius, planet_colors[planet.owner]);

  const float font_size = draw_radius;
  const float spacing = 1;
  const char *ships_text = TextFormat("%d", planet.ships);

  const Vector2 text_measurements =
      MeasureTextEx(font, ships_text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(draw_coords, Vector2Scale(text_measurements, 0.5));

  DrawTextEx(font, ships_text, text_coords, font_size, spacing, BLACK);
}

// Parses a float, advances s to the end of the parsed string, sets out to the
// parsed value. Returns true on success false otherwise.
bool parse_float(char **s, float *out) {
  char *end;
  errno = 0;
  float v = strtof(*s, &end);
  if (end == *s || errno == ERANGE || !isfinite(v))
    return false;
  *s = end;
  *out = v;
  return true;
}

// Parses a int, advances s to the end of the parsed string, sets out to the
// parsed value. Returns true on success false otherwise.
bool parse_int(char **s, int *out) {
  char *end;
  long v = strtol(*s, &end, 10);
  if (end == *s || v < INT_MIN || v > INT_MAX)
    return false;
  *s = end;
  *out = (int)v;
  return true;
}

bool ParseMapFile(const char *map_path, PlanetDA *planets) {
  FILE *map_file = fopen(map_path, "r");
  if (!map_file) {
    perror("Failed loading map file");
    return false;
  }

  char buf[256];
  size_t file_line = 0;

  while (fgets(buf, sizeof buf, map_file)) {
    file_line += 1;
    size_t len = strlen(buf);
    if (buf[len - 1] != '\n') {
      fprintf(stderr, "Map file contains lines longer then 256 characters and "
                      "cannot be read.\n");
      return false;
    }
    // TODO support pre-existing fleet lines
    if (buf[0] != 'P')
      continue;

    char *s_idx = buf + 1;
    Planet planet;
    if (!(parse_float(&s_idx, &planet.coords.x) &&
          parse_float(&s_idx, &planet.coords.y) &&
          parse_int(&s_idx, &planet.owner) &&
          parse_int(&s_idx, &planet.ships) &&
          parse_int(&s_idx, &planet.growth))) {
      fprintf(stderr, "Invalid map file.\nSyntax error at line %zu.\n",
              file_line);
      return false;
    }

    // TODO support arbitrary amount of players, or a reasonably large number
    if (planet.owner < 0 || planet.owner > NOB_ARRAY_LEN(planet_colors)) {
      fprintf(stderr, "Invalid number of player owned planets. The must be "
                      "between 1 to 4 players.\n");
      return false;
    }

    nob_da_append(planets, planet);
  }

  fclose(map_file);
  return true;
}

int main(int argc, char *argv[]) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int screenWidth = 800;
  const int screenHeight = 450;

  PlanetDA planets = {0};
  Vector2 min_coords = {INFINITY, INFINITY},
          max_coords = {-INFINITY, -INFINITY};

  if (argc < 2) {
    fprintf(stderr, "ERROR: No <map_file> provided.\n");
    fprintf(stderr, "Usage: %s <map_file>\n", argv[0]);
    return 1;
  }

  printf("Loading map file from %s.\n", argv[1]);
  if (!ParseMapFile(argv[1], &planets))
    return 1;

  nob_da_foreach(Planet, planet, &planets) {
    const Vector2 coords = planet->coords;
    min_coords = Vector2Min(planet->coords, min_coords);
    max_coords = Vector2Max(planet->coords, max_coords);
  }

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
  font = GetFontDefault();

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    nob_da_foreach(Planet, planet, &planets) {
      DrawPlanet(*planet, min_coords, max_coords);
    }

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
