#include "nob.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "clay_renderer_raylib.c"

#include "ui.h"
#include "viewer.h"

#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector)                                 \
  (Clay_Vector2) { .x = vector.x, .y = vector.y }

UIState ui_state;

void HandleClayErrors(Clay_ErrorData errorData) {
  nob_log(NOB_ERROR, "%s", errorData.errorText.chars);
}

void ChangeScreen(enum Screens screen) {
  switch (ui_state.active_screen) {
  default:
    NOB_UNREACHABLE("Tried to switch screen while UI is in impossible state.");
    break;

  case SCREEN_NULL:
    break;

  case SCREEN_VIEWER:
    ViewerDestroy(&ui_state);
    break;
  }

  switch (ui_state.active_screen) {
  default:
  case SCREEN_NULL:
    NOB_UNREACHABLE("Tried to switch to non-existent screen.");
    break;

  case SCREEN_VIEWER:
    ViewerInit(&ui_state, ui_state.game_log);
    break;
  }

  ui_state.active_screen = screen;
}

void UIInit(enum Screens start_screen) {
  ui_state.active_screen = SCREEN_NULL;
  ui_state.game_log = (GameLog){0};

  const int screenWidth = 800;
  const int screenHeight = 450;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Planet Wars Viewer");

  uint32_t totalMemorySize = Clay_MinMemorySize();
  Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
      totalMemorySize, malloc(totalMemorySize));
  Clay_Initialize(clayMemory, (Clay_Dimensions){screenWidth, screenHeight},
                  (Clay_ErrorHandler){HandleClayErrors, 0});
  InitOverlay();

  Font fonts[1] = {GetFontDefault()};
  Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

  ChangeScreen(start_screen);
}

void UIDraw() {
  switch (ui_state.active_screen) {
  default:
  case SCREEN_NULL:
    NOB_UNREACHABLE("Tried to draw non-existent screen.");
    break;

  case SCREEN_VIEWER:
    ViewerDraw(&ui_state);
    break;
  }
}
