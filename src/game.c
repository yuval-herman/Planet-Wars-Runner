#include "game.h"
#include "subprocess.h"
#include "utils.h"

#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOUNALIGNED
#include "stb_sprintf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// For htonl/ntohl functions
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

// ## won't work in MSVC, we will cross that bridge when we get there.
#define WriteToLogFile(fmt, ...)                                               \
  do {                                                                         \
    if (state->log_file)                                                       \
      fprintf(state->log_file, fmt, ##__VA_ARGS__);                            \
  } while (0)

LogEntry DeepCopyLogEntry(LogEntry entry) {
  LogEntry new_entry = {
      .fleets = malloc(sizeof *entry.fleets * entry.fleet_count),
      .planets = malloc(sizeof *entry.planets * entry.planet_count),
      .planet_count = entry.planet_count,
      .fleet_count = entry.fleet_count,
      .remaining_bots = entry.remaining_bots,
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
      .winning_bot = game_log.winning_bot,
      .items = log_entries,
      .bots = DeepCopyBotsDA(game_log.bots),
      .draw = game_log.draw,
  };
  return new_game_log;
}

void FreeInnerGameLog(GameLog game_log) {
  for (unsigned i = 0; i < game_log.count; i++) {
    FreeInnerLogEntry(game_log.items[i]);
  }
  free(game_log.items);
  FreeInnerBotsDA(game_log.bots);
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
  FreeInnerBotsDA(state.bots);
  if (state.log_file)
    fclose(state.log_file);
}

// This does not copy the bot process! If you want to start another process for
// the bot, you must do so manually.
Bot DeepCopyBot(Bot bot) {
  Bot new_bot = {
      .name = bot.name ? DupeString(bot.name) : NULL,
      .start_command = DupeString(bot.start_command),
      .process = NULL,
  };
  return new_bot;
}

// This DOES stop and free the bot process.
void FreeInnerBot(Bot bot) {
  StopBot(bot);
  free(bot.name);
  free(bot.start_command);
  free(bot.process);
}

BotsDA DeepCopyBotsDA(BotsDA bots) {
  BotsDA new_bots = {
      .items = malloc(sizeof *bots.items * bots.count),
      .count = bots.count,
      .capacity = bots.count,
  };
  for (unsigned i = 0; i < bots.count; i++) {
    new_bots.items[i] = DeepCopyBot(bots.items[i]);
  }
  return new_bots;
}

void FreeInnerBotsDA(BotsDA bots) {
  nob_da_foreach(Bot, bot, &bots) {
    FreeInnerBot(*bot);
    bot->process = NULL;
  }
  nob_da_free(bots);
}

void StopBot(Bot bot) {
  if (bot.process == NULL)
    return;
  if (subprocess_alive(bot.process)) {
    if (subprocess_terminate(bot.process) != 0 ||
        subprocess_join(bot.process, NULL) != 0) {
      nob_log(NOB_WARNING, "Failed terminating bot process: %s.",
              bot.name ? bot.name : bot.start_command);
    }
  }
  subprocess_destroy(bot.process);
}

void StartBot(Bot bot) {
  Nob_Cmd split_command = SplitStringByDelim(bot.start_command, ' ');
  // Required by subprocess.h
  nob_cmd_append(&split_command, NULL);

  if (bot.process == NULL) {
    nob_log(NOB_ERROR, "Can't start a bot without a process struct allocated.");
    // TODO maybe rethink exit calls from this function. Since we moved to a
    // single small and self-contained function, exiting might not be right in
    // all cases, perhaps returning a bool would be better.
    exit(1);
  }
  int result = subprocess_create(split_command.items,
                                 subprocess_option_search_user_path |
                                     subprocess_option_inherit_environment |
                                     subprocess_option_enable_async |
                                     subprocess_option_enable_async_no_wait,
                                 bot.process);

  if (0 != result) {
    // TODO errno is only POSIX, need to add GetLastError for windows here
    nob_log(NOB_ERROR, "ERROR: Failed to launch bot number %d: %s\n%s", 2,
            bot.start_command,
#ifdef _WIN32
            nob_win32_error_message(GetLastError())
#else
            strerror(errno)
#endif
    );
    // See comment on previous exit
    exit(1);
  }
  nob_cmd_free(split_command);
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
  state->bot_bit_set = 0;

  while (fgets(buf, sizeof buf, map_file)) {
    file_line += 1;
    unsigned len = strlen(buf);
    if (len == sizeof(buf) - 1 && buf[len - 1] != '\n') {
      nob_log(NOB_ERROR,
              "Map file contains lines longer then 256 characters and "
              "cannot be read.");
      exit(1);
    }

    if (buf[0] != 'P')
      continue;

    Planet planet;
    if (!ParsePlanetLine(buf, &planet)) {
      nob_log(NOB_ERROR, "Invalid map file.\nSyntax error at line %u.",
              file_line);
      exit(1);
    }
    if (planet.owner > MAX_BOT_AMOUNT) {
      nob_log(NOB_ERROR,
              "Map containes more owners then the max bot count. Encountered "
              "in line: %u\nOwner found: %d\nMax bot count: %d",
              file_line, planet.owner, MAX_BOT_AMOUNT);
      exit(1);
    }

    if (planet.owner != 0 && !TestBit(state->bot_bit_set, planet.owner - 1)) {
      bot_count++;
      SetBit(state->bot_bit_set, planet.owner - 1);
    }

    snprintf(planet.print_prefix, NOB_ARRAY_LEN(planet.print_prefix),
             "P %8.6f %8.6f", planet.coords.x, planet.coords.y);

    nob_da_append(&state->planets, planet);
  }

  fclose(map_file);
  return bot_count;
}

GameState MakeGame(const char *map_file_path, BotsDA bots, bool log) {
  GameState state = {0};

  // ----- LOGGING -----
  if (log) {
    state.log_file = fopen(LOG_FILE, "w");
    if (!state.log_file) {
      nob_log(NOB_WARNING, "Failed to open log file: %s", strerror(errno));
    } else {
      fprintf(state.log_file, "initializing\n");
    }
  }

  // ----- MAP -----
  nob_log(NOB_INFO, "Loading map file from %s.", map_file_path);
  unsigned owner_count = ParseMapFile(&state, map_file_path);
  if (owner_count != bots.count) {
    nob_log(NOB_ERROR,
            "Provided map requires %u player, yet %u bots were given as "
            "arguments.",
            owner_count, bots.count);
    exit(1);
  }

  // ----- BOTS -----
  state.bots = DeepCopyBotsDA(bots);
  nob_da_foreach(Bot, bot, &state.bots) {
    if (bot->process == NULL)
      bot->process = malloc(sizeof *bot->process);
    StartBot(*bot);
  }
  state.remaining_bots = state.bots.count;
  state.game_log.bots = DeepCopyBotsDA(bots);

  return state;
}

void DisqualifyBot(GameState *state, unsigned bot_idx) {
  if (bot_idx >= state->bots.count) {
    nob_log(NOB_ERROR, "Attempted to disqualify non existent bot");
    exit(1);
  }

  // We don't remove the bot from the dynamic array because we use the DA index
  // to address different bots. Instead it should be marked as disqualified and
  // not used.
  StopBot(state->bots.items[bot_idx]);

  state->remaining_bots--;
  nob_da_foreach(Planet, planet, &state->planets) {
    if ((unsigned)planet->owner == bot_idx + 1) {
      planet->owner = 0;
    }
  }
  nob_da_foreach(Fleet, fleet, &state->fleets) {
    if ((unsigned)fleet->owner == bot_idx + 1) {
      *fleet = state->fleets.items[--state->fleets.count];
      fleet--;
    }
  }

  UnsetBit(state->bot_bit_set, bot_idx);

  nob_log(NOB_INFO, "Disqualified bot %u.", bot_idx);
}

static inline void PrintPlanet(FILE *file, Planet planet) {
  char buf[64];
  int len = stbsp_sprintf(buf, "%s %hu %hu %hu\n", planet.print_prefix,
                          planet.owner, planet.ships, planet.growth);
  fwrite(buf, sizeof *buf, len, file);
}

static inline void PrintFleet(FILE *file, Fleet fleet) {
  char buf[64];
  int len = stbsp_sprintf(buf, "F %hu %hu %hu %hu %hu %hu\n", fleet.owner,
                          fleet.ships, fleet.src_id, fleet.dst_id, fleet.total,
                          fleet.remaining);
  fwrite(buf, sizeof *buf, len, file);
}

void sendMapToBot(GameState *state, unsigned bot_idx) {
  if (bot_idx >= state->bots.count) {
    nob_log(NOB_ERROR, "ERROR: Attempting access to non-existent bot process");
    exit(1);
  }
  if (!subprocess_alive(state->bots.items[bot_idx].process)) {
    nob_log(NOB_INFO, "Bot %u has crashed.", bot_idx);
    DisqualifyBot(state, bot_idx);
    return;
  }
  FILE *bot_stdin = subprocess_stdin(state->bots.items[bot_idx].process);

  WriteToLogFile("engine > player%u: ", bot_idx + 1);

#define MoveOwner(Type, entity)                                                \
  Type moved_##entity = *entity;                                               \
  if (bot_idx > 0 && moved_##entity.owner != 0) {                              \
    moved_##entity.owner =                                                     \
        (bot_idx * (state->bots.count - 1) + moved_##entity.owner - 1) %       \
            state->bots.count +                                                \
        1;                                                                     \
  }

  nob_da_foreach(Planet, planet, &state->planets) {
    // Each bot should see itself as bot number 1.
    MoveOwner(Planet, planet);
    PrintPlanet(bot_stdin, moved_planet);
    if (state->log_file)
      PrintPlanet(state->log_file, moved_planet);
  }
  nob_da_foreach(Fleet, fleet, &state->fleets) {
    MoveOwner(Fleet, fleet);
    PrintFleet(bot_stdin, moved_fleet);
    if (state->log_file)
      PrintFleet(state->log_file, moved_fleet);
  }

#undef MoveOwner

  fprintf(bot_stdin, MESSAGE_DELIMETER);
  WriteToLogFile(MESSAGE_DELIMETER "\n");
  fflush(bot_stdin);
}

// Return true if everythin went okay. Return false in case bot should be
// disqualified.
bool GetBotMessage(GameState *state, Nob_String_Builder *sb, unsigned bot_idx) {
  if (bot_idx >= state->bots.count) {
    nob_log(NOB_ERROR, "Tried accessing a bot OOB.");
    exit(1);
  }
  if (!subprocess_alive(state->bots.items[bot_idx].process)) {
    nob_log(NOB_INFO, "Bot %u disqualified since it's process crashed.",
            bot_idx);
    sb->count = 0;
    return false;
  }

  const unsigned max_chunk_length = 512;
  sb->count = 0;
  bool message_ended = false;

  uint64_t start = nob_nanos_since_unspecified_epoch();
  while (!message_ended) {
    nob_da_reserve(sb, sb->count + max_chunk_length);
    // Remove null terminator if it exists
    if (sb->count > 0 && nob_da_last(sb) == '\0') {
      nob_log(NOB_DEBUG, "removed null terminator");
      sb->count--;
    }
    unsigned int received =
        subprocess_read_stdout(state->bots.items[bot_idx].process,
                               sb->items + sb->count, sb->capacity - sb->count);
    if (received == 0) {
      if (nob_nanos_since_unspecified_epoch() - start > MAX_BOT_RESPONSE_TIME) {
        nob_log(NOB_INFO, "Bot %u disqualified for taking too long to reply.",
                bot_idx);
        sb->count = 0;
        return false;
      }
      sleep_ns(WAIT_SLEEP_TIME);
      continue;
    }
    sb->count += received;

    nob_log(NOB_DEBUG, "bot %u sent: |%.*s|", bot_idx, (unsigned)sb->count,
            sb->items);

    // Excluding null terminator
    const unsigned delimeter_length = NOB_ARRAY_LEN(MESSAGE_DELIMETER) - 1;
    // We need to check sb.count is at least `delimeter_length` to make sure
    // memcmp does not access OOB memory
    if (sb->count >= delimeter_length &&
        memcmp(sb->items + sb->count - delimeter_length, MESSAGE_DELIMETER,
               delimeter_length) == 0) {
      message_ended = true;
      nob_log(NOB_DEBUG, "bot %u message ended", bot_idx);
    }
  }

  Nob_String_View sv = {sb->count, sb->items};
  while (sv.count > 0) {
    Nob_String_View line = nob_sv_chop_by_delim(&sv, '\n');
    WriteToLogFile("player%u > engine: %.*s\n", bot_idx + 1, (int)line.count,
                   line.data);
  }
  return true;
}

// Return true if everythin went okay. Return false in case bot should be
// disqualified.
bool ParseBotFleets(GameState *state, Nob_String_View bot_message,
                    unsigned bot_idx) {
  if (bot_message.count < 2 ||
      (bot_message.data[0] == 'g' && bot_message.data[1] == 'o')) {
    return true;
  }

  while (bot_message.count > 1 && bot_message.data[0] != 'g' &&
         bot_message.data[1] != 'o') {
    nob_log(NOB_DEBUG, "parsing bot %u fleets", bot_idx);
    Fleet fleet;
    fleet.owner = bot_idx + 1;
    const char *start = bot_message.data;
    int parsed_int;
#define PARSE_INT(dst, err_msg)                                                \
  if (parse_int(&bot_message.data, &parsed_int) && parsed_int > 0 &&           \
      parsed_int < UINT16_MAX) {                                               \
    dst = parsed_int;                                                          \
  } else {                                                                     \
    nob_log(NOB_INFO, "Invalid bot command. " err_msg);                        \
    return false;                                                              \
  }
    PARSE_INT(fleet.src_id,
              "Source planet out of bounds or impossible to parse.");
    PARSE_INT(fleet.dst_id,
              "Destionation planet out of bounds or impossible to parse.");
    PARSE_INT(fleet.ships,
              "Amount of ships is too high, to low, or impossible to parse.");
#undef PARSE_INT

    bot_message.count -= bot_message.data - start;
    bot_message = nob_sv_trim_left(bot_message);

    if ((unsigned)fleet.src_id >= state->planets.count) {
      nob_log(NOB_INFO, "Bot tried sending fleet from nonexistent planet.");
      return false;
    }
    Planet *src = &state->planets.items[fleet.src_id];
    if (fleet.ships < 1) {
      nob_log(NOB_INFO, "Bot tried sending invalid amount of ships.");
      return false;

    } else if (fleet.src_id == fleet.dst_id) {
      nob_log(NOB_INFO, "Bot tried sending fleet from a planet itself.");
      return false;

    } else if (src->owner != fleet.owner) {
      nob_log(NOB_INFO,
              "Bot tried sending fleet from a planet it does not own.");
      return false;
    } else if ((unsigned)fleet.dst_id > state->planets.count) {
      nob_log(NOB_INFO, "Bot tried sending fleet to nonexistent planet.");
      return false;
    } else if (src->ships < fleet.ships) {
      nob_log(NOB_INFO, "Bot tried sending more ships then the planet has.");
      return false;
    }
    src->ships -= fleet.ships;
    Planet dst = state->planets.items[fleet.dst_id];

    fleet.total = ceilf(Vector2Distance(src->coords, dst.coords));
    fleet.remaining = fleet.total;

    nob_da_append(&state->fleets, fleet);
  }
  nob_log(NOB_DEBUG, "done parsing bot %u fleets", bot_idx);
  return true;
}

void PrintBotDebugMessages(GameState *state, Nob_String_Builder *sb,
                           unsigned bot_idx) {
  if (bot_idx >= state->bots.count) {
    nob_log(NOB_ERROR, "Tried accessing a bot OOB.");
    exit(1);
  }
  if (!TestBit(state->bot_bit_set, bot_idx) ||
      !subprocess_alive(state->bots.items[bot_idx].process)) {
    nob_log(NOB_WARNING, "Bot %u is not active.", bot_idx);
    return;
  }

  const unsigned max_chunk_length = 512;
  sb->count = 0;
  bool message_ended = false;

  while (!message_ended) {
    nob_da_reserve(sb, sb->count + max_chunk_length);
    // Remove null terminator if it exists
    if (sb->count > 0 && nob_da_last(sb) == '\0') {
      nob_log(NOB_DEBUG, "removed null terminator");
      sb->count--;
    }
    unsigned int received =
        subprocess_read_stderr(state->bots.items[bot_idx].process,
                               sb->items + sb->count, sb->capacity - sb->count);
    if (received == 0) {
      message_ended = true;
    }
    sb->count += received;
  }

  if (sb->count)
    nob_log(NOB_INFO, "bot %u says: |%.*s|", bot_idx, (unsigned)sb->count,
            sb->items);
}

void RunBotCycle(GameState *state, Nob_String_Builder *bot_message) {
  unsigned bot_num = 0;
  nob_da_foreach(Bot, bot, &state->bots) {
    // Skip disqualified or lost bots.
    if (TestBit(state->bot_bit_set, bot_num)) {
      nob_log(NOB_DEBUG, "sending map to bot %u", bot_num);

      sendMapToBot(state, bot_num);
    }
    bot_num++;
  }

  bot_num = 0;
  bool bot_okay = true;
  nob_da_foreach(Bot, bot, &state->bots) {
    // Skip disqualified or lost bots.
    if (TestBit(state->bot_bit_set, bot_num)) {
      bot_okay = GetBotMessage(state, bot_message, bot_num);

      if (bot_okay)
        bot_okay = ParseBotFleets(
            state, nob_sv_from_parts(bot_message->items, bot_message->count),
            bot_num);

      PrintBotDebugMessages(state, bot_message, bot_num);

      if (!bot_okay)
        DisqualifyBot(state, bot_num);

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
// Returns the amount of bots still in the game.
int AdvanceTurn(GameState *state) {
  int bot_count = 0;

  state->bot_bit_set = 0;
  nob_da_foreach(Planet, planet, &state->planets) {
    if (planet->owner != 0) {
      planet->ships += planet->growth;
      // If the player wasn't counted yet
      if (!TestBit(state->bot_bit_set, planet->owner - 1)) {
        bot_count++;
        SetBit(state->bot_bit_set, planet->owner - 1);
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

    // MAX_BOT_AMOUNT + 1 to account for neutral planets
    struct {
      int owner;
      int force;
    } forces[MAX_BOT_AMOUNT + 1];
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
    else if (!TestBit(state->bot_bit_set, state->fleets.items[i].owner - 1)) {
      bot_count++;
      SetBit(state->bot_bit_set, state->fleets.items[i].owner - 1);
    }
  }

  return bot_count;
}

void RunGame(GameState *state) {
  // Reusable string builder to hold bot messages
  Nob_String_Builder bot_message = {0};

  for (int sim_turn = 0; state->remaining_bots > 1 && sim_turn < 1000;
       sim_turn++) {
    nob_log(NOB_INFO, "Turn %d", sim_turn);
    // Bot communication
    RunBotCycle(state, &bot_message);

    // Game logic
    state->remaining_bots = AdvanceTurn(state);

    LogEntry entry = DeepCopyLogEntry((LogEntry){
        .remaining_bots = state->remaining_bots,
        .fleet_count = state->fleets.count,
        .fleets = state->fleets.items,
        .planet_count = state->planets.count,
        .planets = state->planets.items,
    });

    nob_da_append(&state->game_log, entry);
  }
  nob_log(NOB_INFO, "Game ended!");
  int winning_bot = bit_index(state->bot_bit_set);
  if (winning_bot == -1) {
    nob_log(NOB_INFO, "It's a draw!");
    state->game_log.draw = true;
  } else {
    state->game_log.winning_bot = winning_bot;
    state->game_log.draw = false;
    nob_log(NOB_INFO, "Bot %d won!", winning_bot + 1);
  }

  nob_sb_free(bot_message);
}

const unsigned version = 0;
const char magic[4] = {'p', 'l', 'w', 's'};

void WriteGameLogToFile(FILE *file, GameLog game_log) {
  union {
    float f;
    unsigned u;
  } wrt_32_float;
  uint16_t wrt_16;
  uint32_t wrt_32;
#define WRITE(var) fwrite(&var, sizeof var, 1, file)
#define WRITE_8(var) WRITE(var)
#define WRITE_16(var) (wrt_16 = htons(var), WRITE(wrt_16))
#define WRITE_32(var) (wrt_32 = htonl(var), WRITE(wrt_32))
#define WRITE_float(var) (wrt_32_float.f = var, WRITE_32(wrt_32_float.u))

  // TODO write bots data too
  WRITE(magic);
  WRITE_16(version);
  WRITE_8(game_log.draw);
  WRITE_8(game_log.winning_bot);
  WRITE_32(game_log.bots.count);
  nob_da_foreach(Bot, bot, &game_log.bots) {
    uint16_t string_length;
    if (bot->name != NULL) {
      // We don't write the null terminator
      string_length = strlen(bot->name);
      WRITE_16(string_length);
      fwrite(bot->name, sizeof *bot->name, string_length, file);
    } else {
      string_length = 0;
      WRITE_16(string_length);
    }
    string_length = strlen(bot->start_command);
    WRITE_16(string_length);
    fwrite(bot->start_command, sizeof *bot->start_command, string_length, file);
  }
  WRITE_32(game_log.count);
  nob_da_foreach(LogEntry, entry, &game_log) {
    WRITE_32(entry->remaining_bots);
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
#undef WRITE_32
#undef WRITE_16
#undef WRITE_8
#undef WRITE
}

bool ReadGameLogFromFile(FILE *file, GameLog *game_log) {
  union {
    float f;
    unsigned u;
  } read_32_float;
  uint16_t read_16;
  uint32_t read_32;

#define READ(var) fread(&var, sizeof var, 1, file)
#define READ_ERROR_CHK(read, ret_val)                                          \
  if (ret_val != read) {                                                       \
    nob_log(NOB_ERROR, "Reading error while reading from plws file.");         \
    return false;                                                              \
  }
  // fread should always return 1 on success here, since we always set the
  // number of elements to 1 and only change the size of the element
#define READ_8(var) READ_ERROR_CHK(READ(var), 1)
#define READ_16(var)                                                           \
  READ_ERROR_CHK(READ(read_16), 1);                                            \
  var = ntohs(read_16)
#define READ_32(var)                                                           \
  READ_ERROR_CHK(READ(read_32), 1);                                            \
  var = ntohl(read_32)
#define READ_float(var)                                                        \
  READ_32(read_32_float.u);                                                    \
  var = read_32_float.f

  char read_magic;
  for (unsigned i = 0; i < NOB_ARRAY_LEN(magic); i++) {
    READ_8(read_magic);
    if (read_magic != magic[i]) {
      nob_log(NOB_ERROR,
              "Provided file is not a Planet Wars serialization file.");
      return false;
    }
  }

  unsigned read_version;
  READ_16(read_version);
  if (read_version != version) {
    nob_log(NOB_ERROR,
            "Serialization file version is unsupported. File version is %u and "
            "reader version is %u",
            read_version, version);
    return false;
  }
  READ_8(game_log->draw);
  READ_8(game_log->winning_bot);
  READ_32(game_log->bots.count);
  game_log->bots.items =
      malloc(sizeof *game_log->bots.items * game_log->bots.count);
  nob_da_foreach(Bot, bot, &game_log->bots) {
    uint16_t string_length;
    READ_16(string_length);
    if (string_length) {
      // +1 to add back null terminator
      bot->name = malloc(sizeof *bot->name * string_length + 1);
      READ_ERROR_CHK(fread(bot->name, sizeof *bot->name, string_length, file),
                     string_length);
      bot->name[string_length] = '\0';
    }
    READ_16(string_length);
    bot->start_command = malloc(sizeof *bot->start_command * string_length + 1);
    READ_ERROR_CHK(fread(bot->start_command, sizeof *bot->start_command,
                         string_length, file),
                   string_length);
    bot->start_command[string_length] = '\0';
    bot->process = NULL;
  }
  READ_32(game_log->count);
  game_log->items = malloc(sizeof *game_log->items * game_log->count);
  game_log->capacity = game_log->count;
  nob_da_foreach(LogEntry, entry, game_log) {
    READ_32(entry->remaining_bots);
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
#undef READ_32
#undef READ_16
#undef READ_8
#undef READ
  return true;
}
