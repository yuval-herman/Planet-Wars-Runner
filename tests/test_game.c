#include "game.h"

#include "test-utils.h"

#include <cmocka.h>

const char test_map_buffer[] = "P 11.6135908004 11.6587374197 0 119 0\n"
                               "P 1.2902863101 9.04078582767 1 100 5\n"
                               "P 21.9368952907 14.2766890117 2 100 5\n"
                               "P 5.64835767563 18.2659924733 0 21 4\n"
                               "P 17.5788239251 5.05148236609 0 21 4\n";

static int setup(void **state) {
  GameState *game_state = calloc(1, sizeof(GameState));

  assert_non_null(game_state);

  *state = game_state;

  return 0;
}

static int teardown(void **state) {
  GameState *game_state = *state;
  FreeInnerGameState(*game_state);
  free(game_state);

  return 0;
}

static void parse_planet_line_valid(void **state) {
  NOB_UNUSED(state);

  Planet planet;
  char line[] = "P 11.61 11.65 0 119 2\n";

  assert_true(ParsePlanetLine(line, NOB_ARRAY_LEN(line), &planet));

  assert_float_equal(planet.coords.x, 11.61, EPSILON);
  assert_float_equal(planet.coords.y, 11.65, EPSILON);
  assert_uint_equal(planet.owner, 0);
  assert_uint_equal(planet.ships, 119);
  assert_uint_equal(planet.growth, 2);
}

static void parse_planet_line_invalid(void **state) {
  NOB_UNUSED(state);

  Planet planet;
  // Added extra number
  char line1[] = "P 11.61 2 11.65 0 119 2\n";
  // Added invalid character
  char line2[] = "P 11.61 /11.65 0 119 2\n";
  // Removed a number
  char line3[] = "P 11.61 0 119 2\n";

  // TODO: After converting planet_wars to use nob.h for logging and such,
  // intercept logs to verify correct errors.
  assert_false(ParsePlanetLine(line1, NOB_ARRAY_LEN(line1), &planet));
  assert_false(ParsePlanetLine(line2, NOB_ARRAY_LEN(line2), &planet));
  assert_false(ParsePlanetLine(line3, NOB_ARRAY_LEN(line3), &planet));
}

static void run_game_no_instructions(void **state) {
  GameState *game_state = *state;
  unsigned player_count;

  assert_true(ParseMapBuffer(&player_count, game_state, test_map_buffer,
                             NOB_ARRAY_LEN(test_map_buffer)));

  AdvanceTurn(game_state);
}

DEFINE_TESTS(game, cmocka_unit_test(parse_planet_line_valid),
             cmocka_unit_test(parse_planet_line_invalid),
             cmocka_unit_test_setup_teardown(run_game_no_instructions, setup,
                                             teardown))
