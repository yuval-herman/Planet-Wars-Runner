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
  Nob_String_Builder script_code;
});

bool StartLuaBot(LuaBot *bot);
bool IsLuaBotActive(LuaBot bot);
void StopLuaBot(LuaBot bot);
bool GetLuaBotInstructions(LuaBot bot, GameInstructionDA *instructions);
void GetLuaBotDebugMessage(LuaBot bot);

#endif // LUA_BOT_H
