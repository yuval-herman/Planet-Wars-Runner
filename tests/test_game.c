#include "game.h"
#include "test-utils.h"

#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_EPSILON 0.001f

// -----------------------------------------------------------------------------
// Test Helpers & Fixtures
// -----------------------------------------------------------------------------

static int setup(void **state) {
  GameState *game_state = calloc(1, sizeof(GameState));
  assert_non_null(game_state);
  *state = game_state;
  return 0;
}

static int teardown(void **state) {
  GameState *game_state = *state;
  if (game_state) {
    FreeInnerGameState(*game_state);
    free(game_state);
  }
  return 0;
}

static void add_test_planet(GameState *state, Vector2 coords, uint8_t owner,
                            uint16_t ships, uint8_t growth) {
  Planet p = {0};
  p.coords = coords;
  p.owner = owner;
  p.ships = ships;
  p.growth = growth;
  snprintf(p.print_prefix, sizeof(p.print_prefix), "P %8.6f %8.6f", coords.x,
           coords.y);
  nob_da_append(&state->planets, p);
  if (owner != 0) {
    SetBit(state->player_bit_set, owner - 1);
  }
}

static void add_test_fleet(GameState *state, uint8_t owner, uint16_t ships,
                           uint16_t src_id, uint16_t dst_id, uint8_t total,
                           uint8_t remaining) {
  Fleet f = {0};
  f.owner = owner;
  f.ships = ships;
  f.src_id = src_id;
  f.dst_id = dst_id;
  f.total = total;
  f.remaining = remaining;
  nob_da_append(&state->fleets, f);
  if (owner != 0) {
    SetBit(state->player_bit_set, owner - 1);
  }
}

// -----------------------------------------------------------------------------
// ParsePlanetLine Tests
// -----------------------------------------------------------------------------

static void test_parse_planet_line_valid_standard(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line[] = "P 1.61359 2.65873 1 100 5\n";

  assert_true(ParsePlanetLine(line, strlen(line), &planet));
  assert_float_equal(planet.coords.x, 1.61359f, TEST_EPSILON);
  assert_float_equal(planet.coords.y, 2.65873f, TEST_EPSILON);
  assert_uint_equal(planet.owner, 1);
  assert_uint_equal(planet.ships, 100);
  assert_uint_equal(planet.growth, 5);
}

static void test_parse_planet_line_valid_neutral(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line[] = "P 0.0 0.0 0 25 0\n";

  assert_true(ParsePlanetLine(line, strlen(line), &planet));
  assert_float_equal(planet.coords.x, 0.0f, TEST_EPSILON);
  assert_float_equal(planet.coords.y, 0.0f, TEST_EPSILON);
  assert_uint_equal(planet.owner, 0);
  assert_uint_equal(planet.ships, 25);
  assert_uint_equal(planet.growth, 0);
}

static void test_parse_planet_line_valid_max_values(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  // MAX_PLAYER_AMOUNT is 32, max ships is UINT16_MAX (65535)
  char line[] = "P 999.99 888.88 32 65535 255\n";

  assert_true(ParsePlanetLine(line, strlen(line), &planet));
  assert_float_equal(planet.coords.x, 999.99f, TEST_EPSILON);
  assert_float_equal(planet.coords.y, 888.88f, TEST_EPSILON);
  assert_uint_equal(planet.owner, 32);
  assert_uint_equal(planet.ships, 65535);
  assert_uint_equal(planet.growth, 255);
}

static void test_parse_planet_line_valid_negative_coords(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line[] = "P -12.345 -67.890 2 0 10\n";

  assert_true(ParsePlanetLine(line, strlen(line), &planet));
  assert_float_equal(planet.coords.x, -12.345f, TEST_EPSILON);
  assert_float_equal(planet.coords.y, -67.890f, TEST_EPSILON);
  assert_uint_equal(planet.owner, 2);
  assert_uint_equal(planet.ships, 0);
  assert_uint_equal(planet.growth, 10);
}

static void test_parse_planet_line_valid_no_newline(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line[] = "P 1.0 2.0 1 50 5";

  assert_true(ParsePlanetLine(line, strlen(line), &planet));
  assert_float_equal(planet.coords.x, 1.0f, TEST_EPSILON);
  assert_float_equal(planet.coords.y, 2.0f, TEST_EPSILON);
  assert_uint_equal(planet.owner, 1);
  assert_uint_equal(planet.ships, 50);
  assert_uint_equal(planet.growth, 5);
}

static void test_parse_planet_line_valid_null_terminated(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line[] = "P 3.5 4.5 2 10 3\0extra";

  assert_true(ParsePlanetLine(line, strlen(line), &planet));
  assert_float_equal(planet.coords.x, 3.5f, TEST_EPSILON);
  assert_float_equal(planet.coords.y, 4.5f, TEST_EPSILON);
  assert_uint_equal(planet.owner, 2);
  assert_uint_equal(planet.ships, 10);
  assert_uint_equal(planet.growth, 3);
}

static void test_parse_planet_line_invalid_prefix(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line1[] = "F 1 10 0 1 5 3\n";
  char line2[] = "p 1.0 2.0 1 10 5\n";
  char line3[] = " 1.0 2.0 1 10 5\n";
  char line4[] = "";

  assert_false(ParsePlanetLine(line1, strlen(line1), &planet));
  assert_false(ParsePlanetLine(line2, strlen(line2), &planet));
  assert_false(ParsePlanetLine(line3, strlen(line3), &planet));
  assert_false(ParsePlanetLine(line4, 0, &planet));
}

static void test_parse_planet_line_invalid_missing_tokens(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line1[] = "P 1.0 2.0\n";
  char line2[] = "P 1.0 2.0 1\n";
  char line3[] = "P 1.0 2.0 1 50\n";

  assert_false(ParsePlanetLine(line1, strlen(line1), &planet));
  assert_false(ParsePlanetLine(line2, strlen(line2), &planet));
  assert_false(ParsePlanetLine(line3, strlen(line3), &planet));
}

static void test_parse_planet_line_invalid_extra_tokens(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line[] = "P 1.0 2.0 1 50 5 extra_garbage\n";

  assert_false(ParsePlanetLine(line, strlen(line), &planet));
}

static void test_parse_planet_line_invalid_owner_overflow(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  // Owner 33 exceeds MAX_PLAYER_AMOUNT (32)
  char line1[] = "P 1.0 2.0 33 50 5\n";
  char line2[] = "P 1.0 2.0 100 50 5\n";

  assert_false(ParsePlanetLine(line1, strlen(line1), &planet));
  assert_false(ParsePlanetLine(line2, strlen(line2), &planet));
}

static void test_parse_planet_line_invalid_ships_overflow(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  // Ships > UINT16_MAX (65535)
  char line[] = "P 1.0 2.0 1 70000 5\n";

  assert_false(ParsePlanetLine(line, strlen(line), &planet));
}

static void test_parse_planet_line_invalid_growth_overflow(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  // Growth > UINT16_MAX
  char line[] = "P 1.0 2.0 1 50 70000\n";

  assert_false(ParsePlanetLine(line, strlen(line), &planet));
}

static void test_parse_planet_line_invalid_non_numeric(void **state) {
  NOB_UNUSED(state);
  Planet planet = {0};
  char line1[] = "P abc 2.0 1 50 5\n";
  char line2[] = "P 1.0 def 1 50 5\n";
  char line3[] = "P 1.0 2.0 xyz 50 5\n";
  char line4[] = "P 1.0 2.0 1 @#$ 5\n";
  char line5[] = "P 1.0 2.0 1 50 foo\n";

  assert_false(ParsePlanetLine(line1, strlen(line1), &planet));
  assert_false(ParsePlanetLine(line2, strlen(line2), &planet));
  assert_false(ParsePlanetLine(line3, strlen(line3), &planet));
  assert_false(ParsePlanetLine(line4, strlen(line4), &planet));
  assert_false(ParsePlanetLine(line5, strlen(line5), &planet));
}

// -----------------------------------------------------------------------------
// ParseMapBuffer, ParseMapFile, MakeGame Tests
// -----------------------------------------------------------------------------

static void test_parse_map_buffer_valid_basic(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  const char map[] = "P 1.0 1.0 1 100 5\n"
                     "P 2.0 2.0 2 100 5\n"
                     "P 3.0 3.0 0 20 2\n";

  assert_true(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
  assert_uint_equal(owner_count, 2);
  assert_uint_equal(game_state->planets.count, 3);
  assert_true(TestBit(game_state->player_bit_set, 0));
  assert_true(TestBit(game_state->player_bit_set, 1));
  assert_false(TestBit(game_state->player_bit_set, 2));

  // Check prefix formatting
  assert_string_equal(game_state->planets.items[0].print_prefix,
                      "P 1.000000 1.000000");
  assert_uint_equal(game_state->planets.items[0].owner, 1);
  assert_uint_equal(game_state->planets.items[0].ships, 100);
  assert_uint_equal(game_state->planets.items[0].growth, 5);

  assert_string_equal(game_state->planets.items[2].print_prefix,
                      "P 3.000000 3.000000");
  assert_uint_equal(game_state->planets.items[2].owner, 0);
  assert_uint_equal(game_state->planets.items[2].ships, 20);
  assert_uint_equal(game_state->planets.items[2].growth, 2);
}

static void test_parse_map_buffer_multiple_planets_same_owner(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  // Player 1 has 3 planets, Player 2 has 2 planets, 2 neutral planets
  const char map[] = "P 0.0 0.0 1 50 5\n"
                     "P 1.0 1.0 1 30 3\n"
                     "P 2.0 2.0 1 20 2\n"
                     "P 3.0 3.0 2 50 5\n"
                     "P 4.0 4.0 2 30 3\n"
                     "P 5.0 5.0 0 10 1\n"
                     "P 6.0 6.0 0 15 1\n";

  assert_true(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
  assert_uint_equal(owner_count, 2);
  assert_uint_equal(game_state->planets.count, 7);
  assert_true(TestBit(game_state->player_bit_set, 0));
  assert_true(TestBit(game_state->player_bit_set, 1));
}

static void
test_parse_map_buffer_prefix_truncation_for_large_coords(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  const char map[] = "P 10.000000 10.000000 1 100 5\n";

  assert_true(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
  assert_uint_equal(game_state->planets.count, 1);

  assert_string_equal(game_state->planets.items[0].print_prefix,
                      "P 10.000000 10.000000");
}

static void test_parse_map_buffer_all_neutral(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  const char map[] = "P 0.0 0.0 0 10 1\n"
                     "P 1.0 1.0 0 20 2\n";

  assert_true(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
  assert_uint_equal(owner_count, 0);
  assert_uint_equal(game_state->planets.count, 2);
  assert_uint_equal(game_state->player_bit_set, 0);
}

static void test_parse_map_buffer_empty_and_comments(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  const char map[] = "# This is a comment\n"
                     "\n"
                     "   \n"
                     "P 5.0 5.0 1 100 5\n"
                     "# Another comment\n"
                     "P 6.0 6.0 2 100 5\n";

  assert_true(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
  assert_uint_equal(owner_count, 2);
  assert_uint_equal(game_state->planets.count, 2);
}

static void test_parse_map_buffer_crlf(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  const char map[] = "P 1.0 1.0 1 50 5\r\n"
                     "P 2.0 2.0 2 50 5\r\n";

  assert_true(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
  assert_uint_equal(owner_count, 2);
  assert_uint_equal(game_state->planets.count, 2);
}

static void test_parse_map_buffer_max_32_players(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  Nob_String_Builder sb = {0};

  for (int i = 1; i <= 32; i++) {
    nob_sb_append_cstr(&sb, nob_temp_sprintf("P %d.0 %d.0 %d 10 1\n", i, i, i));
  }

  assert_true(ParseMapBuffer(&owner_count, game_state, sb.items, sb.count));
  assert_uint_equal(owner_count, 32);
  assert_uint_equal(game_state->planets.count, 32);
  assert_uint_equal(game_state->player_bit_set, 0xFFFFFFFFU);

  nob_sb_free(sb);
}

static void test_parse_map_buffer_invalid_syntax(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  const char map[] = "P 1.0 1.0 1 50 5\n"
                     "P 2.0 invalid_number 2 50 5\n";

  assert_false(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
}

static void test_parse_map_buffer_invalid_owner_exceeds_max(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;
  const char map[] = "P 1.0 1.0 33 50 5\n";

  assert_false(ParseMapBuffer(&owner_count, game_state, map, strlen(map)));
}

static void test_parse_map_file_nonexistent(void **state) {
  GameState *game_state = *state;
  unsigned owner_count = 0;

  assert_false(
      ParseMapFile(&owner_count, game_state, "nonexistent_map_123456.txt"));
}

static void test_parse_map_file_valid_temp(void **state) {
  GameState *game_state = *state;
  const char *temp_file = "temp_test_map.txt";
  FILE *f = fopen(temp_file, "w");
  assert_non_null(f);
  fprintf(f, "P 1.0 1.0 1 100 5\nP 2.0 2.0 2 100 5\n");
  fclose(f);

  unsigned owner_count = 0;
  assert_true(ParseMapFile(&owner_count, game_state, temp_file));
  assert_uint_equal(owner_count, 2);
  assert_uint_equal(game_state->planets.count, 2);

  remove(temp_file);
}

static void test_make_game_success(void **state) {
  GameState *game_state = *state;
  const char *temp_file = "temp_make_game_map.txt";
  FILE *f = fopen(temp_file, "w");
  assert_non_null(f);
  fprintf(f, "P 1.0 1.0 1 100 5\nP 2.0 2.0 2 100 5\n");
  fclose(f);

  assert_true(MakeGame(game_state, temp_file, 2));
  assert_uint_equal(game_state->player_count, 2);
  assert_uint_equal(game_state->remaining_players, 2);
  assert_uint_equal(game_state->planets.count, 2);

  remove(temp_file);
}

static void test_make_game_player_count_mismatch(void **state) {
  GameState *game_state = *state;
  const char *temp_file = "temp_mismatch_map.txt";
  FILE *f = fopen(temp_file, "w");
  assert_non_null(f);
  // Map has 2 owners
  fprintf(f, "P 1.0 1.0 1 100 5\nP 2.0 2.0 2 100 5\n");
  fclose(f);

  // Expecting 3 players should fail
  assert_false(MakeGame(game_state, temp_file, 3));

  remove(temp_file);
}

static void test_make_game_invalid_path(void **state) {
  GameState *game_state = *state;
  assert_false(MakeGame(game_state, "nonexistent_game_map.txt", 2));
}

// -----------------------------------------------------------------------------
// GetMapRepresentation Tests (Player Perspective Translation)
// -----------------------------------------------------------------------------

static void test_get_map_representation_player_0_perspective(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){2.0f, 2.0f}, 2, 60, 5);
  add_test_planet(game_state, (Vector2){3.0f, 3.0f}, 0, 10, 0);

  add_test_fleet(game_state, 1, 15, 0, 1, 10, 8);
  add_test_fleet(game_state, 2, 20, 1, 0, 10, 5);

  Nob_String_Builder sb = {0};
  GetMapRepresentation(game_state, &sb, 0);
  nob_sb_append_null(&sb);

  // Player 0 sees actual map owners (1->1, 2->2, 0->0)
  const char expected[] = "P 1.000000 1.000000 1 50 5\n"
                          "P 2.000000 2.000000 2 60 5\n"
                          "P 3.000000 3.000000 0 10 0\n"
                          "F 1 15 0 1 10 8\n"
                          "F 2 20 1 0 10 5\n"
                          "go\n";

  assert_string_equal(sb.items, expected);
  nob_sb_free(sb);
}

static void test_get_map_representation_player_1_perspective(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){2.0f, 2.0f}, 2, 60, 5);
  add_test_planet(game_state, (Vector2){3.0f, 3.0f}, 0, 10, 0);

  add_test_fleet(game_state, 1, 15, 0, 1, 10, 8);
  add_test_fleet(game_state, 2, 20, 1, 0, 10, 5);

  Nob_String_Builder sb = {0};
  GetMapRepresentation(game_state, &sb, 1);
  nob_sb_append_null(&sb);

  // Player 1 (bot 2) must see itself (owner 2) as 1, and owner 1 as 2, neutral
  // 0 as 0
  const char expected[] = "P 1.000000 1.000000 2 50 5\n"
                          "P 2.000000 2.000000 1 60 5\n"
                          "P 3.000000 3.000000 0 10 0\n"
                          "F 2 15 0 1 10 8\n"
                          "F 1 20 1 0 10 5\n"
                          "go\n";

  assert_string_equal(sb.items, expected);
  nob_sb_free(sb);
}

static void test_get_map_representation_3_players_perspectives(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 3;
  game_state->remaining_players = 3;

  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 1, 10, 1);
  add_test_planet(game_state, (Vector2){2.0f, 2.0f}, 2, 20, 2);
  add_test_planet(game_state, (Vector2){3.0f, 3.0f}, 3, 30, 3);
  add_test_planet(game_state, (Vector2){4.0f, 4.0f}, 0, 40, 0);

  // Check player 0 perspective (identity: 1->1, 2->2, 3->3, 0->0)
  Nob_String_Builder sb0 = {0};
  GetMapRepresentation(game_state, &sb0, 0);
  nob_sb_append_null(&sb0);
  assert_string_equal(sb0.items, "P 1.000000 1.000000 1 10 1\n"
                                 "P 2.000000 2.000000 2 20 2\n"
                                 "P 3.000000 3.000000 3 30 3\n"
                                 "P 4.000000 4.000000 0 40 0\n"
                                 "go\n");
  nob_sb_free(sb0);

  // Check player 1 perspective (owner 2 sees itself as 1):
  // Formula: (1 * 2 + owner - 1) % 3 + 1
  // owner 1 -> (2 + 0) % 3 + 1 = 3
  // owner 2 -> (2 + 1) % 3 + 1 = 1
  // owner 3 -> (2 + 2) % 3 + 1 = 2
  // owner 0 -> 0
  Nob_String_Builder sb1 = {0};
  GetMapRepresentation(game_state, &sb1, 1);
  nob_sb_append_null(&sb1);
  assert_string_equal(sb1.items, "P 1.000000 1.000000 3 10 1\n"
                                 "P 2.000000 2.000000 1 20 2\n"
                                 "P 3.000000 3.000000 2 30 3\n"
                                 "P 4.000000 4.000000 0 40 0\n"
                                 "go\n");
  nob_sb_free(sb1);

  // Check player 2 perspective (owner 3 sees itself as 1):
  // Formula: (2 * 2 + owner - 1) % 3 + 1
  // owner 1 -> (4 + 0) % 3 + 1 = 2
  // owner 2 -> (4 + 1) % 3 + 1 = 3
  // owner 3 -> (4 + 2) % 3 + 1 = 1
  // owner 0 -> 0
  Nob_String_Builder sb2 = {0};
  GetMapRepresentation(game_state, &sb2, 2);
  nob_sb_append_null(&sb2);
  assert_string_equal(sb2.items, "P 1.000000 1.000000 2 10 1\n"
                                 "P 2.000000 2.000000 3 20 2\n"
                                 "P 3.000000 3.000000 1 30 3\n"
                                 "P 4.000000 4.000000 0 40 0\n"
                                 "go\n");
  nob_sb_free(sb2);
}

static void test_get_map_representation_empty_game(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  Nob_String_Builder sb = {0};
  GetMapRepresentation(game_state, &sb, 0);
  nob_sb_append_null(&sb);

  assert_string_equal(sb.items, "go\n");
  nob_sb_free(sb);
}

// -----------------------------------------------------------------------------
// SendPlayerShips & SendPlayerShipsStr Tests
// -----------------------------------------------------------------------------

static void test_send_player_ships_valid(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 owned by player 1 (player_idx 0) with 50 ships at (0, 0)
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  // Planet 1 owned by player 2 (player_idx 1) with 30 ships at (3, 4) ->
  // distance 5.0
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);

  assert_true(SendPlayerShips(game_state, 0, 0, 1, 20));
  assert_uint_equal(game_state->planets.items[0].ships, 30);
  assert_uint_equal(game_state->fleets.count, 1);

  Fleet *fleet = &game_state->fleets.items[0];
  assert_uint_equal(fleet->owner, 1);
  assert_uint_equal(fleet->ships, 20);
  assert_uint_equal(fleet->src_id, 0);
  assert_uint_equal(fleet->dst_id, 1);
  assert_uint_equal(fleet->total, 5);
  assert_uint_equal(fleet->remaining, 5);
}

static void test_send_player_ships_exact_all_ships(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 0.0f}, 2, 30, 5);

  assert_true(SendPlayerShips(game_state, 0, 0, 1, 50));
  assert_uint_equal(game_state->planets.items[0].ships, 0);
  assert_uint_equal(game_state->fleets.count, 1);
  assert_uint_equal(game_state->fleets.items[0].ships, 50);
}

static void test_send_player_ships_distance_ceiling(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // (0,0) to (1,1): dist = sqrt(2) ≈ 1.4142 -> ceilf = 2
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);

  assert_true(SendPlayerShips(game_state, 0, 0, 1, 10));
  assert_uint_equal(game_state->fleets.items[0].total, 2);
  assert_uint_equal(game_state->fleets.items[0].remaining, 2);
}

static void test_send_player_ships_invalid_src_out_of_bounds(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);

  // src_id 5 is out of bounds
  assert_false(SendPlayerShips(game_state, 0, 5, 1, 10));
  // Player 0 should be disqualified
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_invalid_dst_out_of_bounds(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);

  // dst_id 5 is out of bounds
  assert_false(SendPlayerShips(game_state, 0, 0, 5, 10));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_invalid_same_src_dst(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);

  // src_id == dst_id
  assert_false(SendPlayerShips(game_state, 0, 0, 0, 10));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_invalid_zero_ships(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);

  // ships = 0
  assert_false(SendPlayerShips(game_state, 0, 0, 1, 0));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_invalid_not_owner(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);
  add_test_planet(game_state, (Vector2){2.0f, 2.0f}, 0, 20, 2);

  // Player 0 (owner 1) tries to send ships from planet 1 (owned by 2)
  assert_false(SendPlayerShips(game_state, 0, 1, 0, 10));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_invalid_insufficient_ships(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);

  // Planet has 10 ships, trying to send 11
  assert_false(SendPlayerShips(game_state, 0, 0, 1, 11));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_str_empty_and_go(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);

  assert_true(SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("")));
  assert_true(SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("go")));
  assert_true(SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("go\n")));
  assert_uint_equal(game_state->fleets.count, 0);
  assert_uint_equal(game_state->planets.items[0].ships, 50);
}

static void test_send_player_ships_str_single_order(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);

  assert_true(
      SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("0 1 20\ngo\n")));
  assert_uint_equal(game_state->planets.items[0].ships, 30);
  assert_uint_equal(game_state->fleets.count, 1);
  assert_uint_equal(game_state->fleets.items[0].ships, 20);
}

static void test_send_player_ships_str_multiple_orders(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 0, 10, 1);

  const char orders[] = "0 1 10\n0 2 15\ngo\n";
  assert_true(SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr(orders)));
  assert_uint_equal(game_state->planets.items[0].ships, 25);
  assert_uint_equal(game_state->fleets.count, 2);
  assert_uint_equal(game_state->fleets.items[0].ships, 10);
  assert_uint_equal(game_state->fleets.items[1].ships, 15);
}

static void
test_send_player_ships_str_multiple_orders_single_line(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 0, 10, 1);

  const char orders[] = "0 1 10 0 2 15 go\n";
  assert_true(SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr(orders)));
  assert_uint_equal(game_state->planets.items[0].ships, 25);
  assert_uint_equal(game_state->fleets.count, 2);
}

static void test_send_player_ships_str_invalid_token(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);

  // Invalid non-integer token "abc"
  assert_false(
      SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("0 1 abc\ngo\n")));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_str_overflow_token(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);

  // 70000 exceeds UINT16_MAX
  assert_false(
      SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("0 1 70000\ngo\n")));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_str_invalid_ship_logic(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);

  // Same src and dst (0 0 10)
  assert_false(
      SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("0 0 10\ngo\n")));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_str_garbage_starting_with_go(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 30, 5);
  assert_true(IsPlayerAlive(game_state, 0));

  assert_false(
      SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("g 0 1 10\ngo\n")));
  assert_false(IsPlayerAlive(game_state, 0));

  SetBit(game_state->player_bit_set, 0);

  assert_false(
      SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr("1o 0 1 10\ngo\n")));
  assert_false(IsPlayerAlive(game_state, 0));
}

static void test_send_player_ships_str_multiple_orders_same_link(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){3.0f, 4.0f}, 2, 30, 5);

  const char orders[] = "0 1 10\n0 1 15\ngo\n";
  assert_true(SendPlayerShipsStr(game_state, 0, nob_sv_from_cstr(orders)));
  assert_uint_equal(game_state->planets.items[0].ships, 25);
  assert_uint_equal(game_state->fleets.count, 2);
}

// -----------------------------------------------------------------------------
// DisqualifyPlayer Tests
// -----------------------------------------------------------------------------

static void test_disqualify_player_neutralizes_planets(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Player 0 (owner 1) owns planets 0 and 1
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 30, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 1, 40, 5);
  // Player 1 (owner 2) owns planet 2
  add_test_planet(game_state, (Vector2){2.0f, 2.0f}, 2, 50, 5);
  // Neutral owns planet 3
  add_test_planet(game_state, (Vector2){3.0f, 3.0f}, 0, 10, 0);

  DisqualifyPlayer(game_state, 0);

  // Planets 0 and 1 should become neutral (owner 0)
  assert_uint_equal(game_state->planets.items[0].owner, 0);
  assert_uint_equal(game_state->planets.items[1].owner, 0);
  // Ships count on neutralized planets should be preserved
  assert_uint_equal(game_state->planets.items[0].ships, 30);
  assert_uint_equal(game_state->planets.items[1].ships, 40);

  // Planet 2 should stay owned by 2, planet 3 neutral
  assert_uint_equal(game_state->planets.items[2].owner, 2);
  assert_uint_equal(game_state->planets.items[3].owner, 0);

  assert_uint_equal(game_state->remaining_players, 1);
  assert_false(IsPlayerAlive(game_state, 0));
  assert_true(IsPlayerAlive(game_state, 1));
}

static void test_disqualify_player_removes_fleets(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 3;
  game_state->remaining_players = 3;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 50, 5);
  add_test_planet(game_state, (Vector2){2.0f, 2.0f}, 3, 50, 5);

  // Add mixed fleets: [P1, P2, P1, P3, P1]
  add_test_fleet(game_state, 1, 10, 0, 1, 5, 3); // idx 0: P1
  add_test_fleet(game_state, 2, 15, 1, 0, 5, 2); // idx 1: P2
  add_test_fleet(game_state, 1, 20, 0, 2, 5, 4); // idx 2: P1
  add_test_fleet(game_state, 3, 25, 2, 0, 5, 1); // idx 3: P3
  add_test_fleet(game_state, 1, 30, 0, 1, 5, 5); // idx 4: P1

  assert_uint_equal(game_state->fleets.count, 5);

  DisqualifyPlayer(game_state, 0);

  // All P1 fleets should be removed; only P2 and P3 remain
  assert_uint_equal(game_state->fleets.count, 2);
  for (unsigned i = 0; i < game_state->fleets.count; i++) {
    assert_uint_not_equal(game_state->fleets.items[i].owner, 1);
  }
}

static void test_disqualify_player_first_and_only_fleet(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 50, 5);

  add_test_fleet(game_state, 1, 10, 0, 1, 5, 3);

  DisqualifyPlayer(game_state, 0);

  assert_uint_equal(game_state->fleets.count, 0);
}

static void
test_disqualify_player_consecutive_fleets_and_last_element(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 0);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 50, 0);

  // Array end replacement edge cases
  add_test_fleet(game_state, 1, 10, 0, 1, 5, 2); // idx 0: P1
  add_test_fleet(game_state, 1, 10, 0, 1, 5, 3); // idx 1: P1
  add_test_fleet(game_state, 2, 10, 1, 0, 5, 4); // idx 2: P2
  add_test_fleet(game_state, 1, 10, 0, 1, 5, 5); // idx 3: P1 (last element)

  DisqualifyPlayer(game_state, 0);

  assert_uint_equal(game_state->fleets.count, 1);
  assert_uint_equal(game_state->fleets.items[0].owner, 2);
}
// -----------------------------------------------------------------------------
// AdvanceTurn Combat & Mechanics Tests
// -----------------------------------------------------------------------------

static void test_advance_turn_growth_owned_vs_neutral(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 2, 20, 3);
  add_test_planet(game_state, (Vector2){2.0f, 2.0f}, 0, 15, 4);

  AdvanceTurn(game_state);

  // Owned planets grow by growth
  assert_uint_equal(game_state->planets.items[0].ships, 15);
  assert_uint_equal(game_state->planets.items[1].ships, 23);
  // Neutral planets DO NOT grow
  assert_uint_equal(game_state->planets.items[2].ships, 15);
}

static void test_advance_turn_fleet_in_transit(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 0);
  add_test_planet(game_state, (Vector2){10.0f, 10.0f}, 2, 10, 0);

  add_test_fleet(game_state, 1, 20, 0, 1, 5, 3);

  AdvanceTurn(game_state);

  // Fleet has remaining 3 -> decrements to 2, still in transit
  assert_uint_equal(game_state->fleets.count, 1);
  assert_uint_equal(game_state->fleets.items[0].remaining, 2);
  // Target planet untouched by battle
  assert_uint_equal(game_state->planets.items[1].ships, 10);
  assert_uint_equal(game_state->planets.items[1].owner, 2);
}

static void test_advance_turn_friendly_reinforcement(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 2);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 2, 50, 5);

  // Player 1 sends 15 ships to planet 0 arriving this turn (remaining = 1)
  add_test_fleet(game_state, 1, 15, 0, 0, 5, 1);

  AdvanceTurn(game_state);

  // Planet 0: initial 10 + growth 2 + reinforcement 15 = 27
  assert_uint_equal(game_state->planets.items[0].ships, 27);
  assert_uint_equal(game_state->planets.items[0].owner, 1);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void test_advance_turn_attack_conquer(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 owned by 1, 10 ships, 0 growth
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 0);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 2, 50, 0);

  // Player 2 sends 25 ships attacking planet 0 arriving this turn
  add_test_fleet(game_state, 2, 25, 1, 0, 5, 1);

  AdvanceTurn(game_state);

  // Attacker 25 vs Defender 10 -> Captured! Owner = 2, ships = 15
  assert_uint_equal(game_state->planets.items[0].owner, 2);
  assert_uint_equal(game_state->planets.items[0].ships, 15);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void test_advance_turn_attack_defended(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 owned by 1, 30 ships, 0 growth
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 30, 0);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 2, 50, 0);

  // Player 2 sends 10 ships attacking planet 0
  add_test_fleet(game_state, 2, 10, 1, 0, 5, 1);

  AdvanceTurn(game_state);

  // Defender 30 vs Attacker 10 -> Defended! Owner = 1, ships = 20
  assert_uint_equal(game_state->planets.items[0].owner, 1);
  assert_uint_equal(game_state->planets.items[0].ships, 20);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void
test_advance_turn_simultaneous_reinforcement_and_attack(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 owned by 1, 10 ships, 0 growth
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 0);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 2, 50, 0);

  // Arriving same turn: P1 reinforcement of 15, P2 attack of 20
  add_test_fleet(game_state, 1, 15, 0, 0, 5, 1);
  add_test_fleet(game_state, 2, 20, 1, 0, 5, 1);

  AdvanceTurn(game_state);

  // Defense: 10 + 15 = 25 vs Attack: 20 -> Defender holds with 5 ships
  assert_uint_equal(game_state->planets.items[0].owner, 1);
  assert_uint_equal(game_state->planets.items[0].ships, 5);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void test_advance_turn_multiple_fleets_same_attacker(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 owned by 1, 10 ships, 0 growth
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 0);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 2, 50, 0);

  // Player 2 sends 2 fleets: 10 ships and 15 ships to planet 0
  add_test_fleet(game_state, 2, 10, 1, 0, 5, 1);
  add_test_fleet(game_state, 2, 15, 1, 0, 5, 1);

  AdvanceTurn(game_state);

  // Attacker combined force = 25 vs Defender 10 -> Captured! Owner = 2, ships =
  // 15
  assert_uint_equal(game_state->planets.items[0].owner, 2);
  assert_uint_equal(game_state->planets.items[0].ships, 15);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void test_advance_turn_3_way_battle_clear_winner(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 3;
  game_state->remaining_players = 3;

  // Planet 0 is neutral with 5 ships, 0 growth
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 0, 5, 0);
  add_test_planet(game_state, (Vector2){1.0f, 0.0f}, 1, 50, 0);
  add_test_planet(game_state, (Vector2){2.0f, 0.0f}, 2, 50, 0);
  add_test_planet(game_state, (Vector2){3.0f, 0.0f}, 3, 50, 0);

  // Attacker 1 sends 30 ships, Attacker 2 sends 20 ships, Attacker 3 sends 10
  // ships
  add_test_fleet(game_state, 1, 30, 1, 0, 5, 1);
  add_test_fleet(game_state, 2, 20, 2, 0, 5, 1);
  add_test_fleet(game_state, 3, 10, 3, 0, 5, 1);

  AdvanceTurn(game_state);

  // Forces: owner 0 (5), owner 1 (30), owner 2 (20), owner 3 (10)
  // Max = 30 (owner 1), 2nd max = 20 (owner 2)
  // Winner = owner 1, ships = 30 - 20 = 10
  assert_uint_equal(game_state->planets.items[0].owner, 1);
  assert_uint_equal(game_state->planets.items[0].ships, 10);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void test_advance_turn_3_way_battle_tie_between_attackers(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 is neutral with 5 ships, 0 growth
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 0, 5, 0);
  add_test_planet(game_state, (Vector2){1.0f, 0.0f}, 1, 50, 0);
  add_test_planet(game_state, (Vector2){2.0f, 0.0f}, 2, 50, 0);

  // Player 1 sends 20 ships, Player 2 sends 20 ships
  add_test_fleet(game_state, 1, 20, 1, 0, 5, 1);
  add_test_fleet(game_state, 2, 20, 2, 0, 5, 1);

  AdvanceTurn(game_state);

  // Tie between top two forces (20 vs 20) -> ships = 0, owner remains 0
  // (neutral)
  assert_uint_equal(game_state->planets.items[0].owner, 0);
  assert_uint_equal(game_state->planets.items[0].ships, 0);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void test_advance_turn_battle_tie_defender_and_attacker(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 owned by 1 with 20 ships, 0 growth
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 20, 0);
  add_test_planet(game_state, (Vector2){1.0f, 0.0f}, 2, 50, 0);

  // Player 2 attacks with 20 ships
  add_test_fleet(game_state, 2, 20, 1, 0, 5, 1);

  AdvanceTurn(game_state);

  // Tie between defender 20 and attacker 20 -> ships = 0, owner remains 1
  assert_uint_equal(game_state->planets.items[0].owner, 1);
  assert_uint_equal(game_state->planets.items[0].ships, 0);
  assert_uint_equal(game_state->fleets.count, 0);
}

static void
test_advance_turn_independent_battles_multiple_planets(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 (owned by 1, 10 ships, 0 growth)
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 0);
  // Planet 1 (owned by 2, 20 ships, 0 growth)
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 2, 20, 0);

  // Player 2 attacks planet 0 with 25 ships (conquers)
  add_test_fleet(game_state, 2, 25, 1, 0, 5, 1);
  // Player 1 attacks planet 1 with 5 ships (defended)
  add_test_fleet(game_state, 1, 5, 0, 1, 5, 1);

  AdvanceTurn(game_state);

  // Planet 0 conquered by 2: 25 - 10 = 15
  assert_uint_equal(game_state->planets.items[0].owner, 2);
  assert_uint_equal(game_state->planets.items[0].ships, 15);

  // Planet 1 defended by 2: 20 - 5 = 15
  assert_uint_equal(game_state->planets.items[1].owner, 2);
  assert_uint_equal(game_state->planets.items[1].ships, 15);

  assert_uint_equal(game_state->fleets.count, 0);
}

static void test_advance_turn_arriving_and_future_fleets(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 10, 0);
  add_test_planet(game_state, (Vector2){5.0f, 0.0f}, 2, 50, 0);

  // Fleet 1 arrives this turn
  add_test_fleet(game_state, 2, 15, 1, 0, 5, 1);
  // Fleet 2 arrives in 3 turns
  add_test_fleet(game_state, 1, 10, 0, 1, 5, 3);

  AdvanceTurn(game_state);

  // Fleet 1 should be removed, Fleet 2 should remain with remaining = 2
  assert_uint_equal(game_state->fleets.count, 1);
  assert_uint_equal(game_state->fleets.items[0].owner, 1);
  assert_uint_equal(game_state->fleets.items[0].remaining, 2);

  // Planet 0 conquered by 2
  assert_uint_equal(game_state->planets.items[0].owner, 2);
  assert_uint_equal(game_state->planets.items[0].ships, 5);
}

static void
test_advance_turn_player_without_planets_has_flying_fleet(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Planet 0 is owned by player 1 (player 2 owns NO planets)
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);

  // Player 2 has a fleet in flight (remaining = 2)
  add_test_fleet(game_state, 2, 20, 0, 0, 5, 2);

  AdvanceTurn(game_state);

  // Player 2 should still be alive because of active fleet!
  assert_true(IsPlayerAlive(game_state, 1));
  assert_true(IsPlayerAlive(game_state, 0));
  assert_uint_equal(game_state->remaining_players, 2);
}

static void test_advance_turn_remaining_players_when_no_fleets(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Player 1 owns all planets, Player 2 owns no planets, 0 fleets in flight
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 1, 30, 3);

  AdvanceTurn(game_state);

  // Player 1 is alive, Player 2 is eliminated
  assert_true(IsPlayerAlive(game_state, 0));
  assert_false(IsPlayerAlive(game_state, 1));

  // Expected remaining_players is 1 since player 2 had no planets
  assert_uint_equal(game_state->remaining_players, 1);
}

static void
test_advance_turn_remaining_players_whith_1_fleet_and_planet(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Player 1 has a planet but player 2 has no planets
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 5);
  add_test_planet(game_state, (Vector2){1.0f, 1.0f}, 0, 10, 3);

  // Player 2 has a fleet in flight that is now arriving and should conquer the
  // neutral planet
  add_test_fleet(game_state, 2, 20, 0, 1, 5, 1);

  AdvanceTurn(game_state);

  // Player 1 is alive, Player 2 is eliminated
  assert_true(IsPlayerAlive(game_state, 0));
  assert_true(IsPlayerAlive(game_state, 1));

  // Expected remaining_players is 2
  assert_uint_equal(game_state->remaining_players, 2);
}

static void
test_advance_turn_destroyed_arriving_fleet_eliminates_player(void **state) {
  GameState *game_state = *state;
  game_state->player_count = 2;
  game_state->remaining_players = 2;

  // Player 1 owns Planet 0 with 50 ships; Player 2 owns 0 planets
  add_test_planet(game_state, (Vector2){0.0f, 0.0f}, 1, 50, 0);

  // Player 2 sends 5 ships arriving this turn (remaining = 1)
  add_test_fleet(game_state, 2, 5, 0, 0, 5, 1);

  AdvanceTurn(game_state);

  // Player 2 fleet is destroyed, owns 0 planets and 0 fleets -> ELIMINATED
  assert_true(IsPlayerAlive(game_state, 0));
  assert_false(IsPlayerAlive(game_state, 1));
  assert_uint_equal(game_state->remaining_players, 1);
}
// -----------------------------------------------------------------------------
// Misc Unit Tests
// -----------------------------------------------------------------------------

static void test_is_player_alive_bitset(void **state) {
  GameState *game_state = *state;
  game_state->player_bit_set = 0;

  assert_false(IsPlayerAlive(game_state, 0));
  assert_false(IsPlayerAlive(game_state, 5));
  assert_false(IsPlayerAlive(game_state, 31));

  SetBit(game_state->player_bit_set, 0);
  SetBit(game_state->player_bit_set, 5);
  SetBit(game_state->player_bit_set, 31);

  assert_true(IsPlayerAlive(game_state, 0));
  assert_true(IsPlayerAlive(game_state, 5));
  assert_true(IsPlayerAlive(game_state, 31));
  assert_false(IsPlayerAlive(game_state, 1));
}

// -----------------------------------------------------------------------------
// Test Suite Definition
// -----------------------------------------------------------------------------

DEFINE_TESTS(
    game,
    // ParsePlanetLine
    cmocka_unit_test(test_parse_planet_line_valid_standard),
    cmocka_unit_test(test_parse_planet_line_valid_neutral),
    cmocka_unit_test(test_parse_planet_line_valid_max_values),
    cmocka_unit_test(test_parse_planet_line_valid_negative_coords),
    cmocka_unit_test(test_parse_planet_line_valid_no_newline),
    cmocka_unit_test(test_parse_planet_line_valid_null_terminated),
    cmocka_unit_test(test_parse_planet_line_invalid_prefix),
    cmocka_unit_test(test_parse_planet_line_invalid_missing_tokens),
    cmocka_unit_test(test_parse_planet_line_invalid_extra_tokens),
    cmocka_unit_test(test_parse_planet_line_invalid_owner_overflow),
    cmocka_unit_test(test_parse_planet_line_invalid_ships_overflow),
    cmocka_unit_test(test_parse_planet_line_invalid_growth_overflow),
    cmocka_unit_test(test_parse_planet_line_invalid_non_numeric),

    // ParseMapBuffer, ParseMapFile, MakeGame
    cmocka_unit_test_setup_teardown(test_parse_map_buffer_valid_basic, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(
        test_parse_map_buffer_multiple_planets_same_owner, setup, teardown),
    cmocka_unit_test_setup_teardown(test_parse_map_buffer_all_neutral, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_parse_map_buffer_empty_and_comments,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_parse_map_buffer_crlf, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_parse_map_buffer_max_32_players, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_parse_map_buffer_invalid_syntax, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(
        test_parse_map_buffer_prefix_truncation_for_large_coords, setup,
        teardown),
    cmocka_unit_test_setup_teardown(
        test_parse_map_buffer_invalid_owner_exceeds_max, setup, teardown),
    cmocka_unit_test_setup_teardown(test_parse_map_file_nonexistent, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_parse_map_file_valid_temp, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_make_game_success, setup, teardown),
    cmocka_unit_test_setup_teardown(test_make_game_player_count_mismatch, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_make_game_invalid_path, setup,
                                    teardown),

    // GetMapRepresentation
    cmocka_unit_test_setup_teardown(
        test_get_map_representation_player_0_perspective, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_get_map_representation_player_1_perspective, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_get_map_representation_3_players_perspectives, setup, teardown),
    cmocka_unit_test_setup_teardown(test_get_map_representation_empty_game,
                                    setup, teardown),

    // SendPlayerShips & SendPlayerShipsStr
    cmocka_unit_test_setup_teardown(test_send_player_ships_valid, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_exact_all_ships,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_distance_ceiling,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_send_player_ships_invalid_src_out_of_bounds, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_send_player_ships_invalid_dst_out_of_bounds, setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_invalid_same_src_dst,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_invalid_zero_ships,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_invalid_not_owner,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_send_player_ships_invalid_insufficient_ships, setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_str_empty_and_go,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_str_single_order,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_str_multiple_orders,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_send_player_ships_str_multiple_orders_single_line, setup,
        teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_str_invalid_token,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_send_player_ships_str_overflow_token,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_send_player_ships_str_invalid_ship_logic, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_send_player_ships_str_garbage_starting_with_go, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_send_player_ships_str_multiple_orders_same_link, setup, teardown),

    // DisqualifyPlayer
    cmocka_unit_test_setup_teardown(test_disqualify_player_neutralizes_planets,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_disqualify_player_removes_fleets,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_disqualify_player_first_and_only_fleet,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_disqualify_player_consecutive_fleets_and_last_element,
                                    setup, teardown),

    // AdvanceTurn Combat & Mechanics
    cmocka_unit_test_setup_teardown(test_advance_turn_growth_owned_vs_neutral,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_advance_turn_fleet_in_transit, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_advance_turn_friendly_reinforcement,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(test_advance_turn_attack_conquer, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(test_advance_turn_attack_defended, setup,
                                    teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_simultaneous_reinforcement_and_attack, setup,
        teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_multiple_fleets_same_attacker, setup, teardown),
    cmocka_unit_test_setup_teardown(test_advance_turn_3_way_battle_clear_winner,
                                    setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_3_way_battle_tie_between_attackers, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_battle_tie_defender_and_attacker, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_independent_battles_multiple_planets, setup,
        teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_arriving_and_future_fleets, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_player_without_planets_has_flying_fleet, setup,
        teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_remaining_players_when_no_fleets, setup, teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_remaining_players_whith_1_fleet_and_planet, setup,
        teardown),
    cmocka_unit_test_setup_teardown(
        test_advance_turn_destroyed_arriving_fleet_eliminates_player, setup,
        teardown),

    // Misc Unit Tests
    cmocka_unit_test_setup_teardown(test_is_player_alive_bitset, setup,
                                    teardown))
