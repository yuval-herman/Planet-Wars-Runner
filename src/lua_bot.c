#include "lua_bot.h"

bool StartLuaBot(LuaBot *bot) {
  if (!bot->script_code.count) {
    nob_log(NOB_ERROR, "Attempted to start a lua bot without code.");
    return false;
  }

  if (!bot->lua_state) {
    bot->lua_state = luaL_newstate();
  }

  if (bot->lua_state == NULL) {
    nob_log(NOB_WARNING, "Failed to init lua state.");
    return false;
  }

  luaL_openlibs(bot->lua_state);

  int ret = luaL_loadbufferx(bot->lua_state, bot->script_code.items,
                             bot->script_code.count, "bot code", "t");
  if (ret == LUA_ERRSYNTAX) {
    nob_log(NOB_WARNING, "Failed to load lua code:\n%s",
            lua_tostring(bot->lua_state, 1));
    return false;
  } else if (ret == LUA_ERRMEM) {
    nob_log(NOB_ERROR, "Failed starting bot-> Out of memory.");
    return false;
  }

  if (LUA_OK != lua_pcall(bot->lua_state, 0, LUA_MULTRET, 0)) {
    nob_log(NOB_ERROR, "Failed starting bot:\n%s",
            lua_tostring(bot->lua_state, 1));
    return false;
  }
  return true;
}

void StopLuaBot(LuaBot bot) {
  lua_close(bot.lua_state);
  nob_sb_free(bot.script_code);
}

// printf("lua_getglobal ret: %d\n", lua_getglobal(bot.lua_state, "hello"));
// lua_pushinteger(bot.lua_state, 42);
// lua_pushinteger(bot.lua_state, 55);
// lua_call(bot.lua_state, 2, 0);

bool GetLuaBotInstructions(LuaBot bot, GameInstructionDA *instructions);
void GetLuaBotDebugMessage(LuaBot bot);
