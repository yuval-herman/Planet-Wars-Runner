#define RAYMATH_IMPLEMENTATION
#include "raymath.h"

#include "test_game.c"

#include <cmocka.h>

#include "miniz.c"

#define NOB_IMPLEMENTATION
#include "nob.h"

int main(void) { test_game(); }
