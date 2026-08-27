#include "clay.h"
#include "nob.h"
#include "raylib.h"

#include "../ui_utils.h"

// Displays an editable text box.
// If `sb` contains some data prior to calling this function, it will be
// displayed in the text box.
// The return value is a clay string pointing to the `sb` data for convenience.
Clay_String Component_TextEdit(Nob_String_Builder *sb, unsigned *cursor_pos,
                               bool *is_active);

// Don't include implementation when included from the componenets header file
#ifndef COMPONENTS_H

#define CURSOR_COLOR ((Clay_Color){255, 255, 255, 127})

static void onHover(Clay_ElementId element_id, Clay_PointerData pointer_data,
                    void *user_data) {
  NOB_UNUSED(element_id);
  bool *is_focused = user_data;

  if (pointer_data.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    *is_focused = true;
  }
}

static inline void SetCursorByHover() {
  if (Clay_Hovered()) {
    if (GetMouseCursor() != MOUSE_CURSOR_IBEAM)
      SetMouseCursor(MOUSE_CURSOR_IBEAM);
  } else {
    if (GetMouseCursor() != MOUSE_CURSOR_DEFAULT)
      SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  }
}

Clay_String DrawTextEdit(Nob_String_Builder sb, unsigned *cursor_pos,
                         bool *is_focused) {
  const Clay_String str = SB_TO_CLAY(sb);
  const int inner_padding = 8;
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = CLAY_PADDING_ALL(inner_padding),
      .sizing = {.width = CLAY_SIZING_GROW(0)}
    },
    .backgroundColor = *is_focused ? CLAY_COLOR(BLUE) : CLAY_COLOR(GRAY)
  }) {
    SetCursorByHover();
    Clay_OnHover(onHover, is_focused);
    Clay_TextElementConfig text_conf = {.fontId = 1, .fontSize = 32, .textColor = CLAY_COLOR(BLACK)};
    CLAY_TEXT(str, text_conf);
    if (*is_focused) {
      CLAY(CLAY_ID_LOCAL("Cursor"), {
        .floating = {
          .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
          .attachTo = CLAY_ATTACH_TO_PARENT,
          .attachPoints = {CLAY_ATTACH_POINT_LEFT_CENTER, CLAY_ATTACH_POINT_LEFT_CENTER},
          .offset = {.y = 0, .x = inner_padding + *cursor_pos * 16}
        },
        .layout = {
          .sizing = {.height = CLAY_SIZING_PERCENT(0.9), .width = CLAY_SIZING_FIXED(4)},
        },
        .backgroundColor = CURSOR_COLOR
      }) {}
    }
  }
  // clang-format on
  return str;
}

Clay_String Component_TextEdit(Nob_String_Builder *sb, unsigned *cursor_pos,
                               bool *is_focused) {
  if (*is_focused) {
    if (IsKeyPressed(KEY_RIGHT) && *cursor_pos < sb->count)
      (*cursor_pos)++;
    if (IsKeyPressed(KEY_LEFT) && *cursor_pos > 0)
      (*cursor_pos)--;

    if (IsKeyPressed(KEY_BACKSPACE) && *cursor_pos > 0) {
      for (unsigned i = *cursor_pos - 1; i < sb->count; i++) {
        sb->items[i] = sb->items[i + 1];
      }
      sb->count--;
      (*cursor_pos)--;
    }
    if (IsKeyPressed(KEY_DELETE) && *cursor_pos < sb->count) {
      for (unsigned i = *cursor_pos; i < sb->count; i++) {
        sb->items[i] = sb->items[i + 1];
      }
      sb->count--;
    }

    int key = GetCharPressed();

    // Check if more characters have been pressed on the same frame
    while (key > 0) {
      if (key >= 32 && key <= 126) {
        nob_da_reserve(sb, sb->count + 1);

        for (unsigned i = sb->count; i > *cursor_pos; i--) {
          sb->items[i] = sb->items[i - 1];
        }
        sb->count++;
        sb->items[(*cursor_pos)++] = (char)key;
      }

      key = GetCharPressed(); // Check next character in the queue
    }
  }
  return DrawTextEdit(*sb, cursor_pos, is_focused);
}
#endif // COMPONENTS_H
