#include "clay.h"
#include "nob.h"
#include "raylib.h"

#include "../ui_utils.h"

enum ButtonStyle {
  BUTTON_STYLE_MENU = 0,
  BUTTON_STYLE_SUB_MENU,
  BUTTON_STYLE_SETTINGS,
  BUTTON_STYLE_CONTROLLER,
  BUTTON_STYLE_CONTROLLER_NAKED,
  BUTTON_STYLE__COUNT,
};

typedef void (*ButtonOnClickFunction)(void *userData);

bool Component_Button(Clay_String buttonText, enum ButtonStyle style);

// Don't include implementation when included from the componenets header file
#ifndef COMPONENTS_H

struct HoverData {
  ButtonOnClickFunction Fn;
  void *userData;
};

#define DYNAMIC_STYLES(styles)                                                 \
  struct styles;                                                               \
  struct styles hovered;

struct ButtonStyleConfig {
  // Static styles
  Clay_Padding padding;
  Clay_Sizing sizing;
  Clay_CornerRadius cornerRadius;
  Clay_BorderElementConfig border;
  uint16_t fontId;
  uint16_t fontSize;

  // Dynamic styles
  DYNAMIC_STYLES({
    Clay_Color backgroundColor;
    Clay_Color textColor;
  })
} button_styles[] = {
    [BUTTON_STYLE_MENU] =
        {
            .padding = {32, 32, 16, 16},
            .sizing = {.width = CLAY_SIZING_GROW(0)},
            .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
            .border = {.color = C_WHITE, .width = CLAY_BORDER_OUTSIDE(2)},
            .fontId = 2,
            .fontSize = 24,

            .backgroundColor = C_BLANK,
            .textColor = C_WHITE,
            .hovered =
                {
                    .backgroundColor = C_WHITE,
                    .textColor = C_BLACK,
                },
        },
    [BUTTON_STYLE_SUB_MENU] =
        {
            .padding = {32, 32, 8, 8},
            .sizing = {.width = CLAY_SIZING_FIT(0)},
            .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
            .border = {.color = C_WHITE, .width = CLAY_BORDER_OUTSIDE(2)},
            .fontId = 2,
            .fontSize = 24,

            .backgroundColor = C_BLANK,
            .textColor = C_WHITE,
            .hovered =
                {
                    .backgroundColor = C_WHITE,
                    .textColor = C_BLACK,
                },
        },
    [BUTTON_STYLE_SETTINGS] =
        {
            .padding = {16, 16, 8, 8},
            .sizing = {.width = CLAY_SIZING_FIT(0)},
            .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
            .border = {.color = C_GRAY, .width = CLAY_BORDER_OUTSIDE(1)},
            .fontId = 1,
            .fontSize = 16,

            .backgroundColor = C_BLANK,
            .textColor = C_WHITE,
            .hovered =
                {
                    .backgroundColor = C_WHITE,
                    .textColor = C_BLACK,
                },
        },
    [BUTTON_STYLE_CONTROLLER] =
        {
            .padding = {16, 16, 8, 8},
            .sizing = {.width = CLAY_SIZING_FIT(0)},
            .cornerRadius = CLAY_CORNER_RADIUS(8),
            .border = {.color = C_WHITE, .width = CLAY_BORDER_OUTSIDE(1)},
            .fontId = 1,
            .fontSize = 16,

            .backgroundColor = C_BLANK,
            .textColor = C_WHITE,
            .hovered =
                {
                    .backgroundColor = C_WHITE,
                    .textColor = C_BLACK,
                },
        },
    [BUTTON_STYLE_CONTROLLER_NAKED] =
        {
            .padding = {16, 16, 8, 8},
            .sizing = {.width = CLAY_SIZING_FIT(0)},
            .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
            .fontId = 1,
            .fontSize = 16,

            .backgroundColor = C_BLANK,
            .textColor = C_WHITE,
            .hovered =
                {
                    .backgroundColor = C_WHITE,
                    .textColor = C_BLACK,
                },
        },
};

/**
  Styles that can changes statically:
   - padding
   - sizing
   - cornerRadius
   - border
   - fontId
   - fontSize

  Styles that can change dynamically (depened on runtime values like hover):
   - backgroundColor
   - textColor
  */

bool Component_Button(Clay_String buttonText, enum ButtonStyle style) {
  assert(style < BUTTON_STYLE__COUNT);
  bool clicked = false;
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = button_styles[style].padding,
      .sizing = button_styles[style].sizing,
      .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
    },
    .cornerRadius = button_styles[style].cornerRadius,
    .border = button_styles[style].border,
    .backgroundColor = Clay_Hovered() ? button_styles[style].hovered.backgroundColor : button_styles[style].backgroundColor
  }) {
    if(Clay_Hovered() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = true;
    CLAY_TEXT(buttonText, {
      .fontId = button_styles[style].fontId,
      .fontSize = button_styles[style].fontSize,
      .textColor = Clay_Hovered() ? button_styles[style].hovered.textColor : button_styles[style].textColor,
    });
  }
  // clang-format on
  return clicked;
}

#endif // COMPONENTS_H
