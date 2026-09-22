#include "nob.h"

#include "clay_renderer_raylib.c"
#include "fonts.h"

#include "human_game.h"
#include "menu.h"
#include "viewer.h"

#ifdef WASM_MODE
#include <emscripten/emscripten.h>
#endif

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "ui.h"

static const UIScreen *screens[] = {
    [SCREEN_VIEWER] = &viewer_screen,
    [SCREEN_MENU] = &menu_screen,
    [SCREEN_HUMAN_GAME] = &human_game_screen,
};

static const unsigned char *font_lut[] = {
    [FiraCode_Bold] = FiraCode_Bold_source,
    [FiraCode_Regular] = FiraCode_Regular_source,
    [Cousine_Regular] = Cousine_Regular_source,
};

static const unsigned font_size_lut[] = {
    [FiraCode_Bold] = FiraCode_Bold_size,
    [FiraCode_Regular] = FiraCode_Regular_size,
    [Cousine_Regular] = Cousine_Regular_size,
};

static UIScreen active_screen = {0};
static FontsDA fonts = {0};
static unsigned frame;

unsigned GetFontId(enum Fonts font_id, unsigned size) {
  // Default font is always the same for every size, and is loaded at the start
  // of the array.
  if (font_id == RaylibDefault)
    return 0;

  for (unsigned i = 0; i < fonts.count; i++) {
    FontInfo font_info = fonts.items[i];
    if (font_info.font_id == font_id && font_info.font_size == size)
      return i;
  }

  nob_log(NOB_DEBUG, "Adding new font, id = %d, size = %u", font_id, size);

  FontInfo new_font = {
      .font = LoadFontFromMemory(".ttf", font_lut[font_id],
                                 font_size_lut[font_id], 64, NULL, 0),
      .font_id = font_id,
      .font_size = size,
  };
  nob_da_append(&fonts, new_font);
  return fonts.count - 1;
}

unsigned GetFrame() { return frame; }

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
  SetExitKey(KEY_NULL);

#ifndef WASM_MODE
  SetTargetFPS(60);
#endif

  FontInfo default_font_info = {
      .font = GetFontDefault(),
      .font_id = RaylibDefault,
      .font_size = 0,
  };
  nob_da_append(&fonts, default_font_info);

  uint32_t totalMemorySize = Clay_MinMemorySize();
  Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(
      totalMemorySize, malloc(totalMemorySize));
  Clay_Initialize(clayMemory, (Clay_Dimensions){screenWidth, screenHeight},
                  (Clay_ErrorHandler){HandleClayErrors, 0});
  InitOverlay();

  Clay_SetMeasureTextFunction(Raylib_MeasureText, &fonts);

  UpdateClayState();
  ChangeScreen(start_screen);
}

void UIDestroy() {
  if (active_screen.destroy)
    active_screen.destroy();
  active_screen = (UIScreen){0};

  Clay_Raylib_Close();
  nob_da_free(fonts);
}

static void UpdateDrawUI() {
  frame++;
  UpdateClayState();

  BeginDrawing();
  Clay_BeginLayout();
  active_screen.draw();
  Clay_RenderCommandArray renderCommands = Clay_EndLayout(GetFrameTime());
  Clay_Raylib_Render(renderCommands, &fonts);
  EndDrawing();
}

void UIRun() {
  assert(active_screen.draw);
#ifdef WASM_MODE
  emscripten_set_main_loop(UpdateDrawUI, 0, 1);
#else
  while (!WindowShouldClose()) {
    UpdateDrawUI();
  }
#endif
}
