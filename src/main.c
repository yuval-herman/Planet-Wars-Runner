#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#include "../nob.h"
#include "raylib.h"
#include "raymath.h"
#include "subprocess.h"

#include "planet_wars.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define BASE_PLANET_RADIUS 10.0f
#define PLANET_RADIUS_GROWTH_CURVE 20.0f
#define PLANET_RING_MAX_RADIUS 2.0F
#define MAP_MARGIN 50
#define CONTROLS_HEIGHT 70
#define PLAYER_LABELS_HEIGHT 30
#define SHIP_FONT_SIZE 20
#define LOG_FILE "log.txt"
// Time in nanoseconds, currently set to 100ms
#define MAX_BOT_RESPONSE_TIME (1000 * 1000 * 100)
// The name is confusing, but this value denotes the maximum value that the
// `game_speed` variable can hold, which controls the _lowest_ bound for game
// speed, i.e. how slow can the game run.
#define MAX_GAME_SPEED_VALUE 20
// A string used to denote the end of a message by the bots and engine
#define MESSAGE_DELIMETER "go" NOB_LINE_END

// ## won't work in MSVC, we will cross that bridge when we get there.
#define LogToFile(fmt, ...)                                                    \
  do {                                                                         \
    if (log_file)                                                              \
      fprintf(log_file, fmt, ##__VA_ARGS__);                                   \
  } while (0)

#define SetBit(bitset, index)                                                  \
  do {                                                                         \
    bitset |= 1u << (index);                                                   \
  } while (0)
#define UnsetBit(bitset, index)                                                \
  do {                                                                         \
    bitset &= ~(1u << (index));                                                \
  } while (0)
#define TestBit(bitset, index) (bitset & 1u << (index))

// Returns the index of the set bit in x if only one bit is set. Else return -1.
int bit_index(uint32_t x) {
  if (x == 0 || (x & (x - 1)) != 0) {
    return -1; // not exactly one bit set
  }

  int i = 0;
  while ((x & 1u) == 0) {
    x >>= 1;
    i++;
  }
  return i;
}

typedef uint32_t BotBitset;
_Static_assert(MAX_BOT_AMOUNT <= sizeof(BotBitset) * CHAR_BIT,
               "BotBitset is not wide enough to hold max amount of bots");

typedef struct {
  Vector2 min_coords;
  Vector2 max_coords;
} GameSpace;

typedef struct {
  struct subprocess_s *items;
  size_t count;
  size_t capacity;
} BotProcesses;

// Global game state
GameLog game_log = {0};
PlanetDA planets = {0};
FleetsDA fleets = {0};
// 0 indexed. The first bot which is owner 1 is bot 0.
BotBitset bot_bit_set = 0;
int remaining_bots = 0;
int turn = 0;
BotProcesses bot_processes = {0};
GameSpace game_space = {.min_coords = {INFINITY, INFINITY},
                        .max_coords = {-INFINITY, -INFINITY}};

Font font;
FILE *log_file;
unsigned int frame_counter = 0;
// Game run speed in viewer. 0 is realtime, higher is slower.
int game_speed = 5;
bool game_running = false;
bool playing_forewards = true;

// Calculates the minimum and maximum coordinates of all planets, used to space
// planets across the entire screen.
void ComputeGameSpace() {
  nob_da_foreach(Planet, planet, &planets) {
    game_space.min_coords = Vector2Min(planet->coords, game_space.min_coords);
    game_space.max_coords = Vector2Max(planet->coords, game_space.max_coords);
  }
}

Vector2 Game2ScreenCoords(Vector2 coords) {
  return (Vector2){
      .x = Remap(coords.x, game_space.min_coords.x, game_space.max_coords.x,
                 MAP_MARGIN, GetScreenWidth() - MAP_MARGIN),
      .y = GetScreenHeight() -
           Remap(coords.y, game_space.min_coords.y, game_space.max_coords.y,
                 MAP_MARGIN + CONTROLS_HEIGHT,
                 GetScreenHeight() - MAP_MARGIN - PLAYER_LABELS_HEIGHT)};
}

inline Color GetOwnerColor(int owner) {
  return owner == 0 ? GRAY
                    : ColorFromHSV((((owner - 1) * 7) % MAX_BOT_AMOUNT) *
                                       360.0f / MAX_BOT_AMOUNT,
                                   1.0f, 1.0f);
}
// Draws a Planet on the screen.
void DrawPlanet(Planet planet) {
  Vector2 draw_coords = Game2ScreenCoords(planet.coords);

  const float draw_radius =
      BASE_PLANET_RADIUS + PLANET_RADIUS_GROWTH_CURVE -
      PLANET_RADIUS_GROWTH_CURVE / fmaxf(planet.growth, 1);

  // Wrapped time for use in repeating functions
  float r_time = fmodf(GetTime(), PLANET_RING_MAX_RADIUS);
  float ring_radius = r_time;

  DrawCircleV(draw_coords, draw_radius, GetOwnerColor(planet.owner));
  DrawCircleLinesV(
      draw_coords, draw_radius + ring_radius,
      ColorAlpha(GetOwnerColor(planet.owner),
                 (4 * r_time * (PLANET_RING_MAX_RADIUS - r_time)) /
                     (PLANET_RING_MAX_RADIUS * PLANET_RING_MAX_RADIUS)));

  const float font_size = draw_radius;
  const float spacing = 1;
  const char *ships_text = TextFormat("%d", planet.ships);

  const Vector2 text_measurements =
      MeasureTextEx(font, ships_text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(draw_coords, Vector2Scale(text_measurements, 0.5));

  DrawTextEx(font, ships_text, text_coords, font_size, spacing, BLACK);
}

void DrawFleet(Fleet fleet) {
  Vector2 draw_coords = Game2ScreenCoords(Vector2Lerp(
      planets.items[fleet.src_id].coords, planets.items[fleet.dst_id].coords,
      1 - (float)fleet.remaining / fleet.total));

  const char *ships_text = TextFormat("%d", fleet.ships);
  const float font_size = SHIP_FONT_SIZE;
  const float spacing = 1;

  const Vector2 text_measurements =
      MeasureTextEx(font, ships_text, font_size, spacing);
  const Vector2 text_coords =
      Vector2Subtract(draw_coords, Vector2Scale(text_measurements, 0.5));
  DrawTextEx(font, ships_text, text_coords, font_size, spacing,
             GetOwnerColor(fleet.owner));
}

// Parse map file and return the amount of different planet owners it has
int ParseMapFile(const char *map_path, PlanetDA *planets) {
  FILE *map_file = fopen(map_path, "r");
  if (!map_file) {
    perror("Failed loading map file");
    exit(1);
  }

  char buf[256];
  size_t file_line = 0;
  int bot_count = 0;
  bot_bit_set = 0;

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

    if (planet.owner != 0 && !TestBit(bot_bit_set, planet.owner - 1)) {
      bot_count++;
      SetBit(bot_bit_set, planet.owner - 1);
    }

    nob_da_append(planets, planet);
  }

  fclose(map_file);
  return bot_count;
}

void StartBots(char **commands, int command_amount) {
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

    nob_da_append(&bot_processes, process);
  }

  nob_cmd_free(command);
  nob_sb_free(sb);
}

void DisqualifyBot(size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    nob_log(NOB_ERROR, "Attempted to disqualify non existent bot");
    exit(1);
  }

  // We don't remove the bot from the dynamic array because we use the DA index
  // to address different bots. Instead it should be marked as disqualified and
  // not used.
  if (subprocess_alive(&bot_processes.items[bot_idx]))
    subprocess_terminate(&bot_processes.items[bot_idx]);
  subprocess_destroy(&bot_processes.items[bot_idx]);

  remaining_bots--;
  nob_da_foreach(Planet, planet, &planets) {
    if ((size_t)planet->owner == bot_idx + 1) {
      planet->owner = 0;
    }
  }
  nob_da_foreach(Fleet, fleet, &fleets) {
    if ((size_t)fleet->owner == bot_idx + 1)
      fleet->owner = 0;
  }

  UnsetBit(bot_bit_set, bot_idx);

  nob_log(NOB_INFO, "Disqualified bot %zu.", bot_idx);
}

void sendMapToBot(size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    nob_log(NOB_ERROR, "ERROR: Attempting access to non-existent bot process");
    exit(1);
  }
  if (!subprocess_alive(&bot_processes.items[bot_idx])) {
    nob_log(NOB_INFO, "Bot %zu has crashed.", bot_idx);
    DisqualifyBot(bot_idx);
    return;
  }
  FILE *bot_stdin = subprocess_stdin(&bot_processes.items[bot_idx]);

  LogToFile("engine > player%zu: ", bot_idx + 1);

#define MoveOwner(Type, entity)                                                \
  Type moved_##entity = *entity;                                               \
  if (bot_idx > 0 && moved_##entity.owner != 0) {                              \
    moved_##entity.owner =                                                     \
        (bot_idx * (bot_processes.count - 1) + moved_##entity.owner - 1) %     \
            bot_processes.count +                                              \
        1;                                                                     \
  }

  nob_da_foreach(Planet, planet, &planets) {
    // Each bot should see itself as bot number 1.
    MoveOwner(Planet, planet);
    PrintPlanet(bot_stdin, moved_planet);
    if (log_file)
      PrintPlanet(log_file, moved_planet);
  }
  nob_da_foreach(Fleet, fleet, &fleets) {
    MoveOwner(Fleet, fleet);
    PrintFleet(bot_stdin, moved_fleet);
    if (log_file)
      PrintFleet(log_file, moved_fleet);
  }

#undef MoveOwner

  fprintf(bot_stdin, MESSAGE_DELIMETER);
  LogToFile(MESSAGE_DELIMETER "\n");
  fflush(bot_stdin);
}

void PrintBotDebugMessages(Nob_String_Builder *sb, size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    nob_log(NOB_ERROR, "Tried accessing a bot OOB.");
    exit(1);
  }
  if (!TestBit(bot_bit_set, bot_idx) ||
      !subprocess_alive(&bot_processes.items[bot_idx])) {
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
        subprocess_read_stderr(&bot_processes.items[bot_idx],
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

// Return true if everythin went okay. Return false in case bot should be
// disqualified.
bool GetBotMessage(Nob_String_Builder *sb, size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    nob_log(NOB_ERROR, "Tried accessing a bot OOB.");
    exit(1);
  }
  if (!subprocess_alive(&bot_processes.items[bot_idx])) {
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
        subprocess_read_stdout(&bot_processes.items[bot_idx],
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
bool ParseBotFleets(Nob_String_View bot_message, size_t bot_idx) {
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

    if (fleet.src_id < 0 || (size_t)fleet.src_id >= planets.count) {
      nob_log(NOB_INFO, "Bot tried sending fleet from nonexistent planet.");
      return false;
    }
    Planet *src = &planets.items[fleet.src_id];
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
    } else if (fleet.dst_id < 0 || (size_t)fleet.dst_id > planets.count) {
      nob_log(NOB_INFO, "Bot tried sending fleet to nonexistent planet.");
      return false;
    } else if (src->ships < fleet.ships) {
      nob_log(NOB_INFO, "Bot tried sending more ships then the planet has.");
      return false;
    }
    src->ships -= fleet.ships;
    Planet dst = planets.items[fleet.dst_id];

    fleet.total = ceilf(Vector2Distance(src->coords, dst.coords));
    fleet.remaining = fleet.total;

    nob_da_append(&fleets, fleet);
  }
  nob_log(NOB_DEBUG, "done parsing bot %zu fleets", bot_idx);
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

typedef struct {
  int owner;
  int force;
} AttackForce;

// Runs one game turn using the planets and fleets saved.
// Returns the amount of bots still in the game.
int AdvanceTurn() {
  int bot_count = 0;

  bot_bit_set = 0;
  nob_da_foreach(Planet, planet, &planets) {
    if (planet->owner != 0) {
      planet->ships += planet->growth;
      // If the player wasn't counted yet
      if (!TestBit(bot_bit_set, planet->owner - 1)) {
        bot_count++;
        SetBit(bot_bit_set, planet->owner - 1);
      }
    }
  }

  qsort(fleets.items, fleets.count, sizeof(fleets.items[0]),
        cmp_fleet_owner_remaining);

  nob_da_foreach(Fleet, fleet, &fleets) { fleet->remaining--; }

  Fleet *current_fleet = fleets.items;
  Fleet *end_fleet = fleets.items + fleets.count;

  while (current_fleet < end_fleet && current_fleet->remaining == 0) {
    int current_dst = current_fleet->dst_id;
    Planet *planet = &planets.items[current_dst];

    // MAX_BOT_AMOUNT + 1 to account for neutral planets
    AttackForce forces[MAX_BOT_AMOUNT + 1];
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
  for (size_t i = 0; i < fleets.count; i++) {
    if (fleets.items[i].remaining == 0) {
      nob_da_remove_unordered(&fleets, i);
      // Remove unordered replaces the current fleet with the last one,
      // so we need to run the loop again on the same index.
      i--;
    }
    // If the player wasn't counted yet
    else if (!TestBit(bot_bit_set, fleets.items[i].owner - 1)) {
      bot_count++;
      SetBit(bot_bit_set, fleets.items[i].owner - 1);
    }
  }

  return bot_count;
}

void RunBotCycle(Nob_String_Builder *bot_message) {
  size_t bot_num = 0;
  nob_da_foreach(struct subprocess_s, process, &bot_processes) {
    // Skip disqualified or lost bots.
    if (TestBit(bot_bit_set, bot_num)) {
      nob_log(NOB_DEBUG, "sending map to bot %zu", bot_num);

      sendMapToBot(bot_num);
    }
    bot_num++;
  }

  bot_num = 0;
  bool bot_okay = true;
  nob_da_foreach(struct subprocess_s, process, &bot_processes) {
    // Skip disqualified or lost bots.
    if (TestBit(bot_bit_set, bot_num)) {
      bot_okay = GetBotMessage(bot_message, bot_num);

      if (bot_okay)
        bot_okay = ParseBotFleets(
            nob_sv_from_parts(bot_message->items, bot_message->count), bot_num);

      PrintBotDebugMessages(bot_message, bot_num);

      if (!bot_okay)
        DisqualifyBot(bot_num);

      nob_log(NOB_DEBUG, "done with bot %zu, advancing to bot %zu", bot_num,
              bot_num + 1);
    }
    bot_num++;
  }
}

void Setup(int argc, char *argv[]) {
  if (argc < 4) {
    nob_log(NOB_ERROR, "Missing arguments.");
    nob_log(NOB_ERROR, "Usage: %s <map_file> <bot1> <bot2>...", argv[0]);
    exit(1);
  }

  log_file = fopen(LOG_FILE, "w");
  if (!log_file) {
    nob_log(NOB_WARNING, "Failed to open log file: %s", strerror(errno));
  }
  LogToFile("initializing\n");

  nob_log(NOB_INFO, "Loading map file from %s.", argv[1]);
  int bot_count = argc - 2;
  int owner_count = ParseMapFile(argv[1], &planets);
  if (owner_count != bot_count) {
    nob_log(NOB_ERROR,
            "Provided map requires %d player, yet %d bots were given as "
            "arguments.",
            owner_count, bot_count);
    exit(1);
  }

  StartBots(argv + 2, bot_count);
  remaining_bots = bot_processes.count;

  ComputeGameSpace();
}

void UpdateStateFromLogEntry(size_t entry_idx) {
  if (entry_idx >= game_log.count) {
    nob_log(NOB_WARNING, "Attempted accessing OOB game log entry.");
    return;
  }

  LogEntry entry = game_log.items[entry_idx];
  remaining_bots = entry.remaining_bots;

  fleets.count = entry.fleet_count;
  memcpy(fleets.items, entry.fleets, sizeof *fleets.items * entry.fleet_count);
  planets.count = entry.planet_count;
  memcpy(planets.items, entry.planets,
         sizeof *planets.items * entry.planet_count);
}

void FreeGameLog() {
  nob_da_foreach(LogEntry, entry, &game_log) {
    free(entry->fleets);
    free(entry->planets);
  }
  nob_da_free(game_log);
}

void StopAndFreeBots() {
  nob_da_foreach(struct subprocess_s, process, &bot_processes) {
    subprocess_terminate(process);
    subprocess_destroy(process);
  }

  nob_da_free(bot_processes);
}

void DrawControls() {
  static bool is_scrubber_pressed = false;
  // Values for round rectanlges
  const int segments = 6;
  const float roundness = 0.4;

  // Margin between UI elements.
  const float ui_margin = 10;
  const float button_edge = 40;
  const float scrubber_height = 10;
  const int button_amount = 3;

  const Color button_color = WHITE;
  const Color button_pressed_color = GRAY;

  const Rectangle scrubber = {
      .x = MAP_MARGIN,
      .y = GetScreenHeight() - button_edge - ui_margin * 2 - scrubber_height,
      .width = GetScreenWidth() - MAP_MARGIN * 2,
      .height = scrubber_height,
  };
  DrawRectangleRoundedLines(scrubber, roundness, segments, WHITE);

  const float bar_height = scrubber.height * 2;
  const float bar_width = 40;

  Rectangle bar = {
      .x = Remap(turn, 0, game_log.count - 1, scrubber.x - bar_width / 2,
                 scrubber.x + scrubber.width - bar_width / 2),
      .y = scrubber.y - bar_height / 2 + scrubber.height / 2,
      .width = bar_width,
      .height = bar_height,
  };

  if (CheckCollisionPointRec(GetMousePosition(), scrubber) &&
      IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    is_scrubber_pressed = true;
    game_running = false;
  }

  if (is_scrubber_pressed) {
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      is_scrubber_pressed = false;
    } else {
      turn =
          Remap(fmin(scrubber.x + scrubber.width,
                     fmax(scrubber.x, GetMousePosition().x)),
                scrubber.x, scrubber.x + scrubber.width, 0, game_log.count - 1);
      UpdateStateFromLogEntry(turn);
    }
  }

  const float font_size = 20;
  const float spacing = 1;
  const char *turn_text = TextFormat("%d", turn);

  Vector2 text_measurements =
      MeasureTextEx(font, turn_text, font_size, spacing);
  const Vector2 text_coords = {
      .x = bar.x + (bar_width - text_measurements.x) / 2,
      .y = bar.y + (bar_height - text_measurements.y) / 2};

  DrawRectangleRounded(bar, roundness, segments, WHITE);
  DrawTextEx(font, turn_text, text_coords, font_size, spacing, BLACK);

  Rectangle button = {.y = scrubber.y + scrubber.height + ui_margin,
                      .width = button_edge,
                      .height = button_edge};
  Color button_computed_color;
  const int button_row_width =
      button_edge * button_amount + ui_margin * (button_amount - 1);

#define HandleMousePress(action)                                               \
  do {                                                                         \
    if (CheckCollisionPointRec(GetMousePosition(), button)) {                  \
      button_computed_color =                                                  \
          ColorLerp(button_color, button_pressed_color,                        \
                    IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1 : 0.3);           \
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {                          \
        action;                                                                \
      }                                                                        \
    } else {                                                                   \
      button_computed_color = button_color;                                    \
    }                                                                          \
  } while (0)

  // Play backwards
  button.x = (GetScreenWidth() - button_row_width) / 2.0f;
  HandleMousePress(if (turn > 0) {
    playing_forewards = false;
    game_running = true;
  });
  DrawRectangleRounded(button, roundness, segments, button_computed_color);
  DrawTriangle((Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge * 4 / 5},
               (Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge / 5},
               (Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge / 2},
               BLACK);

  // Play/Pause toggle
  button.x += button_edge + ui_margin;
  HandleMousePress(game_running = !game_running);
  DrawRectangleRounded(button, roundness, segments, button_computed_color);
  bar.width = button_edge / 5;
  bar.height = button_edge * 3 / 5;
  bar.y = button.y + button_edge / 5;
  bar.x = button.x + button_edge / 5;
  DrawRectangleRounded(bar, 0.75, segments, BLACK);
  bar.x = button.x + button_edge * 3 / 5;
  DrawRectangleRounded(bar, 0.75, segments, BLACK);

  // Play forewards
  button.x += button_edge + ui_margin;
  HandleMousePress(if ((size_t)turn < game_log.count) {
    playing_forewards = true;
    game_running = true;
  });
  DrawRectangleRounded(button, roundness, segments, button_computed_color);
  DrawTriangle((Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge * 4 / 5},
               (Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge / 2},
               (Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge / 5},
               BLACK);

// TODO add slider or other UI to control replay speed
#undef HandleMousePress

  const float indicator_size = 15;
  const float margin_between_players = 25;
  const float text_margin = 8;

  const char *player_text = "P99";
  text_measurements = MeasureTextEx(font, player_text, font_size, spacing);
  float total_labels_width = (text_measurements.x + indicator_size +
                              text_margin + margin_between_players) *
                                 bot_processes.count -
                             margin_between_players;

  Rectangle color_indicator = {.x = (GetScreenWidth() - total_labels_width) / 2,
                               .y = 25,
                               .width = indicator_size,
                               .height = indicator_size};

  for (size_t i = 0; i < bot_processes.count; i++) {
    Color player_color = GetOwnerColor(i + 1);

    DrawRectangleRec(color_indicator, player_color);
    // Indicator border
    DrawRectangleLinesEx(color_indicator, 1.0f, RAYWHITE);

    const char *player_text = TextFormat("P%zu", i + 1);
    Vector2 text_measurements =
        MeasureTextEx(font, player_text, font_size, spacing);

    Vector2 text_pos = {
        .x = color_indicator.x + color_indicator.width + text_margin,
        .y = color_indicator.y +
             (color_indicator.height - text_measurements.y) / 2};
    DrawTextEx(font, player_text, text_pos, font_size, spacing, WHITE);

    color_indicator.x += color_indicator.width + text_margin +
                         text_measurements.x + margin_between_players;
  }
}

int main(int argc, char *argv[]) {
  Setup(argc, argv);

  const int screenWidth = 800;
  const int screenHeight = 450;

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Planet Wars Viewer");
  font = GetFontDefault();

  SetTargetFPS(60);

  // Reusable string builder to hold bot messages
  Nob_String_Builder bot_message = {0};

  for (int sim_turn = 0; remaining_bots > 1 && sim_turn < 1000; sim_turn++) {
    nob_log(NOB_INFO, "Turn %d", sim_turn);
    // Bot communication
    RunBotCycle(&bot_message);

    // Game logic
    remaining_bots = AdvanceTurn();

    LogEntry entry = {.remaining_bots = remaining_bots,
                      .fleet_count = fleets.count,
                      .fleets = malloc(sizeof *fleets.items * fleets.count),
                      .planet_count = planets.count,
                      .planets = malloc(sizeof *planets.items * planets.count)};
    memcpy(entry.fleets, fleets.items, sizeof *fleets.items * fleets.count);
    memcpy(entry.planets, planets.items, sizeof *planets.items * planets.count);

    nob_da_append(&game_log, entry);
  }
  nob_log(NOB_INFO, "Game ended!");
  nob_log(NOB_INFO, "Bot %d won!", bit_index(bot_bit_set) + 1);

  // Set the game to the first turn
  UpdateStateFromLogEntry(turn = 0);

  while (!WindowShouldClose()) {
    frame_counter++;
    if (game_running &&
        (game_speed == 0 || frame_counter % abs(game_speed) == 0)) {
      UpdateStateFromLogEntry(turn);
      if (playing_forewards && (size_t)turn < game_log.count)
        turn++;
      else if (turn > 0)
        turn--;
      if ((size_t)turn >= game_log.count - 1 || turn == 0)
        game_running = false;
    }

    if (IsKeyPressed(KEY_RIGHT)) {
      game_running = false;
      turn++;
      if ((size_t)turn >= game_log.count)
        turn = game_log.count - 1;
      UpdateStateFromLogEntry(turn);
    } else if (IsKeyPressed(KEY_LEFT)) {
      game_running = false;
      turn--;
      if (turn < 0)
        turn = 0;
      UpdateStateFromLogEntry(turn);
    } else if (IsKeyPressed(KEY_SPACE)) {
      game_running = !game_running;
    }
    if (IsKeyPressed(KEY_UP) && game_speed > 0) {
      game_speed--;
    } else if (IsKeyPressed(KEY_DOWN) && game_speed < MAX_GAME_SPEED_VALUE) {
      game_speed++;
    }

    BeginDrawing();

    ClearBackground(BLACK);

    nob_da_foreach(Planet, planet, &planets) { DrawPlanet(*planet); }

    nob_da_foreach(Fleet, fleet, &fleets) { DrawFleet(*fleet); }

    DrawControls();
    EndDrawing();
  }

  CloseWindow();

  FreeGameLog();
  StopAndFreeBots();
  nob_sb_free(bot_message);
  nob_da_free(planets);
  nob_da_free(fleets);

  fclose(log_file);
  return 0;
}
