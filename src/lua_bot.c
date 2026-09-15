#include "lua_bot.h"

#define DO_TURN_FUNCTION "do_turn"

static bool CheckLuaFunctionExists(lua_State *L, const char *func_name) {
  lua_getglobal(L, func_name);
  bool is_func = lua_isfunction(L, -1);
  lua_pop(L, 1);
  return is_func;
}

// This does not copy the lua state. Even if the bot being copied was started,
// the actually have a copy of it you need to call start on the new bot as well.
LuaBot DeepCopyLuaBot(LuaBot bot) {
  LuaBot new_bot = {
      .script_code = DupeStringBuilder(bot.script_code),
      .lua_state = NULL,
  };
  return new_bot;
}

void FreeInnerLuaBot(LuaBot bot) {
  StopLuaBot(bot);
  nob_sb_free(bot.script_code);
}

bool IsLuaBotActive(LuaBot bot) {
  return bot.lua_state == NULL;
}

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
            lua_tostring(bot->lua_state, -1));
    lua_pop(bot->lua_state, 1);
    return false;
  } else if (ret == LUA_ERRMEM) {
    nob_log(NOB_ERROR, "Failed starting bot-> Out of memory.");
    return false;
  }

  if (lua_pcall(bot->lua_state, 0, 0, 0) != LUA_OK) {
    nob_log(NOB_ERROR, "Failed executing bot script:\n%s",
            lua_tostring(bot->lua_state, -1));
    lua_pop(bot->lua_state, 1);
    return false;
  }

  if (!CheckLuaFunctionExists(bot->lua_state, DO_TURN_FUNCTION)) {
    nob_log(NOB_ERROR,
            "Bot code is missing required function: " DO_TURN_FUNCTION);
    return false;
  }

  return true;
}

void StopLuaBot(LuaBot bot) {
  if (bot.lua_state)
    lua_close(bot.lua_state);
}

// printf("lua_getglobal ret: %d\n", lua_getglobal(bot.lua_state, "hello"));
// lua_pushinteger(bot.lua_state, 42);
// lua_pushinteger(bot.lua_state, 55);
// lua_call(bot.lua_state, 2, 0);

bool GetLuaBotInstructions(LuaBot bot, GameInstructionDA *instructions) {
  NOB_TODO("In the works");
}
void GetLuaBotDebugMessage(LuaBot bot) {
  NOB_TODO("In the works");
}
