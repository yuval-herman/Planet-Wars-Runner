#include "clay.h"
#include "nob.h"
#include "raylib.h"

#include "../ui_utils.h"


bool Component_MessageBox(Clay_String text);

// Don't include implementation when included from the components header file
#ifndef COMPONENTS_H

bool Component_MessageBox(Clay_String text) {
  bool clicked = false;
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = CLAY_PADDING_ALL(32),
      .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
      .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
    },
    .cornerRadius = CLAY_CORNER_RADIUS(8),
    .border = {.color = C_WHITE, .width = CLAY_BORDER_OUTSIDE(2)},
    .backgroundColor = C_BLACK,
    .floating = {
      .attachPoints = {CLAY_ATTACH_POINT_CENTER_CENTER,CLAY_ATTACH_POINT_CENTER_CENTER},
      .attachTo = CLAY_ATTACH_TO_PARENT,
    },
  }) {
    if(Clay_Hovered() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = true;
    CLAY_TEXT(text, {
      .fontId = 1,
      .fontSize = 16,
      .textColor = C_WHITE,
    });
  }
  // clang-format on
  return clicked;
}

#endif // COMPONENTS_H
