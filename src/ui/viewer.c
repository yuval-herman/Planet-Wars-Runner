#include "raylib.h"

#include "../game_log.h"

#include "clay.h"
#include "components.h"
#include "nob.h"
#include "stars_shader.h"
#include "ui_utils.h"
#include "viewer.h"

static GameSpace game_space = {0};
static unsigned int frame_counter;
// Game run speed in viewer. 0 is realtime, higher is slower.
static int game_speed;
static bool game_running;
static bool playing_forewards;

static GameLog game_log = {0};
static unsigned turn;

static void HandleScrubberHover() {
  static bool is_scrubber_held = false;

  if (Clay_Hovered() && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    is_scrubber_held = true;
    game_running = false;
  }

  if (!is_scrubber_held)
    return;

  if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
    is_scrubber_held = false;
  } else {
    Clay_ElementData e_data =
        Clay_GetElementData((Clay_ElementId){.id = Clay_GetOpenElementId()});
    if (!e_data.found)
      return;

    Clay_BoundingBox box = e_data.boundingBox;

    unsigned clmp_mouse = Clamp(GetMousePosition().x, box.x, box.x + box.width);
    turn = Remap(clmp_mouse, box.x, box.x + box.width, 0, game_log.count - 1);
  }
}

static void ScrubberTrackComponent() {
  static Nob_String_Builder scrubber_text = {0};
  // clang-format off
  CLAY(CLAY_ID("ScrubberTrack"), {
  .layout = {
    .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(12)},
    .childAlignment = {CLAY_ALIGN_X_LEFT,CLAY_ALIGN_Y_CENTER}
  },
  .backgroundColor = C_GRAY,
  .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
  }) {
    HandleScrubberHover();

    CLAY(CLAY_ID("PlayedTrack"), {
    .layout = {
      .sizing = {.width = CLAY_SIZING_PERCENT((float)turn/(game_log.count-1)), .height = CLAY_SIZING_GROW(0)},
    },
    .backgroundColor = C_LIGHTGRAY,
    .cornerRadius = {.topLeft = FLT_MAX, .bottomLeft = FLT_MAX},
    }) {
      CLAY(CLAY_ID("ScrubberThumb"), {
      .floating = {
        .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        .attachTo = CLAY_ATTACH_TO_PARENT,
        .attachPoints = {
          .parent = CLAY_ATTACH_POINT_RIGHT_CENTER,
          .element = CLAY_ATTACH_POINT_CENTER_CENTER,
        }
      },
      .layout = {
        .sizing = {.width = CLAY_SIZING_FIT(48), .height = CLAY_SIZING_FIXED(24)},
        .childAlignment = {CLAY_ALIGN_X_CENTER,CLAY_ALIGN_Y_CENTER},
      },
      .backgroundColor = C_WHITE,
      .cornerRadius = CLAY_CORNER_RADIUS(4),
      }) {
        scrubber_text.count = 0;
        nob_sb_appendf(&scrubber_text, "%u", turn);
        CLAY_TEXT(SB_TO_CLAY(scrubber_text), {.fontId = 2, .fontSize = 24, .textColor = C_BLACK});
      }
    }
  }
  // clang-format on
}

static void PlaybackControlsComponent() {
  CLAY(CLAY_ID("PlaybackControls"), {.layout = {.childGap = 8}}) {
    if (Component_Button(CLAY_STRING("|<"), BUTTON_STYLE_CONTROLLER) &&
        turn > 0) {
      playing_forewards = false;
      game_running = true;
    }
    if (Component_Button(CLAY_STRING("||"), BUTTON_STYLE_CONTROLLER)) {
      game_running = !game_running;
    }
    if (Component_Button(CLAY_STRING(">|"), BUTTON_STYLE_CONTROLLER) &&
        turn < game_log.count) {
      playing_forewards = true;
      game_running = true;
    }
  }
}

static void SpeedControlsComponent() {
  CLAY(CLAY_ID("SpeedControls"),
       {.layout = {
            .childGap = 8,
            .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
        }}) {
    CLAY_TEXT(CLAY_STRING("SPEED"),
              {.fontId = 2, .fontSize = 24, .textColor = C_LIGHTGRAY});

    CLAY(CLAY_ID("SpeedControlsButtons"),
         {
             .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
             .border = {.color = C_LIGHTGRAY, .width = CLAY_BORDER_OUTSIDE(1)},
         }) {
      if (Component_Button(CLAY_STRING("0.5x"),
                           BUTTON_STYLE_CONTROLLER_NAKED)) {
        game_speed = 10;
      }
      if (Component_Button(CLAY_STRING("1x"), BUTTON_STYLE_CONTROLLER_NAKED)) {
        game_speed = 5;
      }
      if (Component_Button(CLAY_STRING("2x"), BUTTON_STYLE_CONTROLLER_NAKED)) {
        game_speed = 2;
      }
      if (Component_Button(CLAY_STRING("5x"), BUTTON_STYLE_CONTROLLER_NAKED)) {
        game_speed = 0;
      }
    }
  }
}

static void ControlsComponent() {
  // clang-format off
  CLAY(CLAY_ID("ControlsContainer"), {
    .layout = {
      .sizing = {.width = CLAY_SIZING_GROW(0), .height=CLAY_SIZING_FIT(0)},
      .padding = {32,32,24,24},
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
      .childGap = 24,
    },
    .backgroundColor = (Clay_Color){255,255,255,15},
    .border = {.width = CLAY_BORDER_OUTSIDE(1), .color = C_WHITE},
    .cornerRadius = CLAY_CORNER_RADIUS(8)
  }) {

    ScrubberTrackComponent();

    CLAY(CLAY_ID("PlaybackControlsContainer"), {
         .layout={
           .sizing={.width=CLAY_SIZING_GROW(0)},
         }}) {
      PlaybackControlsComponent();
      SpacerComponent("ControlSpacer");
      SpeedControlsComponent();
    }
  }
  // clang-format on
}

void ViewerInit() {
  assert(game_log.count > 0);
  // Clay_SetDebugModeEnabled(true);

  turn = 0;
  frame_counter = 0;
  game_speed = 5;
  game_running = false;
  playing_forewards = true;

  game_space = ComputeGameSpace(game_log.items[0].planets,
                                game_log.items[0].planet_count);

  StarsShaderInit((StarsShaderConfig){
      .size = 0.5,
      .brightness = 0.4,
      .density = 0.5,
      .time_scale = 1,
      .seed = 1,
  });
}

void ViewerDraw() {
  frame_counter++;
  if (game_running &&
      (game_speed == 0 || frame_counter % abs(game_speed) == 0)) {
    if (playing_forewards && turn < game_log.count - 1)
      turn++;
    else if (turn > 0)
      turn--;
    if (turn >= game_log.count - 1 || turn == 0)
      game_running = false;
  }

  if (IsKeyPressed(KEY_RIGHT)) {
    game_running = false;
    turn++;
    if (turn >= game_log.count)
      turn = game_log.count - 1;
  } else if (IsKeyPressed(KEY_LEFT)) {
    game_running = false;
    if (turn != 0)
      turn--;
  } else if (IsKeyPressed(KEY_SPACE)) {
    game_running = !game_running;
  }
  if (IsKeyPressed(KEY_UP) && game_speed > 0) {
    game_speed--;
  } else if (IsKeyPressed(KEY_DOWN) && game_speed < MAX_GAME_SPEED_VALUE) {
    game_speed++;
  }

  // ============= START DRAWING =============
  ClearBackground(BLACK);
  StarsShaderDraw((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});
  // clang-format off
  CLAY(CLAY_ID("OuterContainer"), {
       .layout = {
         .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
         .padding = CLAY_PADDING_ALL(MAP_MARGIN),
         .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
         .layoutDirection = CLAY_TOP_TO_BOTTOM,
         .childGap = 32,
       },
   }) {
    Component_GameFrame(game_space, game_log.items[turn].planets,
                        game_log.items[turn].planet_count,
                        game_log.items[turn].fleets,
                        game_log.items[turn].fleet_count);
    ControlsComponent();
  }
  // clang-format on
}

void ViewerDestroy() {
  StarsShaderDestroy();
  FreeInnerGameLog(game_log);
  game_log = (GameLog){0};
}

void SetGameLog(GameLog new_game_log) {
  game_log = DeepCopyGameLog(new_game_log);
}

const UIScreen viewer_screen = {
    .init = ViewerInit,
    .draw = ViewerDraw,
    .destroy = ViewerDestroy,
};
