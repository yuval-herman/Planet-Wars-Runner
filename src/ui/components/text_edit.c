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

struct HoverData {
  // Width of a single character in the text edit component
  unsigned character_width;
  bool *is_focused;
  unsigned *caret_pos;
};

static void onHover(Clay_ElementId element_id, Clay_PointerData pointer_data,
                    void *user_data) {
  struct HoverData *hover_data = user_data;

  if (pointer_data.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    *hover_data->is_focused = true;

    Clay_ElementData element_data = Clay_GetElementData(element_id);
    assert(element_data.found);

    Vector2 ep = {element_data.boundingBox.x, element_data.boundingBox.y};
    Vector2 mp = GetMousePosition();
    *hover_data->caret_pos = (mp.x - ep.x) / hover_data->character_width;
  }
}

static inline void SetCursorShapeByHover() {
  if (Clay_Hovered()) {
    if (GetMouseCursor() != MOUSE_CURSOR_IBEAM)
      SetMouseCursor(MOUSE_CURSOR_IBEAM);
  } else {
    if (GetMouseCursor() != MOUSE_CURSOR_DEFAULT)
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  }
}

Clay_String DrawTextEdit(Nob_String_Builder sb, unsigned *caret_pos,
                         bool *is_focused) {
  const Clay_String str = SB_TO_CLAY(sb);
  Clay_String str_to_caret = (Clay_String){
      .isStaticallyAllocated = false,
      .length = *caret_pos,
      .chars = sb.items,
  };
  const int inner_padding = 8;
  Clay_TextElementConfig text_conf = {
      .fontId = 1,
      .fontSize = 32,
      .textColor = CLAY_COLOR(BLACK),
  };
  static struct HoverData hover_data;
  hover_data.caret_pos = caret_pos;
  hover_data.is_focused = is_focused;
  // This will always work for monotone fonts, but at best will be a good
  // estimate otherwise
  hover_data.character_width =
      Clay_MeasureText(&(Clay_String){false, 1, sb.items}, &text_conf).width;
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = CLAY_PADDING_ALL(inner_padding),
      .sizing = {.width = CLAY_SIZING_GROW(0)}
    },
    .backgroundColor = *is_focused ? CLAY_COLOR(BLUE) : CLAY_COLOR(GRAY)
  }) {
    SetCursorShapeByHover();
    Clay_OnHover(onHover, &hover_data);
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
          .sizing = {.height = CLAY_SIZING_PERCENT(0.9), .width = CLAY_SIZING_FIXED(4)},
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
