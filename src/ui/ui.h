#ifndef UI_H
#define UI_H

#include "raylib.h"

enum Screens {
  SCREEN_NULL = 0, // Starting empty screen
  SCREEN_VIEWER,
  SCREEN_MENU,
  SCREEN_HUMAN_GAME,
};

enum Fonts {
  RaylibDefault = 0,
  FiraCode_Bold,
  FiraCode_Regular,
  Cousine_Regular,
};

typedef struct {
  Font font;
  enum Fonts font_id;
  unsigned font_size;
} FontInfo;

typedef struct {
  FontInfo *items;
  unsigned count;
  unsigned capacity;
} FontsDA;

// A little helper to use the font system more naturaly with clay.
// Usege example:
// CLAY_TEXT(text, {
//   .fontId = CLAY_FONT(FiraCode_Bold, 16),
//   .textColor = C_WHITE,
// });
//
#define CLAY_FONT(font, size) GetFontId(font, size), .fontSize = size

// Returns a font id for the requested font and size to be used in clay. May
// load a new font if it was not already loaded.
unsigned GetFontId(enum Fonts font_id, unsigned size);
// Get the frame number. Frames are counted for every frame drawn in UIRun
// across all screen together.
unsigned GetFrame();
void UIInit(enum Screens start_screen);
void ChangeScreen(enum Screens screen);
void UIRun();
void UIDestroy();

#endif // UI_H
