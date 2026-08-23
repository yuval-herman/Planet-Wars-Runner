#ifndef VIEWER_H
#define VIEWER_H
#include "ui_screen.h"

#include "../game_log.h"

#define BASE_PLANET_RADIUS 10.0f
#define PLANET_RADIUS_GROWTH_CURVE 20.0f
#define PLANET_RING_MAX_RADIUS 3.0F
#define MAP_MARGIN 50
#define CONTROLS_HEIGHT 70
#define PLAYER_LABELS_HEIGHT 30
#define SHIP_FONT_SIZE 20
// The name is confusing, but this value denotes the maximum value that the
// `game_speed` variable can hold, which controls the _lowest_ bound for game
// speed, i.e. how slow can the game run.
#define MAX_GAME_SPEED_VALUE 20

extern const UIScreen viewer_screen;

void SetGameLog(GameLog game_log);

#endif // VIEWER_H
