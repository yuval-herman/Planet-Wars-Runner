#ifndef UI_H
#define UI_H

enum Screens {
  SCREEN_NULL = 0, // Starting empty screen
  SCREEN_VIEWER,
  SCREEN_MENU,
};

// Get the frame number. Frames are counted for every frame drawn in UIRun
// across all screen together.
unsigned GetFrame();
void UIInit(enum Screens start_screen);
void ChangeScreen(enum Screens screen);
void UIRun();
void UIDestroy();

#endif // UI_H
