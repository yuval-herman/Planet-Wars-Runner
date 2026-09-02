#include "nob.h"

#include "clay_renderer_raylib.c"
#include "fonts.h"

#include "menu.h"
#include "viewer.h"
#include "human_game.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "ui.h"

static const UIScreen *screens[] = {
    [SCREEN_VIEWER] = &viewer_screen,
    [SCREEN_MENU] = &menu_screen,
    [SCREEN_HUMAN_GAME] = &human_game_screen,
};

static UIScreen active_screen = {0};
static Font fonts[4];
static unsigned frame;

unsigned GetFrame() {
  return frame;
}

void HandleClayErrors(Clay_ErrorData errorData) {
  nob_log(NOB_ERROR, "%s", errorData.errorText.chars);
}

void ChangeScreen(enum Screens screen) {
  assert(screen > SCREEN_NULL && screen < NOB_ARRAY_LEN(screens));

  if (active_screen.destroy)
    active_screen.destroy();

  active_screen = *screens[screen];

  if (active_screen.init)
    active_screen.init();
}

void UpdateClayState() {
  Clay_SetLayoutDimensions(
      (Clay_Dimensions){GetScreenWidth(), GetScreenHeight()});
  Vector2 mp = GetMousePosition();
  Clay_SetPointerState((Clay_Vector2){mp.x, mp.y},
                       IsMouseButtonDown(MOUSE_LEFT_BUTTON));
  Vector2 mw = GetMouseWheelMoveV();
  Clay_UpdateScrollContainers(true, (Clay_Vector2){mw.x, mw.y}, GetFrameTime());
}

void UIInit(enum Screens start_screen) {
  const int screenWidth = 1200;
  const int screenHeight = 600;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Planet Wars Viewer");

  SetTargetFPS(60);

  fonts[0] = GetFontDefault();
  fonts[1] = LoadFontFromMemory(".ttf", Cousine_Regular_source,
                                Cousine_Regular_size, 64, NULL, 0);
  fonts[2] = LoadFontFromMemory(".ttf", FiraCode_Regular_source,
                                FiraCode_Regular_size, 64, NULL, 0);
  fonts[3] = LoadFontFromMemory(".ttf", FiraCode_Bold_source,
                                FiraCode_Bold_size, 64, NULL, 0);

  uint32_t totalMemorySize = Clay_MinMemorySize();
  Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
      totalMemorySize, malloc(totalMemorySize));
  Clay_Initialize(clayMemory, (Clay_Dimensions){screenWidth, screenHeight},
                  (Clay_ErrorHandler){HandleClayErrors, 0});
  InitOverlay();

  Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

  UpdateClayState();
  ChangeScreen(start_screen);
}

void UIDestroy() {
  if (active_screen.destroy)
    active_screen.destroy();
  active_screen = (UIScreen){0};

  Clay_Raylib_Close();
}

void UIRun() {
  assert(active_screen.draw);
  while (!WindowShouldClose()) {
    frame++;
    UpdateClayState();

    BeginDrawing();
    Clay_BeginLayout();
    active_screen.draw();
    Clay_RenderCommandArray renderCommands = Clay_EndLayout(GetFrameTime());
    Clay_Raylib_Render(renderCommands, fonts);
    EndDrawing();
  }
}
