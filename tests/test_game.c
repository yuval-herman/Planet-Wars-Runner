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

static void test_run_game_no_instructions(void **state) {
  GameState *game_state = *state;
  unsigned player_count;

  assert_true(ParseMapBuffer(&player_count, game_state, test_map_buffer,
                             NOB_ARRAY_LEN(test_map_buffer)));

  AdvanceTurn(game_state);
}

DEFINE_TESTS(game,
             cmocka_unit_test_setup_teardown(test_run_game_no_instructions,
                                             setup, teardown))
