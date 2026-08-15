#ifndef PLAYER_H
#define PLAYER_H

#include "bot.h"

typedef enum {
  PLAYER_BOT,
  PLAYER_HUMAN,
  // This type is used when loading players from a save file. This players
  // cannot be interacted with.
  PLAYER_REPLAY,
} PlayerType;

DefineComplexStruct(Player, {
  PlayerType type;
  char *name;
  union {
    Bot bot;
    void *human; // currently nothing to store here.
  } as;
});

DefineComplexStruct(PlayerDA, {
  Player *items;
  unsigned count;
  unsigned capacity;
});

// If a player needs starting (for example a bot process) call this function
// before using it.
void StartPlayer(Player *player);
// If a player needs stopping (for example a bot process) this function does it.
// Note this does not neccesarily free memory, the exact implementation depend
// on the player type. You can still get a player name or other metadata after
// calling this function, but cannot interact with the player.
void StopPlayer(Player *player);
// Returns whether this player can be interacted with. For example in the case
// of a bot process, whether the process is alive
bool IsPlayerActive(Player player);
// Send a message to the player, return true on success, false otherwise.
bool SendMessageToPlayer(Player player, char *message, unsigned length);
// Return true if everythin went okay. Return false in case player should be
// disqualified.
bool GetPlayerMessage(Player player, Nob_String_Builder *sb);
// TODO Rename to get player chat message and add chat. Or IDK but this
// currently only makes sense to bots, it should make sense to every type of
// player, or a seperate system for bot development should be made.
void GetPlayerDebugMessage(Player player, Nob_String_Builder *sb);

#endif // PLAYER_H
