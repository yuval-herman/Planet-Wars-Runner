#ifndef UI_H
#define UI_H

#include "raylib.h"

#include "../game_log.h"

enum Screens {
  SCREEN_NULL, // Starting empty screen
  SCREEN_VIEWER,
};

typedef struct {
  GameLog game_log;
  unsigned turn;
  int star_shader_time_loc;
  Shader stars_shader;
} ViewerState;

typedef struct {
  union {
    ViewerState viewer;
  } screen_data;
  enum Screens active_screen;
  GameLog game_log;
} UIState;

void UIInit(enum Screens start_screen);
void UIDraw();
void UIDestroy();

#endif // UI_H
