#include "../nob.h"
#include "raylib.h"
#include "raymath.h"

#define BASE_PLANET_RADIUS 10
#define PLANET_COLOR_NEUTRAL GRAY

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

Font font;

void DrawPlanet(Planet planet) {
  DrawCircleV(planet.coords, BASE_PLANET_RADIUS * planet.growth, GRAY);

  const float font_size = 10 * planet.growth;
  const float spacing = 1;
  const char *ships_text = TextFormat("%d", planet.ships);

  const Vector2 text_measurements =
      MeasureTextEx(font, ships_text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(planet.coords, Vector2Scale(text_measurements, 0.5));

  DrawTextEx(font, ships_text, text_coords, font_size, spacing, BLACK);
}

int main(void) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int screenWidth = 800;
  const int screenHeight = 450;

  InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");
  font = GetFontDefault();

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  const Planet planets[] = {
      {.coords = {.x = 100, .y = 100}, .owner = 0, .ships = 50, .growth = 5}};

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    for (size_t i = 0; i < NOB_ARRAY_LEN(planets); i++) {
      DrawPlanet(NOB_ARRAY_GET(planets, i));
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
