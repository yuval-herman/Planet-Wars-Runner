#ifndef GAME_H
#define GAME_H
#include "nob.h"
#include "planet_wars.h"
#include "player.h"
#include "utils.h"

#include <stdint.h>

typedef uint32_t PlayerBitset;
_Static_assert(MAX_PLAYER_AMOUNT <= sizeof(PlayerBitset) * CHAR_BIT,
               "PlayerBitset is not wide enough to hold max amount of players");

DefineComplexStruct(GameState, {
  PlanetDA planets;
  FleetsDA fleets;
  PlayerDA players;
  PlayerBitset player_bit_set;
  unsigned remaining_players;
  unsigned turn;
});

GameState MakeGame(const char *map_file_path, PlayerDA players);

// Get map representation for a specific player. Each player should see itself
// as player 1 according to the protocol. This function takes care of that. If
// `player_idx` is 0 the real map is returned with no modification.
void GetMapRepresentation(GameState *state, Nob_String_Builder *sb,
                          unsigned player_idx);

// Return true if the player can play this actions. Return false in case the
// player has bean disqualified for attempting an invalid action.
bool SendPlayerShips(GameState *state, unsigned player_idx, uint16_t src_id,
                     uint16_t dst_id, uint16_t ships);

// Play player actions from string. Parses the string and calls
// `SendPlayerShips`.
bool SendPlayerShipsStr(GameState *state, unsigned player_idx,
                        Nob_String_View order_sv);

// Runs one game turn using the planets and fleets saved.
// Appends an entry to the game log.
void AdvanceTurn(GameState *state);

void DisqualifyPlayer(GameState *state, unsigned player_idx);

// Whether the player is still in the game, or has been defeated or
// disqualified.
static FORCEINLINE bool IsPlayerAlive(GameState *state, unsigned player_idx) {
  return TestBit(state->player_bit_set, player_idx);
}

#endif // GAME_H
