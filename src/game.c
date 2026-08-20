#define FFC_IMPL
#include "game.h"
#include "utils.h"

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOUNALIGNED
#include "stb_sprintf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GameState DeepCopyGameState(GameState state) {
  NOB_UNUSED(state);
  // Copying is unsupported because game state contains some things relevant to
  // a single game that would not make sense duplicated, such as bot processes.
  // It would be technically possible to spin up new bot processes using the
  // same command and so on, but it wouldn't make sense. If you got here,
  // perhaps you should reconsider what you are trying to do.
  NOB_UNREACHABLE("Coping game state is unsupported.");
}

void FreeInnerGameState(GameState state) {
  nob_da_free(state.planets);
  nob_da_free(state.fleets);
}

bool ParseMapFile(unsigned *owner_count, GameState *state,
                  const char *map_path) {
  FILE *map_file = fopen(map_path, "r");
  if (!map_file) {
    nob_log(NOB_ERROR, "Failed loading map file \"%s\": %s", map_path,
            strerror(errno));
    return false;
  }

  char *data;
  size_t length;
  if (!ReadEntireFile(map_file, MAX_MAP_FILE_SIZE, &data, &length)) {
    nob_log(NOB_ERROR,
            "Failed loading map file \"%s\"\n Perhapas the map file is bigger "
            "then the max map size allowed?. Max map file size allowed is %u",
            map_path, MAX_MAP_FILE_SIZE);
    return false;
  }

  bool parsing_result = ParseMapBuffer(owner_count, state, data, length);
  free(data);
  return parsing_result;
}

bool ParseMapBuffer(unsigned *owner_count, GameState *state,
                    const char *map_buffer, unsigned buffer_length) {
  Nob_String_View content = nob_sv_from_parts(map_buffer, buffer_length);
  unsigned file_line = 0;
  *owner_count = 0;
  state->player_bit_set = 0;

  while (content.count > 0) {
    file_line += 1;
    Nob_String_View line = nob_sv_trim(nob_sv_chop_by_delim(&content, '\n'));

    if (line.count == 0 || line.data[0] != 'P')
      continue;

    Planet planet;
    if (!ParsePlanetLine(line.data, line.count, &planet)) {
      nob_log(NOB_ERROR, "Invalid map file.\nSyntax error at line %u.",
              file_line);
      return false;
    }
    if (planet.owner > MAX_PLAYER_AMOUNT) {
      nob_log(NOB_ERROR,
              "Map containes more owners then the max bot count. Encountered "
              "in line: %u\nOwner found: %d\nMax bot count: %d",
              file_line, planet.owner, MAX_PLAYER_AMOUNT);
      return false;
    }

    if (planet.owner != 0 &&
        !TestBit(state->player_bit_set, planet.owner - 1)) {
      (*owner_count)++;
      SetBit(state->player_bit_set, planet.owner - 1);
    }

    snprintf(planet.print_prefix, NOB_ARRAY_LEN(planet.print_prefix),
             "P %8.6f %8.6f", planet.coords.x, planet.coords.y);

    nob_da_append(&state->planets, planet);
  }

  return true;
}

bool MakeGame(GameState *state, const char *map_file_path,
              unsigned player_count) {
  // ----- MAP -----
  nob_log(NOB_INFO, "Loading map file from %s.", map_file_path);
  unsigned owner_count = 0;
  if (!ParseMapFile(&owner_count, state, map_file_path)) {
    nob_log(NOB_ERROR, "Failed parsing map file.");
    return false;
  }
  if (owner_count != player_count) {
    nob_log(NOB_ERROR,
            "Provided map requires %u players, yet %u players were given as "
            "arguments.",
            owner_count, player_count);
    return false;
  }

  state->remaining_players = player_count;
  state->player_count = player_count;

  return true;
}

void DisqualifyPlayer(GameState *state, unsigned player_idx) {
  assert(player_idx < state->player_count &&
         "Attempted to disqualify non existent player");

  state->remaining_players--;
  nob_da_foreach(Planet, planet, &state->planets) {
    if ((unsigned)planet->owner == player_idx + 1) {
      planet->owner = 0;
    }
  }
  nob_da_foreach(Fleet, fleet, &state->fleets) {
    if ((unsigned)fleet->owner == player_idx + 1) {
      *fleet = state->fleets.items[--state->fleets.count];
      fleet--;
    }
  }

  UnsetBit(state->player_bit_set, player_idx);

  nob_log(NOB_INFO, "Disqualified bot %u.", player_idx);
}

static char *sb_printf_callback(const char *buf, void *user, int len) {
  NOB_UNUSED(buf);
  Nob_String_Builder *sb = user;

  sb->count += len;
  nob_da_reserve(sb, STB_SPRINTF_MIN + sb->count);

  return sb->items + sb->count;
}

int vsb_printf(Nob_String_Builder *sb, char const *fmt, va_list va) {
  // Make sure we have enough memory for at least `STB_SPRINTF_MIN` in the
  // initial write.
  nob_da_reserve(sb, STB_SPRINTF_MIN + sb->count);
  return stbsp_vsprintfcb(sb_printf_callback, sb, sb->items + sb->count, fmt,
                          va);
}

int sb_printf(Nob_String_Builder *sb, char const *fmt, ...) {
  int result;
  va_list va;
  va_start(va, fmt);

  result = vsb_printf(sb, fmt, va);
  va_end(va);

  return result;
}

static inline void PrintPlanet(Nob_String_Builder *sb, Planet planet) {
  sb_printf(sb, "%s %hu %hu %hu\n", planet.print_prefix, planet.owner,
            planet.ships, planet.growth);
}

static inline void PrintFleet(Nob_String_Builder *sb, Fleet fleet) {
  sb_printf(sb, "F %hu %hu %hu %hu %hu %hu\n", fleet.owner, fleet.ships,
            fleet.src_id, fleet.dst_id, fleet.total, fleet.remaining);
}

void GetMapRepresentation(GameState *state, Nob_String_Builder *sb,
                          unsigned player_idx) {
  assert(player_idx < state->player_count &&
         "Attempting access to non-existent bot process");

#define MoveOwner(Type, entity)                                                \
  Type moved_##entity = *entity;                                               \
  if (player_idx > 0 && moved_##entity.owner != 0) {                           \
    moved_##entity.owner =                                                     \
        (player_idx * (state->player_count - 1) + moved_##entity.owner - 1) %  \
            state->player_count +                                              \
        1;                                                                     \
  }

  sb->count = 0;
  nob_da_foreach(Planet, planet, &state->planets) {
    // Each bot should see itself as bot number 1.
    MoveOwner(Planet, planet);
    PrintPlanet(sb, moved_planet);
  }

  nob_da_foreach(Fleet, fleet, &state->fleets) {
    MoveOwner(Fleet, fleet);
    PrintFleet(sb, moved_fleet);
  }

  nob_sb_append_cstr(sb, MESSAGE_DELIMETER);

#undef MoveOwner
}

bool SendPlayerShips(GameState *state, unsigned player_idx, uint16_t src_id,
                     uint16_t dst_id, uint16_t ships) {
  Fleet fleet;
  fleet.owner = player_idx + 1;
  fleet.src_id = src_id;
  fleet.dst_id = dst_id;
  fleet.ships = ships;

  if ((unsigned)fleet.src_id >= state->planets.count) {
    nob_log(NOB_INFO, "Bot tried sending fleet from nonexistent planet.");
    DisqualifyPlayer(state, player_idx);
    return false;
  }
  Planet *src = &state->planets.items[fleet.src_id];
  if (fleet.ships < 1) {
    nob_log(NOB_INFO, "Bot tried sending invalid amount of ships.");
    DisqualifyPlayer(state, player_idx);
    return false;
  } else if (fleet.src_id == fleet.dst_id) {
    nob_log(NOB_INFO, "Bot tried sending fleet from a planet itself.");
    DisqualifyPlayer(state, player_idx);
    return false;
  } else if (src->owner != fleet.owner) {
    nob_log(NOB_INFO, "Bot tried sending fleet from a planet it does not own.");
    DisqualifyPlayer(state, player_idx);
    return false;
  } else if (fleet.dst_id >= state->planets.count) {
    nob_log(NOB_INFO, "Bot tried sending fleet to nonexistent planet.");
    DisqualifyPlayer(state, player_idx);
    return false;
  } else if (src->ships < fleet.ships) {
    nob_log(NOB_INFO, "Bot tried sending more ships then the planet has.");
    DisqualifyPlayer(state, player_idx);
    return false;
  }
  src->ships -= fleet.ships;
  Planet dst = state->planets.items[fleet.dst_id];

  fleet.total = ceilf(Vector2Distance(src->coords, dst.coords));
  fleet.remaining = fleet.total;

  nob_da_append(&state->fleets, fleet);
  return true;
}

bool SendPlayerShipsStr(GameState *state, unsigned player_idx,
                        Nob_String_View order_sv) {
  if (order_sv.count < 2 ||
      (order_sv.data[0] == 'g' && order_sv.data[1] == 'o')) {
    return true;
  }

  while (order_sv.count > 1 && order_sv.data[0] != 'g' &&
         order_sv.data[1] != 'o') {
    nob_log(NOB_DEBUG, "parsing bot %u fleets", player_idx);
    unsigned parsed_uint;
    uint16_t src_id, dst_id, ships;
    ffc_result result;
    const char *p_end = order_sv.data + order_sv.count;
    ffc_parse_options parse_options = ffc_parse_options_default();
    parse_options.format |= FFC_FORMAT_FLAG_SKIP_WHITE_SPACE;

#define PARSE_INT(output, err_msg)                                             \
  result = ffc_from_chars_u32_options(order_sv.data, p_end, 10, &parsed_uint,  \
                                      parse_options);                          \
  if (result.outcome != FFC_OUTCOME_OK || parsed_uint > UINT16_MAX) {          \
    nob_log(NOB_INFO, "Invalid bot command. " err_msg);                        \
    DisqualifyPlayer(state, player_idx);                                       \
    return false;                                                              \
  }                                                                            \
  output = parsed_uint;                                                        \
  order_sv.count -= result.ptr - order_sv.data;                                \
  order_sv.data = result.ptr;

    PARSE_INT(src_id, "Source planet out of bounds or impossible to parse.");
    PARSE_INT(dst_id,
              "Destionation planet out of bounds or impossible to parse.");
    PARSE_INT(ships,
              "Amount of ships is too high, to low, or impossible to parse.");
#undef PARSE_INT

    if (!SendPlayerShips(state, player_idx, src_id, dst_id, ships))
      return false;

    order_sv = nob_sv_trim_left(order_sv);
  }
  nob_log(NOB_DEBUG, "done parsing bot %u fleets", player_idx);
  return true;
}

// Used for sorting fleets in attack resolution.
int cmp_fleet_owner_remaining(const void *a, const void *b) {
  const Fleet *fa = a;
  const Fleet *fb = b;

  if (fa->remaining < fb->remaining)
    return -1;
  if (fa->remaining > fb->remaining)
    return 1;

  if (fa->dst_id < fb->dst_id)
    return -1;
  if (fa->dst_id > fb->dst_id)
    return 1;

  if (fa->owner < fb->owner)
    return -1;
  if (fa->owner > fb->owner)
    return 1;
  return 0;
}

void AdvanceTurn(GameState *state) {
  int bot_count = 0;

  state->player_bit_set = 0;
  nob_da_foreach(Planet, planet, &state->planets) {
    if (planet->owner != 0) {
      planet->ships += planet->growth;
      // If the player wasn't counted yet
      if (!TestBit(state->player_bit_set, planet->owner - 1)) {
        bot_count++;
        SetBit(state->player_bit_set, planet->owner - 1);
      }
    }
  }

  if (state->fleets.count == 0) {
    state->remaining_players = bot_count;
    return;
  }

  qsort(state->fleets.items, state->fleets.count,
        sizeof(state->fleets.items[0]), cmp_fleet_owner_remaining);

  nob_da_foreach(Fleet, fleet, &state->fleets) { fleet->remaining--; }

  Fleet *current_fleet = state->fleets.items;
  Fleet *end_fleet = state->fleets.items + state->fleets.count;

  while (current_fleet < end_fleet && current_fleet->remaining == 0) {
    int current_dst = current_fleet->dst_id;
    Planet *planet = &state->planets.items[current_dst];

    // MAX_PLAYER_AMOUNT + 1 to account for neutral planets
    struct {
      int owner;
      int force;
    } forces[MAX_PLAYER_AMOUNT + 1];
    int forces_count = 0;

    // Add current planet being attack, even if it's a neutral planet
    forces[forces_count].owner = planet->owner;
    forces[forces_count].force = planet->ships;
    forces_count++;

    // Process all fleets attacking the current planet
    while (current_fleet < end_fleet && current_fleet->remaining == 0 &&
           current_fleet->dst_id == current_dst) {

      // In case this fleet belongs to the owner of the current planet
      if (current_fleet->owner == forces[0].owner) {
        forces[0].force += current_fleet->ships;
      }
      // In case this fleet is from the same owner that sent the previous
      // fleet in the list. This works because fleet are sorted by
      // remaining->dst_id->owner. So we don't need to search the forces array
      // for the owner.
      else if (current_fleet->owner == forces[forces_count - 1].owner) {
        forces[forces_count - 1].force += current_fleet->ships;
      }
      // In case this is a new owner, add it to the list.
      else {
        forces[forces_count].owner = current_fleet->owner;
        forces[forces_count].force = current_fleet->ships;
        forces_count++;
      }

      current_fleet++;
    }

    // Find the two biggest forces
    int max_force_idx = 0;
    int second_force_idx = -1;

    for (int i = 1; i < forces_count; i++) {
      if (forces[i].force > forces[max_force_idx].force) {
        second_force_idx = max_force_idx;
        max_force_idx = i;
      } else if (second_force_idx == -1 ||
                 forces[i].force > forces[second_force_idx].force) {
        second_force_idx = i;
      }
    }

    if (second_force_idx == -1 ||
        forces[max_force_idx].force > forces[second_force_idx].force) {
      int winner_force = forces[max_force_idx].force;
      int runner_up_force =
          (second_force_idx == -1) ? 0 : forces[second_force_idx].force;

      planet->owner = forces[max_force_idx].owner;
      planet->ships = winner_force - runner_up_force;
    } else {
      planet->ships = 0;
    }
  }
  // We do this after processing becuase we need to keep fleet order while
  // processing them.
  for (unsigned i = 0; i < state->fleets.count; i++) {
    if (state->fleets.items[i].remaining == 0) {
      nob_da_remove_unordered(&state->fleets, i);
      // Remove unordered replaces the current fleet with the last one,
      // so we need to run the loop again on the same index.
      i--;
    }
    // If the player wasn't counted yet
    else if (!TestBit(state->player_bit_set,
                      state->fleets.items[i].owner - 1)) {
      bot_count++;
      SetBit(state->player_bit_set, state->fleets.items[i].owner - 1);
    }
  }

  state->remaining_players = bot_count;
}
