#include "clay.h"
#include "nob.h"
#include "raylib.h"

#include "../ui_utils.h"

// Displays an editable text box. Only a single line is supported.
// If `sb` contains some data prior to calling this function, it will be
// displayed in the text box.
// The return value is a clay string pointing to the `sb` data for convenience.
Clay_String Component_TextEdit(Nob_String_Builder *sb, unsigned *caret_pos,
                               bool *is_active);

// Don't include implementation when included from the componenets header file
#ifndef COMPONENTS_H

#define CARET_COLOR ((Clay_Color){255, 255, 255, 127})
#define BACKGROUND_COLOR ((Clay_Color){255, 255, 255, 20})
#define BORDER_COLOR ((Clay_Color){255, 255, 255, 30})
#define TEXT_COLOR ((Clay_Color){255, 255, 255, 255})
#define FONT_SIZE 16

static void handleClick(Clay_ElementId element_id, unsigned character_width,
                        bool *is_focused, unsigned *caret_pos) {
  *is_focused = true;

  Clay_ElementData element_data = Clay_GetElementData(element_id);
  assert(element_data.found);

  Vector2 ep = {element_data.boundingBox.x, element_data.boundingBox.y};
  Vector2 mp = GetMousePosition();
  *caret_pos = (mp.x - ep.x) / character_width;
}

static inline void SetCursorShapeByHover() {}

Clay_String DrawTextEdit(Nob_String_Builder sb, unsigned *caret_pos,
                         bool *is_focused) {
  static uint32_t element_hover_id = 0;
  const Clay_String str = SB_TO_CLAY(sb);
  Clay_String str_to_caret = (Clay_String){
      .isStaticallyAllocated = false,
      .length = *caret_pos,
      .chars = sb.items,
  };
  const int inner_padding = 8;
  Clay_TextElementConfig text_conf = {
      .fontId = 1,
      .fontSize = FONT_SIZE,
      .textColor = TEXT_COLOR,
  };
  // This will always work for monotone fonts, but at best will be a good
  // estimate otherwise
  const unsigned character_width =
      Clay_MeasureText(&(Clay_String){false, 1, sb.items}, &text_conf).width;
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = CLAY_PADDING_ALL(inner_padding),
      .sizing = {.width = CLAY_SIZING_GROW(0)}
    },
    .backgroundColor = BACKGROUND_COLOR,
    .border = {.color = BORDER_COLOR, .width = CLAY_BORDER_OUTSIDE(1)},
    .cornerRadius = CLAY_CORNER_RADIUS(5),
  }) {
    // clang-format on
    if (Clay_Hovered()) {
      element_hover_id = Clay_GetOpenElementId();
      if (GetMouseCursor() != MOUSE_CURSOR_IBEAM)
        SetMouseCursor(MOUSE_CURSOR_IBEAM);
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        handleClick((Clay_ElementId){.id = element_hover_id}, character_width,
                    is_focused, caret_pos);
    } else {
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        *is_focused = false;
      // If this element was the one that set the cursor originally, return it
      // to normal, if not, ignore it.
      if (element_hover_id == Clay_GetOpenElementId()) {
        element_hover_id = 0;
        if (GetMouseCursor() != MOUSE_CURSOR_DEFAULT)
          SetMouseCursor(MOUSE_CURSOR_DEFAULT);
      }
    }
    // clang-format off

    CLAY_TEXT(str, text_conf);
    if (*is_focused) {
      CLAY(CLAY_ID_LOCAL("Caret"), {
        .floating = {
          .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
          .attachTo = CLAY_ATTACH_TO_PARENT,
          .attachPoints = {CLAY_ATTACH_POINT_LEFT_CENTER, CLAY_ATTACH_POINT_LEFT_CENTER},
          .offset = {
            .y = 0,
            .x = inner_padding + Clay_MeasureText(&str_to_caret, &text_conf).width
          }
        },
        .layout = {
          .sizing = {
            .height = CLAY_SIZING_FIXED(FONT_SIZE - inner_padding/2.0f + 2),
            .width = CLAY_SIZING_FIXED(2)
          },
        },
        .backgroundColor = CARET_COLOR
      }) {}
    }
  }
  // clang-format on
  return str;
}

Clay_String Component_TextEdit(Nob_String_Builder *sb, unsigned *caret_pos,
                               bool *is_focused) {
  if (*is_focused) {
    if (IsKeyPressed(KEY_RIGHT) && *caret_pos < sb->count)
      (*caret_pos)++;
    if (IsKeyPressed(KEY_LEFT) && *caret_pos > 0)
      (*caret_pos)--;

    if (IsKeyPressed(KEY_BACKSPACE) && *caret_pos > 0) {
      for (unsigned i = *caret_pos - 1; i < sb->count; i++) {
        sb->items[i] = sb->items[i + 1];
      }
      sb->count--;
      (*caret_pos)--;
    }
    if (IsKeyPressed(KEY_DELETE) && *caret_pos < sb->count) {
      for (unsigned i = *caret_pos; i < sb->count; i++) {
        sb->items[i] = sb->items[i + 1];
      }
      sb->count--;
    }

    int key = GetCharPressed();

    // Check if more characters have been pressed on the same frame
    while (key > 0) {
      if (key >= 32 && key <= 126) {
        nob_da_reserve(sb, sb->count + 1);

        for (unsigned i = sb->count; i > *caret_pos; i--) {
          sb->items[i] = sb->items[i - 1];
        }
        sb->count++;
        sb->items[(*caret_pos)++] = (char)key;
      }

      key = GetCharPressed(); // Check next character in the queue
    }
  }
  return DrawTextEdit(*sb, caret_pos, is_focused);
}
#endif // COMPONENTS_H
