#include "game.h"

#include "test-utils.h"

#include <cmocka.h>

static int setup(void **state) {
  GameState *game_state = calloc(1, sizeof(GameState));

  assert_non_null(game_state);

  assert_true(MakeGame(game_state, "../maps/default_map.txt", 2));

  *state = game_state;

  return 0;
}

static int teardown(void **state) {
  GameState *game_state = *state;
  FreeInnerGameState(*game_state);
  free(game_state);

  return 0;
}

/* A test case that does check if an int is equal. */
static void int_test_success(void **state) {
  GameState *game_state = *state;

  assert_int_equal(game_state->turn, 0);
}

DEFINE_TESTS(game,
             cmocka_unit_test_setup_teardown(int_test_success, setup, teardown))
