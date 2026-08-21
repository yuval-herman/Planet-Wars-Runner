#include "nob.h"

#include "viewer.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "clay_renderer_raylib.c"

#include "ui.h"

#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector)                                 \
  (Clay_Vector2) { .x = vector.x, .y = vector.y }

static const UIScreen *screens[] = {
    [SCREEN_VIEWER] = &viewer_screen,
};
static UIScreen active_screen = {0};

void HandleClayErrors(Clay_ErrorData errorData) {
  nob_log(NOB_ERROR, "%s", errorData.errorText.chars);
}

void ChangeScreen(enum Screens screen, void *screen_params) {
  assert(screen > SCREEN_NULL && screen < NOB_ARRAY_LEN(screens));

  if (active_screen.destroy)
    active_screen.destroy();

  active_screen = *screens[screen];

  if (active_screen.init)
    active_screen.init(screen_params);
}

void UIInit(enum Screens start_screen, void *screen_params) {
  const int screenWidth = 800;
  const int screenHeight = 450;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Planet Wars Viewer");

  SetTargetFPS(60);

  uint32_t totalMemorySize = Clay_MinMemorySize();
  Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
      totalMemorySize, malloc(totalMemorySize));
  Clay_Initialize(clayMemory, (Clay_Dimensions){screenWidth, screenHeight},
                  (Clay_ErrorHandler){HandleClayErrors, 0});
  InitOverlay();

  Font fonts[1] = {GetFontDefault()};
  Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

  ChangeScreen(start_screen, screen_params);
}

void UIDestroy() {
  if (active_screen.destroy)
    active_screen.destroy();
  active_screen = (UIScreen){0};
}

void UIRun() {
  assert(active_screen.draw);
  while (!WindowShouldClose()) {
    active_screen.draw();
  }
}
