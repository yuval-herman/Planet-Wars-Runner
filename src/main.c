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
#define SHIP_FONT_SIZE 20
// The lower the number the faster the game, 0 for realtime
#define GAME_SPEED 5

typedef struct {
  Vector2 min_coords;
  Vector2 max_coords;
} GameSpace;

typedef struct {
  struct subprocess_s *items;
  size_t count;
  size_t capacity;
} BotProcesses;

const Color planet_colors[] = {GRAY, RED, GREEN, BLUE, YELLOW};

PlanetDA planets = {0};
FleetsDA fleets = {0};
BotProcesses bot_processes = {0};
unsigned int turn = 0;
GameSpace game_space = {.min_coords = {INFINITY, INFINITY},
                        .max_coords = {-INFINITY, -INFINITY}};

Font font;
unsigned int tick = 0;
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
      .y = GetScreenHeight() - Remap(coords.y, game_space.min_coords.y,
                                     game_space.max_coords.y, MAP_MARGIN,
                                     GetScreenHeight() - MAP_MARGIN)};
}

// Draws a Planet on the screen.
void DrawPlanet(Planet planet) {
  Vector2 draw_coords = Game2ScreenCoords(planet.coords);

  const float draw_radius =
      BASE_PLANET_RADIUS + PLANET_RADIUS_GROWTH_CURVE -
      PLANET_RADIUS_GROWTH_CURVE / fmaxf(planet.growth, 1);

  DrawCircleV(draw_coords, draw_radius, planet_colors[planet.owner]);

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
             planet_colors[fleet.owner]);
}

bool ParseMapFile(const char *map_path, PlanetDA *planets) {
  FILE *map_file = fopen(map_path, "r");
  if (!map_file) {
    perror("Failed loading map file");
    return false;
  }

  char buf[256];
  size_t file_line = 0;
  while (fgets(buf, sizeof buf, map_file)) {
    file_line += 1;
    size_t len = strlen(buf);
    if (len == sizeof(buf) - 1 && buf[len - 1] != '\n') {
      fprintf(stderr, "Map file contains lines longer then 256 characters and "
                      "cannot be read.\n");
      return false;
    }

    // TODO support pre-existing fleets
    if (buf[0] != 'P')
      continue;

    Planet planet;
    if (!ParsePlanetLine(buf, &planet)) {
      fprintf(stderr, "Invalid map file.\nSyntax error at line %zu.\n",
              file_line);
      return false;
    }
    nob_da_append(planets, planet);
  }

  fclose(map_file);
  return true;
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

bool StartBots(char **commands) {
  Nob_Cmd command = {0};
  Nob_String_View view = {0};
  Nob_String_Builder sb = {0};
  // TODO start as many bots as the user provides, assuming the map has the
  // required amount of user owned planets.
  for (int i = 0; i < 2; i++) {
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
      fprintf(stderr, "ERROR: Failed to launch bot number %d: %s\n%s\n", 2,
              sb.items, strerror(errno));
      return false;
    } else {
      printf("Started bot %d: %s\n", i, sb.items);
    }

    nob_da_append(&bot_processes, process);
  }

  nob_cmd_free(command);
  nob_sb_free(sb);
  return true;
}

void sendMapToBot(size_t bot_idx) {
  if (bot_idx >= bot_processes.count) {
    fprintf(stderr, "ERROR: Attempting access to non-existent bot process");
    exit(1);
  }
  FILE *bot_stdin = subprocess_stdin(&bot_processes.items[bot_idx]);

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
    MoveOwner(Planet, planet) PrintPlanet(bot_stdin, moved_planet);
    printf("Sending plant %ld as owned by player %d to bot %zu\n",
           planet - planets.items, moved_planet.owner, bot_idx);
  }
  nob_da_foreach(Fleet, fleet, &fleets) {
    MoveOwner(Fleet, fleet) PrintFleet(bot_stdin, moved_fleet);
  }
  fprintf(bot_stdin, "go\n");
  fflush(bot_stdin);
}

int main(int argc, char *argv[]) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int screenWidth = 800;
  const int screenHeight = 450;

  if (argc < 4) {
    fprintf(stderr, "ERROR: Missing arguments.\n");
    fprintf(stderr, "Usage: %s <map_file> <bot1> <bot2>...\n", argv[0]);
    return 1;
  }

  printf("Loading map file from %s.\n", argv[1]);
  if (!ParseMapFile(argv[1], &planets))
    return 1;

  StartBots(argv + 2);

  ComputeGameSpace();

  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(screenWidth, screenHeight, "Planet Wars Viewer");
  font = GetFontDefault();

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    tick++;
    if (game_running && tick % GAME_SPEED == 0) {
      int bot_num = 0;
      nob_da_foreach(struct subprocess_s, process, &bot_processes) {
        printf("sending map to bot %d\n", bot_num);
        sendMapToBot(bot_num);

        char buf[4096];
        bool message_ended = false;
        Nob_String_View sv = {.data = buf, .count = 1};
        while (!message_ended) {
          // TODO use the no_wait flag for subprocess so we can kill bots taking
          // too long
          unsigned int received = subprocess_read_stdout(
              process,
              // count - 1 to overwrite previous null terminator
              buf + (sv.count ? sv.count - 1 : 0), sizeof buf - sv.count);

          sv.count += received;
          printf("bot %d sent: |%.4096s|\n", bot_num, buf);
          for (size_t i = 0; i < sv.count; i++) {
            printf("%d ", sv.data[i]);
          }
          printf("\n");
          if (sv.count == sizeof(buf) - 1 && buf[sv.count - 1] != '\n') {
            fprintf(stderr,
                    "Bot message is longer then the 4096 characters limit.\n");
            // TODO handle bot disqualification
            return 1;
          }
          // We need to check sv.count is at least 4 to make sure memcmp does
          // not access OOB memory
          if (sv.count >= 4 && memcmp(sv.data + sv.count - 4, "go\n", 4) == 0) {
            message_ended = true;
            printf("bot %d message ended\n", bot_num);
          }
        }
        // sv.count < 4 because this allows crlf as well as simple new line.
        if (sv.count < 4 && buf[0] == 'g' && buf[1] == 'o') {
          continue;
        }

        while (sv.data[0] != 'g' && sv.data[1] != 'o') {
          printf("parsing bot %d fleets\n", bot_num);
          Fleet fleet;
          fleet.owner = bot_num + 1;
          const char *start = sv.data;
          if (!(parse_int(&sv.data, &fleet.src_id) &&
                parse_int(&sv.data, &fleet.dst_id) &&
                parse_int(&sv.data, &fleet.ships))) {
            fprintf(stderr, "Invalid bot command.\n");
            // TODO handle bot disqualification
            return 1;
          }
          sv.count -= sv.data - start;
          sv = nob_sv_trim_left(sv);

          if (fleet.src_id < 0 || (size_t)fleet.src_id >= planets.count) {
            fprintf(stderr,
                    "Bot tried sending fleet from nonexistent planet.\n");
            // TODO handle bot disqualification
            return 1;
          }
          Planet *src = &planets.items[fleet.src_id];
          if (fleet.ships < 1) {
            fprintf(stderr, "Bot tried sending invalid amount of ships.\n");
            // TODO handle bot disqualification
            return 1;

          } else if (fleet.src_id == fleet.dst_id) {
            fprintf(stderr, "Bot tried sending fleet from a planet itself.\n");
            // TODO handle bot disqualification
            return 1;

          } else if (src->owner != fleet.owner) {
            fprintf(stderr,
                    "Bot tried sending fleet from a planet it does not own.\n");
            // TODO handle bot disqualification
            return 1;
          } else if (fleet.dst_id < 0 || (size_t)fleet.dst_id > planets.count) {
            fprintf(stderr, "Bot tried sending fleet to nonexistent planet.\n");
            // TODO handle bot disqualification
            return 1;
          } else if (src->ships < fleet.ships) {
            fprintf(stderr,
                    "Bot tried sending more ships then the planet has.\n");
            // TODO handle bot disqualification
            return 1;
          }
          src->ships -= fleet.ships;
          Planet dst = planets.items[fleet.dst_id];

          fleet.total = ceilf(sqrtf(powf((src->coords.x - dst.coords.x), 2) +
                                    powf((src->coords.y - dst.coords.y), 2)));
          fleet.remaining = fleet.total;

          nob_da_append(&fleets, fleet);
        }
        printf("done parsing bot %d fleets\n", bot_num);

        printf("done with bot %d, advancing to bot %d\n", bot_num, bot_num + 1);
        bot_num++;
        // TODO remove this line, or add a debug flag to it, as it is only
        // relevant for debugging when printing the contents of bot messages.
        memset(buf, 0, NOB_ARRAY_LEN(buf));
      }

      turn += 1;
      bool player_map[NOB_ARRAY_LEN(planet_colors)] = {0};
      nob_da_foreach(Planet, planet, &planets) {
        if (planet->owner != 0) {
          planet->ships += planet->growth;
          player_map[planet->owner] = true;
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
          player_map[fleet->owner] = true;
        }
      }

      int player_count = 0;
      for (size_t i = 0; i < NOB_ARRAY_LEN(player_map); i++) {
        if (player_map[i])
          player_count++;
      }
      if (player_count <= 1) {
        game_running = false;
      }
    }
    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    nob_da_foreach(Planet, planet, &planets) { DrawPlanet(*planet); }

    nob_da_foreach(Fleet, fleet, &fleets) { DrawFleet(*fleet); }

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
