#include "clay.h"
#include "raylib.h"

#include "../configs.h"
#include "../game.h"
#include "../game_log.h"
#include "../runner.h"
#include "nob.h"

#include "menu.h"
#include "ui.h"
#include "viewer.h"

// Convenient shorthand for Raylib -> Clay color conversion.
#define CLAY_COLOR(color)                                                      \
  (Clay_Color) { .r = color.r, .g = color.g, .b = color.b, .a = color.a }

enum ButtonFunction : size_t {
  BUTTON_PLAY_MATCH,
  BUTTON_REPLAY,
};

static Configs *configs = NULL;

void MenuButtonHoverFunction(Clay_ElementId element_id,
                             Clay_PointerData pointer_data, void *user_data) {
  NOB_UNUSED(element_id);
  if (pointer_data.state != CLAY_POINTER_DATA_RELEASED_THIS_FRAME)
    return;

  assert(configs);

  enum ButtonFunction button_function = (size_t)user_data;

  GameLog game_log = {0};

  switch (button_function) {
  default:
    NOB_UNREACHABLE("impossible button function");

  case BUTTON_REPLAY: {
    FILE *save_file = fopen(configs->save_file, "rb");
    if (!save_file) {
      nob_log(NOB_ERROR, "Failed opening save file \"%s\": %s.",
              configs->save_file, strerror(errno));
      NOB_TODO("handle errors isn't implemented...");
    } else {
      if (!ReadGameLogFromFile(save_file, &game_log)) {
        nob_log(NOB_ERROR, "Failed reading \"%s\".", configs->save_file);
      } else {
        SetGameLog(game_log);
        ChangeScreen(SCREEN_VIEWER);
      }
      fclose(save_file);
    }
  } break;
  case BUTTON_PLAY_MATCH: {
    GameState state = {0};
    if (MakeGame(&state, configs->map_file, configs->players.count)) {
      if (!RunMatch(&game_log, &state, configs->players)) {
        nob_log(NOB_ERROR, "Failed running match.");
        NOB_TODO("handle errors isn't implemented...");
      }
      if (configs->write_save) {
        FILE *file = fopen("game.plws", "wb");
        WriteGameLogToFile(file, game_log);
        fclose(file);
      }

      SetGameLog(game_log);
      ChangeScreen(SCREEN_VIEWER);
    } else {
      NOB_TODO("handle errors isn't implemented...");
    }

    FreeInnerGameState(state);

  } break;
  }
  // TODO: How do I free this? Need to think about architecture here
  // FreeInnerGameLog(*game_log);
  // free(game_log);
}

void MenuButton(Clay_String buttonText, enum ButtonFunction button_function) {
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = CLAY_PADDING_ALL(8),
      .sizing = {.width = CLAY_SIZING_GROW(0)}
    },
    .backgroundColor = Clay_Hovered() ? CLAY_COLOR(BLUE) : CLAY_COLOR(GRAY)
  }) {
    Clay_OnHover(MenuButtonHoverFunction, (void*)button_function);
    CLAY_TEXT(buttonText, {.fontSize = 32, .textColor = CLAY_COLOR(BLACK)});
  }
  // clang-format on
}

void MenuInit() {
  assert(configs);
  // Clay_SetDebugModeEnabled(true);
}

void MenuDraw() {
  // clang-format off
  CLAY(CLAY_ID("OuterContainer"), {
       .layout = {
         .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
         .padding = CLAY_PADDING_ALL(32),
         .childGap = 16,
         .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
       },
       .backgroundColor = CLAY_COLOR(RAYWHITE)
   }) {
    CLAY_TEXT(CLAY_STRING("Planet Wars"), {.fontSize = 64, .textColor = CLAY_COLOR(BLACK)});
    CLAY(CLAY_ID("OptionsContainer"), {
      .layout = {
        .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)},
        .padding = CLAY_PADDING_ALL(32),
        .childGap = 16,
        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_BOTTOM},
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
       },
    }) {
      MenuButton(CLAY_STRING("play match"), BUTTON_PLAY_MATCH);
      MenuButton(CLAY_STRING("Replay match"), BUTTON_REPLAY);
    }
  }
  // clang-format on
}

void MenuDestroy() { configs = NULL; }

void SetConfig(Configs *new_configs) { configs = new_configs; }

const UIScreen menu_screen = {
    .init = MenuInit,
    .draw = MenuDraw,
    .destroy = MenuDestroy,
};
