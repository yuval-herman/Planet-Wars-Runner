#ifndef VIEWER_H
#define VIEWER_H
#include "ui_screen.h"

#include "../game_log.h"

#define MAP_MARGIN 32
#define PLAYER_LABELS_HEIGHT 30
// The name is confusing, but this value denotes the maximum value that the
// `game_speed` variable can hold, which controls the _lowest_ bound for game
// speed, i.e. how slow can the game run.
#define MAX_GAME_SPEED_VALUE 20

extern const UIScreen viewer_screen;

void SetGameLog(GameLog game_log);

#endif // VIEWER_H
