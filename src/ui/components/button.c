#include "clay.h"
#include "nob.h"
#include "raylib.h"

#include "../ui_utils.h"

typedef void (*ButtonOnClickFunction)(void *userData);

bool Component_Button(Clay_String buttonText, bool grow);

// Don't include implementation when included from the componenets header file
#ifndef COMPONENTS_H

struct HoverData {
  ButtonOnClickFunction Fn;
  void *userData;
};

bool Component_Button(Clay_String buttonText, bool grow) {
  bool clicked = false;
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = {32, 32, 16, 16},
      .sizing = {.width = grow ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0)},
      .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
    },
    .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
    .border = {.color = CLAY_COLOR(WHITE), .width = CLAY_BORDER_OUTSIDE(2) },
    .backgroundColor = Clay_Hovered() ? CLAY_COLOR(WHITE) : (Clay_Color){0}
  }) {
    if(Clay_Hovered() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = true;
    CLAY_TEXT(buttonText, {
      .fontId = 2,
      .fontSize = 24,
      .textColor = Clay_Hovered() ? CLAY_COLOR(BLACK) : CLAY_COLOR(WHITE),
    });
  }
  // clang-format on
  return clicked;
}

#endif // COMPONENTS_H
