#define FFC_IMPL
#include "game.h"
#include "utils.h"

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOUNALIGNED
#include "stb_sprintf.h"

#define COMPRESSESOR_BUF_SIZE (1024 * 1024)
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// For htonl/ntohl functions
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

LogEntry DeepCopyLogEntry(LogEntry entry) {
  LogEntry new_entry = {
      .fleets = malloc(sizeof *entry.fleets * entry.fleet_count),
      .planets = malloc(sizeof *entry.planets * entry.planet_count),
      .planet_count = entry.planet_count,
      .fleet_count = entry.fleet_count,
      .remaining_players = entry.remaining_players,
  };
  memcpy(new_entry.fleets, entry.fleets,
         sizeof *entry.fleets * entry.fleet_count);
  memcpy(new_entry.planets, entry.planets,
         sizeof *entry.planets * entry.planet_count);
  return new_entry;
}

void FreeInnerLogEntry(LogEntry entry) {
  free(entry.fleets);
  free(entry.planets);
}

GameLog DeepCopyGameLog(GameLog game_log) {
  LogEntry *log_entries = malloc(sizeof *game_log.items * game_log.count);
  for (unsigned i = 0; i < game_log.count; i++) {
    log_entries[i] = DeepCopyLogEntry(game_log.items[i]);
  }

  GameLog new_game_log = {
      .count = game_log.count,
      .capacity = game_log.capacity,
      .winning_player = game_log.winning_player,
      .items = log_entries,
      .players = DeepCopyPlayerDA(game_log.players),
      .draw = game_log.draw,
  };
  return new_game_log;
}

void FreeInnerGameLog(GameLog game_log) {
  for (unsigned i = 0; i < game_log.count; i++) {
    FreeInnerLogEntry(game_log.items[i]);
  }
  free(game_log.items);
  FreeInnerPlayerDA(game_log.players);
}

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
  FreeInnerGameLog(state.game_log);
  nob_da_free(state.planets);
  nob_da_free(state.fleets);
  FreeInnerPlayerDA(state.players);
}

// Parse map file, saving the map into the game state and returning the amount
// of different planet owners it has
unsigned ParseMapFile(GameState *state, const char *map_path) {
  FILE *map_file = fopen(map_path, "r");
  if (!map_file) {
    perror("Failed loading map file");
    exit(1);
  }

  char buf[256];
  unsigned file_line = 0;
  int bot_count = 0;
  state->player_bit_set = 0;

  while (fgets(buf, sizeof buf, map_file)) {
    file_line += 1;
    const unsigned buf_len = strlen(buf);
    if (buf_len == sizeof(buf) - 1 && buf[buf_len - 1] != '\n') {
      nob_log(NOB_ERROR,
              "Map file contains lines longer then 256 characters and "
              "cannot be read.");
      exit(1);
    }

    if (buf[0] != 'P')
      continue;

    Planet planet;
    if (!ParsePlanetLine(buf, buf_len, &planet)) {
      nob_log(NOB_ERROR, "Invalid map file.\nSyntax error at line %u.",
              file_line);
      exit(1);
    }
    if (planet.owner > MAX_PLAYER_AMOUNT) {
      nob_log(NOB_ERROR,
              "Map containes more owners then the max bot count. Encountered "
              "in line: %u\nOwner found: %d\nMax bot count: %d",
              file_line, planet.owner, MAX_PLAYER_AMOUNT);
      exit(1);
    }

    if (planet.owner != 0 &&
        !TestBit(state->player_bit_set, planet.owner - 1)) {
      bot_count++;
      SetBit(state->player_bit_set, planet.owner - 1);
    }

    snprintf(planet.print_prefix, NOB_ARRAY_LEN(planet.print_prefix),
             "P %8.6f %8.6f", planet.coords.x, planet.coords.y);

    nob_da_append(&state->planets, planet);
  }

  fclose(map_file);
  return bot_count;
}

GameState MakeGame(const char *map_file_path, PlayerDA players) {
  GameState state = {0};

  // ----- MAP -----
  nob_log(NOB_INFO, "Loading map file from %s.", map_file_path);
  unsigned owner_count = ParseMapFile(&state, map_file_path);
  if (owner_count != players.count) {
    nob_log(NOB_ERROR,
            "Provided map requires %u players, yet %u players were given as "
            "arguments.",
            owner_count, players.count);
    exit(1);
  }

  // ----- BOTS -----
  state.players = DeepCopyPlayerDA(players);
  state.remaining_players = state.players.count;
  state.game_log.players = DeepCopyPlayerDA(players);

  nob_da_foreach(Player, player, &state.players) {
      StartPlayer(player);
  }

  return state;
}

void DisqualifyPlayer(GameState *state, unsigned player_idx) {
  if (player_idx >= state->players.count) {
    nob_log(NOB_ERROR, "Attempted to disqualify non existent player");
    exit(1);
  }

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
  if (player_idx >= state->players.count) {
    nob_log(NOB_ERROR, "ERROR: Attempting access to non-existent bot process");
    exit(1);
  }
#define MoveOwner(Type, entity)                                                \
  Type moved_##entity = *entity;                                               \
  if (player_idx > 0 && moved_##entity.owner != 0) {                           \
    moved_##entity.owner =                                                     \
        (player_idx * (state->players.count - 1) + moved_##entity.owner - 1) % \
            state->players.count +                                             \
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

void sendMapToBot(GameState *state, Nob_String_Builder *sb, unsigned bot_idx) {
  if (bot_idx >= state->players.count) {
    nob_log(NOB_ERROR, "ERROR: Attempting access to non-existent bot process");
    exit(1);
  }

  GetMapRepresentation(state, sb, bot_idx);
  SendMessageToPlayer(state->players.items[bot_idx], sb->items, sb->count);
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

void RunBotCycle(GameState *state, Nob_String_Builder *sb) {
  unsigned bot_num = 0;
  nob_da_foreach(Player, player, &state->players) {
    // Skip disqualified or lost bots.
    if (TestBit(state->player_bit_set, bot_num)) {
      nob_log(NOB_DEBUG, "sending map to bot %u", bot_num);

      sb->count = 0;
      sendMapToBot(state, sb, bot_num);
    }
    bot_num++;
  }

  bot_num = 0;
  bool bot_okay = true;
  nob_da_foreach(Player, player, &state->players) {
    // Skip disqualified or lost bots.
    if (TestBit(state->player_bit_set, bot_num)) {
      sb->count = 0;
      bot_okay = GetPlayerMessage(*player, sb);

      if (bot_okay)
        bot_okay = SendPlayerShipsStr(state, bot_num,
                                      nob_sv_from_parts(sb->items, sb->count));

      if (bot_okay) {
        sb->count = 0;
        GetPlayerDebugMessage(*player, sb);
        if (sb->count)
          nob_log(NOB_INFO, "bot %s says: |%.*s|", player->name,
                  (unsigned)sb->count, sb->items);
      } else {
        // We don't remove the player from the dynamic array because we use the
        // DA index to address different players. Instead it is marked as
        // disqualified and not used.
        StopPlayer(&state->players.items[bot_num]);
      }

      nob_log(NOB_DEBUG, "done with bot %u, advancing to bot %u", bot_num,
              bot_num + 1);
    }
    bot_num++;
  }
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

// Runs one game turn using the planets and fleets saved.
// Appends an entry to the game log.
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

  // Save state to game log
  LogEntry entry = DeepCopyLogEntry((LogEntry){
      .remaining_players = state->remaining_players,
      .fleet_count = state->fleets.count,
      .fleets = state->fleets.items,
      .planet_count = state->planets.count,
      .planets = state->planets.items,
  });

  nob_da_append(&state->game_log, entry);
}

void RunGame(GameState *state) {
  // Reusable string builder to hold messages sent and received between the bots
  // and the engine.
  Nob_String_Builder sb = {0};

  for (int sim_turn = 0; state->remaining_players > 1 && sim_turn < 1000;
       sim_turn++) {
    nob_log(NOB_INFO, "Turn %d", sim_turn);
    // Bot communication
    sb.count = 0;
    RunBotCycle(state, &sb);

    // Game logic
    AdvanceTurn(state);
  }
  nob_log(NOB_INFO, "Game ended!");
  int winning_bot = bit_index(state->player_bit_set);
  if (winning_bot == -1) {
    nob_log(NOB_INFO, "It's a draw!");
    state->game_log.draw = true;
  } else {
    state->game_log.winning_player = winning_bot;
    state->game_log.draw = false;
    nob_log(NOB_INFO, "Bot %d won!", winning_bot + 1);
  }

  nob_sb_free(sb);
}

const unsigned version = 2;
const char magic[4] = {'p', 'l', 'w', 's'};

// ============================================================================
// Compression Helpers for Writing
// ============================================================================

typedef struct {
  FILE *file;
  mz_stream stream;
  unsigned char in_buf[COMPRESSESOR_BUF_SIZE];
  unsigned char out_buf[COMPRESSESOR_BUF_SIZE];
} CompressedWriter;

static bool InitCompressedWriter(CompressedWriter *cw, FILE *file) {
  memset(cw, 0, sizeof(*cw));
  cw->file = file;
  cw->stream.next_in = cw->in_buf;
  cw->stream.avail_in = 0;
  cw->stream.next_out = cw->out_buf;
  cw->stream.avail_out = COMPRESSESOR_BUF_SIZE;

  if (mz_deflateInit(&cw->stream, MZ_UBER_COMPRESSION) != MZ_OK) {
    return false;
  }
  return true;
}

static void FlushDeflateBuffer(CompressedWriter *cw, int flush) {
  cw->stream.next_in = cw->in_buf;
  while (1) {
    cw->stream.next_out = cw->out_buf;
    cw->stream.avail_out = COMPRESSESOR_BUF_SIZE;

    int status = mz_deflate(&cw->stream, flush);
    if (status != MZ_OK && status != MZ_STREAM_END && status != MZ_BUF_ERROR) {
      nob_log(NOB_ERROR, "deflate() failed with status %d.", status);
      exit(1);
    }

    size_t produced = COMPRESSESOR_BUF_SIZE - cw->stream.avail_out;
    if (produced > 0) {
      if (fwrite(cw->out_buf, 1, produced, cw->file) != produced) {
        nob_log(NOB_ERROR, "Failed writing to save file.");
        exit(1);
      }
    }

    if (flush == MZ_FINISH) {
      if (status == MZ_STREAM_END)
        break;
    } else {
      if (cw->stream.avail_in == 0)
        break;
    }
  }
  cw->stream.avail_in = 0;
  cw->stream.next_in = cw->in_buf;
}

static void WriteCompressed(CompressedWriter *cw, const void *data,
                            size_t size) {
  const unsigned char *src = (const unsigned char *)data;
  while (size > 0) {
    if (cw->stream.avail_in == COMPRESSESOR_BUF_SIZE) {
      FlushDeflateBuffer(cw, MZ_NO_FLUSH);
    }
    size_t to_copy = COMPRESSESOR_BUF_SIZE - cw->stream.avail_in;
    if (to_copy > size)
      to_copy = size;
    memcpy(cw->in_buf + cw->stream.avail_in, src, to_copy);
    cw->stream.avail_in += (unsigned int)to_copy;
    src += to_copy;
    size -= to_copy;
  }
}

static void FinishCompressedWriter(CompressedWriter *cw) {
  FlushDeflateBuffer(cw, MZ_FINISH);
  if (mz_deflateEnd(&cw->stream) != MZ_OK) {
    nob_log(NOB_ERROR, "deflateEnd() failed.");
    exit(1);
  }
}

// ============================================================================
// Decompression Helpers for Reading
// ============================================================================

typedef struct {
  FILE *file;
  mz_stream stream;
  unsigned char in_buf[COMPRESSESOR_BUF_SIZE];
  unsigned char out_buf[COMPRESSESOR_BUF_SIZE];
  size_t out_pos;
  size_t out_avail;
} CompressedReader;

static bool InitCompressedReader(CompressedReader *cr, FILE *file) {
  memset(cr, 0, sizeof(*cr));
  cr->file = file;
  if (mz_inflateInit(&cr->stream) != MZ_OK) {
    nob_log(NOB_ERROR, "inflateInit() failed!");
    return false;
  }
  return true;
}

static void FreeCompressedReader(CompressedReader *cr) {
  mz_inflateEnd(&cr->stream);
}

static bool ReadCompressed(CompressedReader *cr, void *dest, size_t size) {
  unsigned char *dst = (unsigned char *)dest;
  while (size > 0) {
    if (cr->out_avail > 0) {
      size_t to_copy = cr->out_avail < size ? cr->out_avail : size;
      memcpy(dst, cr->out_buf + cr->out_pos, to_copy);
      cr->out_pos += to_copy;
      cr->out_avail -= to_copy;
      dst += to_copy;
      size -= to_copy;
      if (size == 0)
        return true;
    }

    if (cr->stream.avail_in == 0) {
      size_t n = fread(cr->in_buf, 1, COMPRESSESOR_BUF_SIZE, cr->file);
      if (n == 0) {
        nob_log(NOB_ERROR, "Unexpected end of file while decompressing log.");
        return false;
      }
      cr->stream.next_in = cr->in_buf;
      cr->stream.avail_in = (unsigned int)n;
    }

    cr->stream.next_out = cr->out_buf;
    cr->stream.avail_out = COMPRESSESOR_BUF_SIZE;
    cr->out_pos = 0;

    int status = mz_inflate(&cr->stream, MZ_NO_FLUSH);
    if (status != MZ_OK && status != MZ_STREAM_END) {
      nob_log(NOB_ERROR, "inflate() failed with status %d.", status);
      return false;
    }

    cr->out_avail = COMPRESSESOR_BUF_SIZE - cr->stream.avail_out;
    if (cr->out_avail == 0 && status == MZ_STREAM_END) {
      nob_log(NOB_ERROR, "Reached end of compressed stream unexpectedly.");
      return false;
    }
  }
  return true;
}

// ============================================================================
// Main Serialization Functions
// ============================================================================

void WriteGameLogToFile(FILE *file, GameLog game_log) {
  // 1. Write uncompressed header (Magic & Version)
  fwrite(magic, 1, sizeof(magic), file);
  uint16_t version_net = htons((uint16_t)version);
  fwrite(&version_net, sizeof(version_net), 1, file);

  // 2. Initialize compressed stream writer
  CompressedWriter cw;
  if (!InitCompressedWriter(&cw, file)) {
    nob_log(NOB_ERROR, "deflateInit() failed!\n");
    exit(1);
  }

  union {
    float f;
    uint32_t u;
  } wrt_32_float;
  uint16_t wrt_16;
  uint32_t wrt_32;

#define WRITE(var) WriteCompressed(&cw, &(var), sizeof(var))
#define WRITE_8(var) WRITE(var)
#define WRITE_16(var)                                                          \
  do {                                                                         \
    wrt_16 = htons((uint16_t)(var));                                           \
    WRITE(wrt_16);                                                             \
  } while (0)
#define WRITE_32(var)                                                          \
  do {                                                                         \
    wrt_32 = htonl((uint32_t)(var));                                           \
    WRITE(wrt_32);                                                             \
  } while (0)
#define WRITE_float(var)                                                       \
  do {                                                                         \
    wrt_32_float.f = (float)(var);                                             \
    WRITE_32(wrt_32_float.u);                                                  \
  } while (0)

  WRITE_8(game_log.draw);
  WRITE_8(game_log.winning_player);
  WRITE_32(game_log.players.count);

  nob_da_foreach(Player, player, &game_log.players) {
    uint16_t string_length;

    string_length = (uint16_t)strlen(player->name);
    WRITE_16(string_length);
    for (uint16_t i = 0; i < string_length; i++) {
      WRITE_8(player->name[i]);
    }
  }

  WRITE_32(game_log.count);

  nob_da_foreach(LogEntry, entry, &game_log) {
    WRITE_32(entry->remaining_players);
    WRITE_32(entry->fleet_count);
    WRITE_32(entry->planet_count);

    for (unsigned i = 0; i < entry->fleet_count; i++) {
      WRITE_8(entry->fleets[i].owner);
      WRITE_8(entry->fleets[i].total);
      WRITE_8(entry->fleets[i].remaining);
      WRITE_16(entry->fleets[i].ships);
      WRITE_16(entry->fleets[i].src_id);
      WRITE_16(entry->fleets[i].dst_id);
    }

    for (unsigned i = 0; i < entry->planet_count; i++) {
      WRITE_8(entry->planets[i].owner);
      WRITE_8(entry->planets[i].growth);
      WRITE_16(entry->planets[i].ships);
      WRITE_float(entry->planets[i].coords.x);
      WRITE_float(entry->planets[i].coords.y);
    }
  }

  FinishCompressedWriter(&cw);

#undef WRITE_float
#undef WRITE_32
#undef WRITE_16
#undef WRITE_8
#undef WRITE
}

bool ReadGameLogFromFile(FILE *file, GameLog *game_log) {
  // 1. Read and verify uncompressed header (Magic & Version)
  char read_magic[sizeof(magic)];
  if (fread(read_magic, 1, sizeof(read_magic), file) != sizeof(read_magic)) {
    nob_log(NOB_ERROR, "Failed to read magic header.");
    return false;
  }
  if (memcmp(read_magic, magic, sizeof(magic)) != 0) {
    nob_log(NOB_ERROR,
            "Provided file is not a Planet Wars serialization file.");
    return false;
  }

  uint16_t read_version;
  if (fread(&read_version, sizeof(read_version), 1, file) != 1) {
    nob_log(NOB_ERROR, "Failed to read version.");
    return false;
  }
  read_version = ntohs(read_version);
  if (read_version != version) {
    nob_log(NOB_ERROR,
            "Serialization file version is unsupported. File version is %u and "
            "reader version is %u",
            read_version, version);
    return false;
  }

  // 2. Initialize compressed stream reader
  CompressedReader cr;
  if (!InitCompressedReader(&cr, file)) {
    return false;
  }

  union {
    float f;
    uint32_t u;
  } read_32_float;
  uint16_t read_16;
  uint32_t read_32;

#define READ(var) ReadCompressed(&cr, &(var), sizeof(var))
#define READ_ERROR_CHK(cond)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      nob_log(NOB_ERROR, "Reading error while reading from plws file.");       \
      FreeCompressedReader(&cr);                                               \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define READ_8(var) READ_ERROR_CHK(READ(var))
#define READ_16(var)                                                           \
  do {                                                                         \
    READ_ERROR_CHK(READ(read_16));                                             \
    var = ntohs(read_16);                                                      \
  } while (0)
#define READ_32(var)                                                           \
  do {                                                                         \
    READ_ERROR_CHK(READ(read_32));                                             \
    var = ntohl(read_32);                                                      \
  } while (0)
#define READ_float(var)                                                        \
  do {                                                                         \
    READ_32(read_32_float.u);                                                  \
    var = read_32_float.f;                                                     \
  } while (0)

  READ_8(game_log->draw);
  READ_8(game_log->winning_player);
  READ_32(game_log->players.count);

  game_log->players.items =
      malloc(sizeof *game_log->players.items * game_log->players.count);
  nob_da_foreach(Player, player, &game_log->players) {
    uint16_t string_length;
    READ_16(string_length);
    player->name = malloc(sizeof *player->name * (string_length + 1));
    READ_ERROR_CHK(ReadCompressed(&cr, player->name, string_length));
    player->name[string_length] = '\0';
    player->type = PLAYER_REPLAY;
  }

  READ_32(game_log->count);
  game_log->items = malloc(sizeof *game_log->items * game_log->count);
  game_log->capacity = game_log->count;

  nob_da_foreach(LogEntry, entry, game_log) {
    READ_32(entry->remaining_players);
    READ_32(entry->fleet_count);
    READ_32(entry->planet_count);
    entry->fleets = malloc(sizeof *entry->fleets * entry->fleet_count);
    entry->planets = malloc(sizeof *entry->planets * entry->planet_count);

    for (unsigned i = 0; i < entry->fleet_count; i++) {
      READ_8(entry->fleets[i].owner);
      READ_8(entry->fleets[i].total);
      READ_8(entry->fleets[i].remaining);
      READ_16(entry->fleets[i].ships);
      READ_16(entry->fleets[i].src_id);
      READ_16(entry->fleets[i].dst_id);
    }

    for (unsigned i = 0; i < entry->planet_count; i++) {
      READ_8(entry->planets[i].owner);
      READ_8(entry->planets[i].growth);
      READ_16(entry->planets[i].ships);
      READ_float(entry->planets[i].coords.x);
      READ_float(entry->planets[i].coords.y);
    }
  }

  FreeCompressedReader(&cr);

#undef READ_float
#undef READ_32
#undef READ_16
#undef READ_8
#undef READ_ERROR_CHK
#undef READ

  return true;
}
