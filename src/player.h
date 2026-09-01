#ifndef PLAYER_H
#define PLAYER_H

#include "bot.h"

typedef enum {
  // This type is used when loading players from a save file. This players
  // cannot be interacted with.
  PLAYER_REPLAY,
  PLAYER_BOT,
  PLAYER_HUMAN,
} PlayerType;

DefineComplexStruct(Player, {
  PlayerType type;
  Nob_String_Builder name;
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
// before using it. If a player can not be started, this function returns false,
// otherwise it returns true. Note this function will returns true even if the
// player was already active before calling this function.
bool StartPlayer(Player *player);
// If a player needs stopping (for example a bot process) this function does it.
// Note this does not neccesarily free memory, the exact implementation depend
// on the player type. You can still get a player name or other metadata after
// calling this function, but cannot interact with the player.
// If a player cannot be stopped, this function returns false, otherwise, it
// retuns true. Note this function will return true even if the player was
// stopped before calling this function.
bool StopPlayer(Player *player);
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
