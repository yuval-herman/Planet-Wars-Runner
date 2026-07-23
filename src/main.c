#include <stdbool.h>
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
#define MAP_MARGIN 50
#define CONTROLS_HEIGHT 70
#define SHIP_FONT_SIZE 20
// The lower the number the faster the game, 0 for realtime
#define GAME_SPEED 5
#define LOG_FILE "log.txt"

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
BotBitset bot_bit_set = 0;
int remaining_bots = 0;
int turn = 0;
BotProcesses bot_processes = {0};
GameSpace game_space = {.min_coords = {INFINITY, INFINITY},
                        .max_coords = {-INFINITY, -INFINITY}};

Font font;
FILE *log_file;
unsigned int frame_counter = 0;
bool game_running = true;

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
                 MAP_MARGIN + CONTROLS_HEIGHT, GetScreenHeight() - MAP_MARGIN)};
}

#define GetOwnerColor(entity)                                                  \
  (entity.owner == 0                                                           \
       ? GRAY                                                                  \
       : ColorFromHSV((((entity.owner - 1) * 7) % MAX_BOT_AMOUNT) * 360.0f /   \
                          MAX_BOT_AMOUNT,                                      \
                      1.0f, 1.0f))

// Draws a Planet on the screen.
void DrawPlanet(Planet planet) {
  Vector2 draw_coords = Game2ScreenCoords(planet.coords);

  const float draw_radius =
      BASE_PLANET_RADIUS + PLANET_RADIUS_GROWTH_CURVE -
      PLANET_RADIUS_GROWTH_CURVE / fmaxf(planet.growth, 1);

  DrawCircleV(draw_coords, draw_radius, GetOwnerColor(planet));

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
             GetOwnerColor(fleet));
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

void ComputeAttack(Fleet fleet) {
  if (fleet.remaining > 0)
    return;
  Planet *planet = &planets.items[fleet.dst_id];

  if (planet->owner == fleet.owner)
    planet->ships += fleet.ships;
  else if (planet->ships < fleet.ships) {
    planet->owner = fleet.owner;
    planet->ships = fleet.ships - planet->ships;
  } else {
    planet->ships -= fleet.ships;
  }
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
                                       subprocess_option_enable_async,
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

void sendMapToBot(size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    nob_log(NOB_ERROR, "ERROR: Attempting access to non-existent bot process");
    exit(1);
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

  fprintf(bot_stdin, "go\n");
  LogToFile("go\n\n");
  fflush(bot_stdin);
}

void GetBotMessage(Nob_String_Builder *sb, size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    nob_log(NOB_ERROR, "Tried accessing a bot OOB.");
    exit(1);
  }

  const size_t max_chunk_length = 512;
  sb->count = 0;
  bool message_ended = false;

  while (!message_ended) {
    nob_da_reserve(sb, sb->count + max_chunk_length);
    // Remove null terminator if it exists
    if (sb->count > 0 && nob_da_last(sb) == '\0') {
      nob_log(NOB_INFO, "removed null terminator");
      sb->count--;
    }
    // TODO use the no_wait flag for subprocess so we can kill bots taking
    // too long
    unsigned int received =
        subprocess_read_stdout(&bot_processes.items[bot_idx],
                               sb->items + sb->count, sb->capacity - sb->count);
    sb->count += received;

    nob_log(NOB_INFO, "bot %zu sent: |%.*s|", bot_idx, (unsigned)sb->count,
            sb->items);
    // We need to check sb.count is at least 3 to make sure memcmp does
    // not access OOB memory
    if (sb->count >= 3 && memcmp(sb->items + sb->count - 3, "go\n", 3) == 0) {
      message_ended = true;
      nob_log(NOB_INFO, "bot %zu message ended", bot_idx);
    }
  }

  Nob_String_View sv = {sb->count, sb->items};
  while (sv.count > 0) {
    Nob_String_View line = nob_sv_chop_by_delim(&sv, '\n');
    LogToFile("player%zu > engine: %.*s\n", bot_idx + 1, (int)line.count,
              line.data);
  }
}

void DisqualifyBot(size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    nob_log(NOB_ERROR, "Attempted to disqualify non existent bot");
    exit(1);
  }

  // We don't remove the bot from the dynamic array because we use the DA index
  // to address different bots. Instead it should be marked as disqualified and
  // not used.
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

  UnsetBit(bot_bit_set, bot_idx + 1);

  nob_log(NOB_INFO, "Disqualified bot %zu.\n", bot_idx);
}

void ParseBotFleets(Nob_String_View bot_message, size_t bot_idx) {
  if (bot_message.count < 2 ||
      (bot_message.data[0] == 'g' && bot_message.data[1] == 'o')) {
    return;
  }

  while (bot_message.count > 0 && bot_message.data[0] != 'g' &&
         bot_message.data[1] != 'o') {
    nob_log(NOB_INFO, "parsing bot %zu fleets", bot_idx);
    Fleet fleet;
    fleet.owner = bot_idx + 1;
    const char *start = bot_message.data;
    if (!(parse_int(&bot_message.data, &fleet.src_id) &&
          parse_int(&bot_message.data, &fleet.dst_id) &&
          parse_int(&bot_message.data, &fleet.ships))) {
      nob_log(NOB_INFO, "Invalid bot command.");
      DisqualifyBot(bot_idx);
      break;
    }
    bot_message.count -= bot_message.data - start;
    bot_message = nob_sv_trim_left(bot_message);

    if (fleet.src_id < 0 || (size_t)fleet.src_id >= planets.count) {
      nob_log(NOB_INFO, "Bot tried sending fleet from nonexistent planet.");
      DisqualifyBot(bot_idx);
      break;
    }
    Planet *src = &planets.items[fleet.src_id];
    if (fleet.ships < 1) {
      nob_log(NOB_INFO, "Bot tried sending invalid amount of ships.");
      DisqualifyBot(bot_idx);
      break;

    } else if (fleet.src_id == fleet.dst_id) {
      nob_log(NOB_INFO, "Bot tried sending fleet from a planet itself.");
      DisqualifyBot(bot_idx);
      break;

    } else if (src->owner != fleet.owner) {
      nob_log(NOB_INFO,
              "Bot tried sending fleet from a planet it does not own.");
      DisqualifyBot(bot_idx);
      break;
    } else if (fleet.dst_id < 0 || (size_t)fleet.dst_id > planets.count) {
      nob_log(NOB_INFO, "Bot tried sending fleet to nonexistent planet.");
      DisqualifyBot(bot_idx);
      break;
    } else if (src->ships < fleet.ships) {
      nob_log(NOB_INFO, "Bot tried sending more ships then the planet has.");
      DisqualifyBot(bot_idx);
      break;
    }
    src->ships -= fleet.ships;
    Planet dst = planets.items[fleet.dst_id];

    fleet.total = ceilf(Vector2Distance(src->coords, dst.coords));
    fleet.remaining = fleet.total;

    nob_da_append(&fleets, fleet);
  }
  nob_log(NOB_INFO, "done parsing bot %zu fleets", bot_idx);
}

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

  for (size_t i = 0; i < fleets.count; i++) {
    Fleet *fleet = &fleets.items[i];
    fleet->remaining--;
    if (fleet->remaining == 0) {
      // TODO attack computations should happen simultanously for all
      // fleets attacking a planet. In a situation where a player attempts
      // to defend his planet while an enemy attacks and both fleets
      // arrive at the same time, if the enemy the player have the same
      // amount of ships overall (including the player owned planet) the
      // planet stays owned by the player.
      ComputeAttack(*fleet);
      nob_da_remove_unordered(&fleets, i);
      // Remove unordered replaces the current fleet with the last one,
      // so we need to run the loop again on the same index.
      i--;
    } else {
      // If the player wasn't counted yet
      if (!TestBit(bot_bit_set, fleet->owner - 1)) {
        bot_count++;
        SetBit(bot_bit_set, fleet->owner - 1);
      }
    }
  }
  printf("advance turn bot bitset: %u\n", bot_bit_set);

  return bot_count;
}

void RunBotCycle(Nob_String_Builder *bot_message) {
  size_t bot_num = 0;
  printf("bot bitset: %u\n", bot_bit_set);
  nob_da_foreach(struct subprocess_s, process, &bot_processes) {
    // Skip disqualified or lost bots.
    if (TestBit(bot_bit_set, bot_num)) {
      nob_log(NOB_INFO, "sending map to bot %zu", bot_num);

      sendMapToBot(bot_num);
    }
    bot_num++;
  }

  bot_num = 0;
  nob_da_foreach(struct subprocess_s, process, &bot_processes) {
    // Skip disqualified or lost bots.
    if (TestBit(bot_bit_set, bot_num)) {
      GetBotMessage(bot_message, bot_num);
      ParseBotFleets(nob_sv_from_parts(bot_message->items, bot_message->count),
                     bot_num);

      nob_log(NOB_INFO, "done with bot %zu, advancing to bot %zu", bot_num,
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
    nob_log(NOB_ERROR, "Attempted accessing OOB game log entry.\n");
    exit(1);
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
  // Values for round rectanlges
  const int segments = 6;
  const float roundness = 0.4;

  // Margin between UI elements.
  const float ui_margin = 10;
  const float button_edge = 40;
  const float scrubber_height = 10;
  const int button_amount = 3;

  const Rectangle scrubber = {
      .x = MAP_MARGIN,
      .y = GetScreenHeight() - button_edge - ui_margin * 2 - scrubber_height,
      .width = GetScreenWidth() - MAP_MARGIN * 2,
      .height = scrubber_height,
  };
  DrawRectangleRoundedLines(scrubber, roundness, segments, WHITE);

  const float bar_height = scrubber.height * 2;
  Rectangle bar = {
      .x = Remap(turn, 0, game_log.count, scrubber.x,
                 scrubber.x + scrubber.width),
      .y = scrubber.y - bar_height / 2 + scrubber.height / 2,
      .width = 10,
      .height = bar_height,
  };
  DrawRectangleRounded(bar, roundness, segments, WHITE);

  Rectangle button = {.y = scrubber.y + scrubber.height + ui_margin,
                      .width = button_edge,
                      .height = button_edge};
  const int button_row_width =
      button_edge * button_amount + ui_margin * (button_amount - 1);

  // Play backwards
  button.x = (GetScreenWidth() - button_row_width) / 2.0f;
  DrawRectangleRounded(button, roundness, segments, WHITE);
  DrawTriangle((Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge * 4 / 5},
               (Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge / 5},
               (Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge / 2},
               BLACK);

  // Play/Pause toggle
  button.x += button_edge + ui_margin;
  DrawRectangleRounded(button, roundness, segments, WHITE);
  bar.width = button_edge / 5;
  bar.height = button_edge * 3 / 5;
  bar.y = button.y + button_edge / 5;
  bar.x = button.x + button_edge / 5;
  DrawRectangleRounded(bar, 0.75, segments, BLACK);
  bar.x = button.x + button_edge * 3 / 5;
  DrawRectangleRounded(bar, 0.75, segments, BLACK);

  // Play forewards
  button.x += button_edge + ui_margin;
  DrawRectangleRounded(button, roundness, segments, WHITE);
  DrawTriangle((Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge * 4 / 5},
               (Vector2){.x = button.x + button_edge * 4 / 5,
                         .y = button.y + button_edge / 2},
               (Vector2){.x = button.x + button_edge / 5,
                         .y = button.y + button_edge / 5},
               BLACK);
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

  for (int turn = 0; remaining_bots > 1 && turn < 1000; turn++) {
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

  while (!WindowShouldClose()) {
    frame_counter++;
    if (game_running && frame_counter % GAME_SPEED == 0) {
      UpdateStateFromLogEntry(turn++);
      if (remaining_bots <= 1)
        game_running = false;
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
