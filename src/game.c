#include "game.h"
#include "subprocess.h"
#include "utils.h"
#include <string.h>
#include <time.h>

// ## won't work in MSVC, we will cross that bridge when we get there.
#define LogToFile(fmt, ...)                                                    \
  do {                                                                         \
    if (state->log_file)                                                       \
      fprintf(state->log_file, fmt, ##__VA_ARGS__);                            \
  } while (0)

void StartBots(GameState *state, const char *const commands[],
               int command_amount) {
  if (command_amount > MAX_BOT_AMOUNT) {
    nob_log(NOB_ERROR,
            "Provided more then %d bots, which is the maximum supprted number "
            "allowed",
            MAX_BOT_AMOUNT);
    exit(1);
  }

  Nob_Cmd command = {0};
  Nob_String_View view = {0};
  Nob_String_Builder sb = {0};
  for (int i = 0; i < command_amount; i++) {
    sb.count = 0;
    command.count = 0;
    view = nob_sv_from_cstr(commands[i]);

    while (view.count > 0) {
      nob_cmd_append(&command,
                     nob_temp_sv_to_cstr(nob_sv_chop_by_delim(&view, ' ')));
    }

    nob_cmd_append(&command, NULL);

    struct subprocess_s process;
    int result = subprocess_create(command.items,
                                   subprocess_option_search_user_path |
                                       subprocess_option_inherit_environment |
                                       subprocess_option_enable_async |
                                       subprocess_option_enable_async_no_wait,
                                   &process);

    nob_cmd_render(command, &sb);
    nob_sb_append_null(&sb);
    if (0 != result) {
      nob_log(NOB_ERROR, "ERROR: Failed to launch bot number %d: %s\n%s", 2,
              sb.items, strerror(errno));
      exit(1);
    } else {
      nob_log(NOB_INFO, "Started bot %d: %s", i, sb.items);
    }

    nob_da_append(&state->bot_processes, process);
  }

  nob_cmd_free(command);
  nob_sb_free(sb);
}

// Parse map file, saving the map into the game state and returning the amount
// of different planet owners it has
int ParseMapFile(GameState *state, const char *map_path) {
  FILE *map_file = fopen(map_path, "r");
  if (!map_file) {
    perror("Failed loading map file");
    exit(1);
  }

  char buf[256];
  size_t file_line = 0;
  int bot_count = 0;
  state->bot_bit_set = 0;

  while (fgets(buf, sizeof buf, map_file)) {
    file_line += 1;
    size_t len = strlen(buf);
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
      nob_log(NOB_ERROR, "Invalid map file.\nSyntax error at line %zu.",
              file_line);
      exit(1);
    }
    if (planet.owner > MAX_BOT_AMOUNT) {
      nob_log(NOB_ERROR,
              "Map containes more owners then the max bot count. Encountered "
              "in line: %zu\nOwner found: %d\nMax bot count: %d",
              file_line, planet.owner, MAX_BOT_AMOUNT);
      exit(1);
    }

    if (planet.owner != 0 && !TestBit(state->bot_bit_set, planet.owner - 1)) {
      bot_count++;
      SetBit(state->bot_bit_set, planet.owner - 1);
    }

    nob_da_append(&state->planets, planet);
  }

  fclose(map_file);
  return bot_count;
}

GameState MakeGame(const char *map_file_path,
                   const char *const bot_start_commands[], int bot_count) {
  GameState state = {0};
  state.log_file = fopen(LOG_FILE, "w");
  if (!state.log_file) {
    nob_log(NOB_WARNING, "Failed to open log file: %s", strerror(errno));
  }
  fprintf(state.log_file, "initializing\n");

  nob_log(NOB_INFO, "Loading map file from %s.", map_file_path);
  int owner_count = ParseMapFile(&state, map_file_path);
  if (owner_count != bot_count) {
    nob_log(NOB_ERROR,
            "Provided map requires %d player, yet %d bots were given as "
            "arguments.",
            owner_count, bot_count);
    exit(1);
  }

  StartBots(&state, bot_start_commands, bot_count);
  state.remaining_bots = state.bot_processes.count;

  return state;
}

void DisqualifyBot(GameState *state, size_t bot_idx) {
  if (bot_idx >= state->bot_processes.count) {
    nob_log(NOB_ERROR, "Attempted to disqualify non existent bot");
    exit(1);
  }

  // We don't remove the bot from the dynamic array because we use the DA index
  // to address different bots. Instead it should be marked as disqualified and
  // not used.
  if (subprocess_alive(&state->bot_processes.items[bot_idx]))
    subprocess_terminate(&state->bot_processes.items[bot_idx]);
  subprocess_destroy(&state->bot_processes.items[bot_idx]);

  state->remaining_bots--;
  nob_da_foreach(Planet, planet, &state->planets) {
    if ((size_t)planet->owner == bot_idx + 1) {
      planet->owner = 0;
    }
  }
  nob_da_foreach(Fleet, fleet, &state->fleets) {
    if ((size_t)fleet->owner == bot_idx + 1)
      fleet->owner = 0;
  }

  UnsetBit(state->bot_bit_set, bot_idx);

  nob_log(NOB_INFO, "Disqualified bot %zu.", bot_idx);
}

void sendMapToBot(GameState *state, size_t bot_idx) {
  if (bot_idx >= state->bot_processes.count) {
    nob_log(NOB_ERROR, "ERROR: Attempting access to non-existent bot process");
    exit(1);
  }
  if (!subprocess_alive(&state->bot_processes.items[bot_idx])) {
    nob_log(NOB_INFO, "Bot %zu has crashed.", bot_idx);
    DisqualifyBot(state, bot_idx);
    return;
  }
  FILE *bot_stdin = subprocess_stdin(&state->bot_processes.items[bot_idx]);

  LogToFile("engine > player%zu: ", bot_idx + 1);

#define MoveOwner(Type, entity)                                                \
  Type moved_##entity = *entity;                                               \
  if (bot_idx > 0 && moved_##entity.owner != 0) {                              \
    moved_##entity.owner = (bot_idx * (state->bot_processes.count - 1) +       \
                            moved_##entity.owner - 1) %                        \
                               state->bot_processes.count +                    \
                           1;                                                  \
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
  LogToFile(MESSAGE_DELIMETER "\n");
  fflush(bot_stdin);
}

// Return true if everythin went okay. Return false in case bot should be
// disqualified.
bool GetBotMessage(GameState *state, Nob_String_Builder *sb, size_t bot_idx) {
  if (bot_idx >= state->bot_processes.count) {
    nob_log(NOB_ERROR, "Tried accessing a bot OOB.");
    exit(1);
  }
  if (!subprocess_alive(&state->bot_processes.items[bot_idx])) {
    nob_log(NOB_INFO, "Bot %zu disqualified since it's process crashed.",
            bot_idx);
    sb->count = 0;
    return false;
  }

  const size_t max_chunk_length = 512;
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
        subprocess_read_stdout(&state->bot_processes.items[bot_idx],
                               sb->items + sb->count, sb->capacity - sb->count);
    if (received == 0) {
      if (nob_nanos_since_unspecified_epoch() - start > MAX_BOT_RESPONSE_TIME) {
        nob_log(NOB_INFO, "Bot %zu disqualified for taking too long to reply.",
                bot_idx);
        sb->count = 0;
        return false;
      }
      sleep_ms(2);
      continue;
    }
    sb->count += received;

    nob_log(NOB_DEBUG, "bot %zu sent: |%.*s|", bot_idx, (unsigned)sb->count,
            sb->items);

    // Excluding null terminator
    const size_t delimeter_length = NOB_ARRAY_LEN(MESSAGE_DELIMETER) - 1;
    // We need to check sb.count is at least `delimeter_length` to make sure
    // memcmp does not access OOB memory
    if (sb->count >= delimeter_length &&
        memcmp(sb->items + sb->count - delimeter_length, MESSAGE_DELIMETER,
               delimeter_length) == 0) {
      message_ended = true;
      nob_log(NOB_DEBUG, "bot %zu message ended", bot_idx);
    }
  }

  Nob_String_View sv = {sb->count, sb->items};
  while (sv.count > 0) {
    Nob_String_View line = nob_sv_chop_by_delim(&sv, '\n');
    LogToFile("player%zu > engine: %.*s\n", bot_idx + 1, (int)line.count,
              line.data);
  }
  return true;
}

// Return true if everythin went okay. Return false in case bot should be
// disqualified.
bool ParseBotFleets(GameState *state, Nob_String_View bot_message,
                    size_t bot_idx) {
  if (bot_message.count < 2 ||
      (bot_message.data[0] == 'g' && bot_message.data[1] == 'o')) {
    return true;
  }

  while (bot_message.count > 1 && bot_message.data[0] != 'g' &&
         bot_message.data[1] != 'o') {
    nob_log(NOB_DEBUG, "parsing bot %zu fleets", bot_idx);
    Fleet fleet;
    fleet.owner = bot_idx + 1;
    const char *start = bot_message.data;
    if (!(parse_int(&bot_message.data, &fleet.src_id) &&
          parse_int(&bot_message.data, &fleet.dst_id) &&
          parse_int(&bot_message.data, &fleet.ships))) {
      nob_log(NOB_INFO, "Invalid bot command.");
      return false;
    }
    bot_message.count -= bot_message.data - start;
    bot_message = nob_sv_trim_left(bot_message);

    if (fleet.src_id < 0 || (size_t)fleet.src_id >= state->planets.count) {
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
    } else if (fleet.dst_id < 0 ||
               (size_t)fleet.dst_id > state->planets.count) {
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
  nob_log(NOB_DEBUG, "done parsing bot %zu fleets", bot_idx);
  return true;
}

void PrintBotDebugMessages(GameState *state, Nob_String_Builder *sb,
                           size_t bot_idx) {
  if (bot_idx >= state->bot_processes.count) {
    nob_log(NOB_ERROR, "Tried accessing a bot OOB.");
    exit(1);
  }
  if (!TestBit(state->bot_bit_set, bot_idx) ||
      !subprocess_alive(&state->bot_processes.items[bot_idx])) {
    nob_log(NOB_WARNING, "Bot %zu is not active.", bot_idx);
    return;
  }

  const size_t max_chunk_length = 512;
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
        subprocess_read_stderr(&state->bot_processes.items[bot_idx],
                               sb->items + sb->count, sb->capacity - sb->count);
    if (received == 0) {
      message_ended = true;
    }
    sb->count += received;
  }

  if (sb->count)
    nob_log(NOB_INFO, "bot %zu says: |%.*s|", bot_idx, (unsigned)sb->count,
            sb->items);
}

void RunBotCycle(GameState *state, Nob_String_Builder *bot_message) {
  size_t bot_num = 0;
  nob_da_foreach(struct subprocess_s, process, &state->bot_processes) {
    // Skip disqualified or lost bots.
    if (TestBit(state->bot_bit_set, bot_num)) {
      nob_log(NOB_DEBUG, "sending map to bot %zu", bot_num);

      sendMapToBot(state, bot_num);
    }
    bot_num++;
  }

  bot_num = 0;
  bool bot_okay = true;
  nob_da_foreach(struct subprocess_s, process, &state->bot_processes) {
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

      nob_log(NOB_DEBUG, "done with bot %zu, advancing to bot %zu", bot_num,
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
  for (size_t i = 0; i < state->fleets.count; i++) {
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

    LogEntry entry = {
        .remaining_bots = state->remaining_bots,
        .fleet_count = state->fleets.count,
        .fleets = malloc(sizeof *state->fleets.items * state->fleets.count),
        .planet_count = state->planets.count,
        .planets = malloc(sizeof *state->planets.items * state->planets.count)};
    memcpy(entry.fleets, state->fleets.items,
           sizeof *state->fleets.items * state->fleets.count);
    memcpy(entry.planets, state->planets.items,
           sizeof *state->planets.items * state->planets.count);

    nob_da_append(&state->game_log, entry);
  }
  nob_log(NOB_INFO, "Game ended!");
  nob_log(NOB_INFO, "Bot %d won!", bit_index(state->bot_bit_set) + 1);

  nob_sb_free(bot_message);
}

void FreeState(GameState *state) {
  FreeGameLog(&state->game_log);
  nob_da_free(state->planets);
  nob_da_free(state->fleets);
  StopAndFreeBots(&state->bot_processes);
  fclose(state->log_file);

  memset(state, 0, sizeof *state);
}

void FreeGameLog(GameLog *game_log) {
  nob_da_foreach(LogEntry, entry, game_log) {
    free(entry->fleets);
    free(entry->planets);
  }
  nob_da_free(*game_log);
}

void StopAndFreeBots(BotProcesses *bot_processes) {
  nob_da_foreach(struct subprocess_s, process, bot_processes) {
    subprocess_terminate(process);
    subprocess_destroy(process);
  }

  nob_da_free(*bot_processes);
  bot_processes->capacity = 0;
  bot_processes->count = 0;
  bot_processes->items = NULL;
}

void UpdateStateFromLogEntry(GameState *state, size_t entry_idx) {
  if (entry_idx >= state->game_log.count) {
    nob_log(NOB_WARNING, "Attempted accessing OOB game log entry.");
    return;
  }

  LogEntry entry = state->game_log.items[entry_idx];
  state->remaining_bots = entry.remaining_bots;

  state->fleets.count = entry.fleet_count;
  memcpy(state->fleets.items, entry.fleets,
         sizeof *state->fleets.items * entry.fleet_count);
  state->planets.count = entry.planet_count;
  memcpy(state->planets.items, entry.planets,
         sizeof *state->planets.items * entry.planet_count);
}
