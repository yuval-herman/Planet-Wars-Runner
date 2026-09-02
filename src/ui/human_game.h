#ifndef HUMAN_GAME_H
#define HUMAN_GAME_H
#include "ui_screen.h"

#include "../game.h"
#include "../player.h"

extern const UIScreen human_game_screen;

void SetGameState(GameState state);
void SetPlayers(PlayerDA players);

#endif // HUMAN_GAME_H

