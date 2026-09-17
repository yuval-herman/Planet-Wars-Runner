#include "lua_bot.h"
#include "lua_scripts.h"

#define DO_TURN_FUNCTION "do_turn"

// Extracts a planet ID from Lua stack index idx.
// Accepts either an integer ID or a planet table containing a 'planet_id' field.
// Returns true on successful extraction into *out_id, false on type or range error.
static bool extract_planet_id(lua_State *L, int idx, uint16_t *out_id) {
  int isnum = 0;
  if (lua_istable(L, idx)) {
    lua_getfield(L, idx, "planet_id");
    lua_Integer id = lua_tointegerx(L, -1, &isnum);
    lua_pop(L, 1);
    if (!isnum || id < 0 || id > UINT16_MAX)
      return false;
    *out_id = (uint16_t)id;
    return true;
  }
  lua_Integer id = lua_tointegerx(L, idx, &isnum);
  if (!isnum || id < 0 || id > UINT16_MAX)
    return false;
  *out_id = (uint16_t)id;
  return true;
}

// C closure bound to pw:issue_order(source, destination, num_ships)
// Records the order in the bot's instructions array.
// Sets bot->had_error if arguments are invalid or missing.
static int l_issue_order(lua_State *L) {
  LuaBot *bot = (LuaBot *)lua_touserdata(L, lua_upvalueindex(1));

  int top = lua_gettop(L);
  int arg_offset = 0;
  if (top >= 4 || (top >= 3 && lua_istable(L, 1))) {
    arg_offset = 1; // skip 'self' table when called with method syntax
  }

  uint16_t src, dst, ships;
  int isnum = 0;

  if (!extract_planet_id(L, arg_offset + 1, &src)) {
    nob_log(NOB_WARNING, "Lua bot issued order with invalid source planet.");
    bot->had_error = true;
    return 0;
  }

  if (!extract_planet_id(L, arg_offset + 2, &dst)) {
    nob_log(NOB_WARNING,
            "Lua bot issued order with invalid destination planet.");
    bot->had_error = true;
    return 0;
  }

  lua_Integer s = lua_tointegerx(L, arg_offset + 3, &isnum);
  if (!isnum || s <= 0 || s > UINT16_MAX) {
    nob_log(NOB_WARNING, "Lua bot issued order with invalid ship count.");
    bot->had_error = true;
    return 0;
  }
  ships = (uint16_t)s;

  GameInstruction inst = {
      .src_id = src,
      .dst_id = dst,
      .ships = ships,
  };
  nob_da_append(&bot->instructions, inst);
  return 0;
}

// C closure bound to pw:debug(message)
// Appends the debug message string to the bot's debug_messages string builder.
static int l_debug(lua_State *L) {
  LuaBot *bot = (LuaBot *)lua_touserdata(L, lua_upvalueindex(1));

  int top = lua_gettop(L);
  int msg_idx = (top >= 2 && lua_istable(L, 1)) ? 2 : 1;

  if (msg_idx <= top) {
    const char *msg = luaL_tolstring(L, msg_idx, NULL);
    nob_sb_append_cstr(&bot->debug_messages, msg);
    nob_sb_append(&bot->debug_messages, '\n');
    lua_pop(L, 1);
  }
  return 0;
}

static bool InitBotReferences(LuaBot *bot) {
  lua_getglobal(bot->lua_state, DO_TURN_FUNCTION);

  if (!lua_isfunction(bot->lua_state, -1)) {
    lua_pop(bot->lua_state, 1);
    bot->do_turn_ref = LUA_NOREF;
    return false;
  }

  bot->do_turn_ref = luaL_ref(bot->lua_state, LUA_REGISTRYINDEX);
  return true;
}

// This does not copy the lua state. Even if the bot being copied was started,
// the actually have a copy of it you need to call start on the new bot as well.
LuaBot DeepCopyLuaBot(LuaBot bot) {
  LuaBot new_bot = {
      .script_code = DupeStringBuilder(bot.script_code),
      .lua_state = NULL,
      .do_turn_ref = LUA_NOREF,
      .had_error = false,
  };
  return new_bot;
}

void FreeInnerLuaBot(LuaBot bot) {
  StopLuaBot(bot);
  nob_sb_free(bot.script_code);
  nob_sb_free(bot.debug_messages);
  nob_da_free(bot.instructions);
}

bool IsLuaBotActive(LuaBot bot) { return bot.lua_state != NULL; }

bool StartLuaBot(LuaBot *bot) {
  if (!bot->script_code.count) {
    nob_log(NOB_ERROR, "Attempted to start a lua bot without code.");
    return false;
  }

  if (!bot->lua_state) {
    bot->lua_state = luaL_newstate();
  } else
    return true;

  if (bot->lua_state == NULL) {
    nob_log(NOB_WARNING, "Failed to init lua state.");
    return false;
  }

  luaL_openselectedlibs(bot->lua_state,
                        LUA_GLIBK | LUA_LOADLIBK | LUA_COLIBK | LUA_UTF8LIBK |
                            LUA_STRLIBK | LUA_TABLIBK | LUA_MATHLIBK,
                        0);

  // Remove filesystem access functions exposed by base library
  lua_pushnil(bot->lua_state);
  lua_setglobal(bot->lua_state, "dofile");

  lua_pushnil(bot->lua_state);
  lua_setglobal(bot->lua_state, "loadfile");

  // Harden package: clear paths, remove loadlib, restrict searchers to preload only
  lua_getglobal(bot->lua_state, "package");
  lua_pushstring(bot->lua_state, "");
  lua_setfield(bot->lua_state, -2, "path");
  lua_pushstring(bot->lua_state, "");
  lua_setfield(bot->lua_state, -2, "cpath");
  lua_pushnil(bot->lua_state);
  lua_setfield(bot->lua_state, -2, "loadlib");

  // Keep only the preload searcher (searcher 1), remove searchers 2, 3, 4
  lua_getfield(bot->lua_state, -1, "searchers");
  for (int i = 2; i <= 4; ++i) {
    lua_pushnil(bot->lua_state);
    lua_rawseti(bot->lua_state, -2, i);
  }
  lua_pop(bot->lua_state, 1); // pop searchers

  // Load and execute the embedded PlanetWars module
  if (luaL_loadbufferx(bot->lua_state, (const char *)PlanetWars_source,
                       PlanetWars_size, "PlanetWars.lua", "t") != LUA_OK) {
    nob_log(NOB_ERROR, "Failed to load PlanetWars module:\n%s",
            lua_tostring(bot->lua_state, -1));
    lua_pop(bot->lua_state, 1);
    lua_pop(bot->lua_state, 1); // pop package
    return false;
  }

  if (lua_pcall(bot->lua_state, 0, 1, 0) != LUA_OK) {
    nob_log(NOB_ERROR, "Failed to execute PlanetWars module:\n%s",
            lua_tostring(bot->lua_state, -1));
    lua_pop(bot->lua_state, 1);
    lua_pop(bot->lua_state, 1); // pop package
    return false;
  }

  // Attach C closures to the PlanetWars table with bot upvalue
  lua_pushlightuserdata(bot->lua_state, bot);
  lua_pushcclosure(bot->lua_state, l_issue_order, 1);
  lua_setfield(bot->lua_state, -2, "issue_order");

  lua_pushlightuserdata(bot->lua_state, bot);
  lua_pushcclosure(bot->lua_state, l_debug, 1);
  lua_setfield(bot->lua_state, -2, "debug");

  // Register in registry as "PlanetWars" metatable
  lua_pushvalue(bot->lua_state, -1);
  lua_setfield(bot->lua_state, LUA_REGISTRYINDEX, "PlanetWars");

  // Register in package.loaded["PlanetWars"] so require("PlanetWars") works
  lua_getfield(bot->lua_state, -2, "loaded"); // package table is at -2
  lua_pushvalue(bot->lua_state, -2);          // PlanetWars table
  lua_setfield(bot->lua_state, -2, "PlanetWars");
  lua_pop(bot->lua_state, 1); // pop loaded

  // Expose PlanetWars as a global table
  lua_setglobal(bot->lua_state, "PlanetWars");

  lua_pop(bot->lua_state, 1); // pop package table

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

  if (!InitBotReferences(bot)) {
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

// Helper to push a single Planet struct as a Lua table with named fields
static void PushPlanet(lua_State *L, const Planet *planet, unsigned planet_id) {
  lua_createtable(L, 0, 6);

  lua_pushinteger(L, planet_id);
  lua_setfield(L, -2, "planet_id");

  lua_pushinteger(L, planet->owner);
  lua_setfield(L, -2, "owner");

  lua_pushinteger(L, planet->ships);
  lua_setfield(L, -2, "num_ships");

  lua_pushinteger(L, planet->growth);
  lua_setfield(L, -2, "growth_rate");

  lua_pushnumber(L, planet->coords.x);
  lua_setfield(L, -2, "x");

  lua_pushnumber(L, planet->coords.y);
  lua_setfield(L, -2, "y");
}

// Helper to push a single Fleet struct as a Lua table with named fields
static void PushFleet(lua_State *L, const Fleet *fleet) {
  lua_createtable(L, 0, 6);

  lua_pushinteger(L, fleet->owner);
  lua_setfield(L, -2, "owner");

  lua_pushinteger(L, fleet->ships);
  lua_setfield(L, -2, "num_ships");

  lua_pushinteger(L, fleet->src_id);
  lua_setfield(L, -2, "source_planet");

  lua_pushinteger(L, fleet->dst_id);
  lua_setfield(L, -2, "destination_planet");

  lua_pushinteger(L, fleet->total);
  lua_setfield(L, -2, "total_trip_length");

  lua_pushinteger(L, fleet->remaining);
  lua_setfield(L, -2, "turns_remaining");
}

bool SendMapToLuaBot(LuaBot *bot, Planet *planets, unsigned planet_count,
                     Fleet *fleets, unsigned fleet_count) {
  assert(bot->do_turn_ref >= 2);

  bot->instructions.count = 0;
  bot->debug_messages.count = 0;

  // Push the do_turn function
  lua_rawgeti(bot->lua_state, LUA_REGISTRYINDEX, bot->do_turn_ref);

  // Build the pw table
  lua_createtable(bot->lua_state, 0, 2);

  // Build planets array
  lua_createtable(bot->lua_state, planet_count, 0);
  for (unsigned i = 0; i < planet_count; ++i) {
    PushPlanet(bot->lua_state, &planets[i], i);
    lua_rawseti(bot->lua_state, -2, i + 1);
  }
  lua_setfield(bot->lua_state, -2, "planets");

  // Build fleets array
  lua_createtable(bot->lua_state, fleet_count, 0);
  for (unsigned i = 0; i < fleet_count; ++i) {
    PushFleet(bot->lua_state, &fleets[i]);
    lua_rawseti(bot->lua_state, -2, i + 1);
  }
  lua_setfield(bot->lua_state, -2, "fleets");

  // Attach PlanetWars metatable so pw has all module methods
  luaL_getmetatable(bot->lua_state, "PlanetWars");
  lua_setmetatable(bot->lua_state, -2);

  // Execute do_turn(pw)
  if (lua_pcall(bot->lua_state, 1, 0, 0) != LUA_OK) {
    nob_log(NOB_WARNING, "lua error: %s", lua_tostring(bot->lua_state, -1));
    lua_pop(bot->lua_state, 1);
    bot->had_error = true;
    return false;
  }
  return true;
}

bool GetLuaBotInstructions(LuaBot *bot, GameInstructionDA *instructions) {
  if (bot->had_error) {
    bot->had_error = false;
    bot->instructions.count = 0;
    return false;
  }
  nob_da_append_many(instructions, bot->instructions.items,
                     bot->instructions.count);
  bot->instructions.count = 0;
  return true;
}

void GetLuaBotDebugMessage(LuaBot *bot, Nob_String_Builder *sb) {
  nob_sb_append_buf(sb, bot->debug_messages.items, bot->debug_messages.count);
  bot->debug_messages.count = 0;
}
