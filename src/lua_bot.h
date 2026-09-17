#ifndef LUA_BOT_H
#define LUA_BOT_H
#include "utils.h"
#include "game.h"

#include <lauxlib.h>
#include <lualib.h>

// Time in nanoseconds, currently set to 100ms
#define MAX_BOT_RESPONSE_TIME (1000 * 1000 * 100)
// How much time to sleep between checks on bots responses in nanoseconds,
// currently set to 100 microseconds
#define WAIT_SLEEP_TIME (1000 * 100)

DefineComplexStruct(LuaBot, {
  lua_State *lua_state;
  int do_turn_ref;
  Nob_String_Builder script_code;
  GameInstructionDA instructions;
  Nob_String_Builder debug_messages;
  bool had_error;
});

bool StartLuaBot(LuaBot *bot);
bool IsLuaBotActive(LuaBot bot);
void StopLuaBot(LuaBot *bot);
bool SendMapToLuaBot(LuaBot *bot, Planet *planets, unsigned planet_count,
                     Fleet *fleets, unsigned fleet_count);
// This should be called after calling `SendMapToLuaBot`. It simply copies the
// cached instructions from the previous calls and clear them.
bool GetLuaBotInstructions(LuaBot *bot, GameInstructionDA *instructions);
void GetLuaBotDebugMessage(LuaBot *bot, Nob_String_Builder *sb);

#endif // LUA_BOT_H
