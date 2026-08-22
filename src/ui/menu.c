#include "clay.h"
#include "raylib.h"

#include "nob.h"

#include "menu.h"

// Convenient shorthand for Raylib -> Clay color conversion.
#define CLAY_COLOR(color)                                                      \
  (Clay_Color) { .r = color.r, .g = color.g, .b = color.b, .a = color.a }

void MenuButton(Clay_String buttonText) {
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = CLAY_PADDING_ALL(8),
      .sizing = {.width = CLAY_SIZING_GROW(0)}
    },
    .backgroundColor = CLAY_COLOR(GRAY)
  }) {
      CLAY_TEXT(buttonText, {.fontSize = 32, .textColor = CLAY_COLOR(BLACK)});
  }
  // clang-format on
}

void MenuInit(void *params) {
  NOB_UNUSED(params);
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
      MenuButton(CLAY_STRING("play match"));
      MenuButton(CLAY_STRING("Replay match"));
    }
  }
  // clang-format on
}

void MenuDestroy() {}

const UIScreen menu_screen = {
    .init = MenuInit,
    .draw = MenuDraw,
    .destroy = MenuDestroy,
};
