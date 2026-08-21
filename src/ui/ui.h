#ifndef UI_H
#define UI_H

enum Screens {
  SCREEN_NULL = 0, // Starting empty screen
  SCREEN_VIEWER,
  SCREEN_MENU,
};

void UIInit(enum Screens start_screen, void *screen_params);
void ChangeScreen(enum Screens screen, void *screen_params);
void UIRun();
void UIDestroy();

#endif // UI_H
