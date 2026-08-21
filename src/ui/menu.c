#include "raylib.h"

#include "nob.h"

#include "menu.h"

void MenuInit(void *params) { NOB_UNUSED(params); }
void MenuDraw() {
  ClearBackground(RAYWHITE);

}
void MenuDestroy() {}

const UIScreen menu_screen = {
    .init = MenuInit,
    .draw = MenuDraw,
    .destroy = MenuDestroy,
};
