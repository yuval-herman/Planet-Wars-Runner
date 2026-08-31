#include "clay.h"
#include "raylib.h"
#include "tinyfiledialogs.h"

#include "../configs.h"
#include "../game.h"
#include "../game_log.h"
#include "../runner.h"
#include "nob.h"

#include "components.h"
#include "menu.h"
#include "stars_shader.h"
#include "ui.h"
#include "ui_utils.h"
#include "viewer.h"

enum ButtonFunction : size_t {
  // Main menu options
  BUTTON_PLAY_MATCH,
  BUTTON_REPLAY,
};

enum SubMenu {
  MENU_MAIN,
  MENU_REPLAY,
  MENU_PLAY_MATCH,
};

static Configs *configs = NULL;
static enum SubMenu sub_menu = MENU_MAIN;

struct {
  struct {
    unsigned cursor;
    bool focused;
  } input_states[5];
  // Used to track input states
  unsigned current_input;
} play_match_data = {0};

static void MenuButtonHoverFunction(Clay_ElementId element_id,
                                    Clay_PointerData pointer_data,
                                    void *user_data) {
  NOB_UNUSED(element_id);
  if (pointer_data.state != CLAY_POINTER_DATA_RELEASED_THIS_FRAME)
    return;

  assert(configs);

  enum ButtonFunction button_function = (size_t)user_data;

  GameLog game_log = {0};

  switch (button_function) {
  default:
    NOB_UNREACHABLE("impossible button function");

  case BUTTON_REPLAY: {
    // char *result = tinyfd_openFileDialog("Select file to play", NULL, 1,
    //                                      (const char *[]){"*.plws"},
    //                                      "Planet Wars Serialization files.",
    //                                      0);
    sub_menu = MENU_REPLAY;
    EnsureNullTerminated(&configs->save_file);
    FILE *save_file = fopen(configs->save_file.items, "rb");
    if (!save_file) {
      nob_log(NOB_ERROR, "Failed opening save file \"%s\": %s.",
              configs->save_file.items, strerror(errno));
      NOB_TODO("handle errors isn't implemented...");
    } else {
      if (!ReadGameLogFromFile(save_file, &game_log)) {
        nob_log(NOB_ERROR, "Failed reading \"%s\".", configs->save_file.items);
      } else {
        SetGameLog(game_log);
        ChangeScreen(SCREEN_VIEWER);
      }
      fclose(save_file);
    }
  } break;
  }
  FreeInnerGameLog(game_log);
}

void MenuButton(Clay_String buttonText, enum ButtonFunction button_function) {
  // clang-format off
  CLAY_AUTO_ID({
    .layout = {
      .padding = {32, 32, 16, 16},
      .sizing = {.width = CLAY_SIZING_GROW(0)},
      .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
    },
    .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
    .border = {.color = CLAY_COLOR(WHITE), .width = CLAY_BORDER_OUTSIDE(2) },
    .backgroundColor = Clay_Hovered() ? CLAY_COLOR(WHITE) : (Clay_Color){0}
  }) {
    
    Clay_OnHover(MenuButtonHoverFunction, (void*)button_function);
    CLAY_TEXT(buttonText, {
      .fontId = 2,
      .fontSize = 24,
      .textColor = Clay_Hovered() ? CLAY_COLOR(BLACK) : CLAY_COLOR(WHITE),
    });
  }
  // clang-format on
}

#define InputComponent(id, label, sb)                                          \
  CLAY(CLAY_ID(id "InputContainer"),                                           \
       {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)},                   \
                   .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {                  \
    CLAY_TEXT(CLAY_STRING(label),                                              \
              {.fontSize = 16, .fontId = 1, .textColor = CLAY_COLOR(GRAY)});   \
    Component_TextEdit(                                                        \
        sb,                                                                    \
        &play_match_data.input_states[play_match_data.current_input].cursor,   \
        &play_match_data.input_states[play_match_data.current_input].focused); \
    play_match_data.current_input++;                                           \
  }

void StartMatch() {
  GameLog game_log = {0};
  GameState state = {0};
  EnsureNullTerminated(&configs->map_file);
  if (MakeGame(&state, configs->map_file.items, configs->players.count)) {
    if (!RunMatch(&game_log, &state, configs->players)) {
      nob_log(NOB_ERROR, "Failed running match.");
      NOB_TODO("handle errors isn't implemented...");
    }
    if (configs->write_save) {
      FILE *file = fopen("game.plws", "wb");
      WriteGameLogToFile(file, game_log);
      fclose(file);
    }

    SetGameLog(game_log);
    ChangeScreen(SCREEN_VIEWER);
  } else {
    NOB_TODO("handle errors isn't implemented...");
  }

  FreeInnerGameState(state);
  FreeInnerGameLog(game_log);
}

void PlayMatchView() {
  play_match_data.current_input = 0;

  // clang-format off
  CLAY(CLAY_ID("PlayMatchContainer"), {
    .layout = {
      .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
      .padding = CLAY_PADDING_ALL(32),
      .childGap = 16,
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
     },
     .border = { .color = CLAY_COLOR(GRAY), .width = CLAY_BORDER_OUTSIDE(2)},
     .cornerRadius = CLAY_CORNER_RADIUS(10),
  }) {
    CLAY_TEXT(CLAY_STRING("MATCH SETUP"), {.fontSize = 32, .fontId = 1, .textColor = CLAY_COLOR(WHITE)});
    HorizontalSeperatorComponent("HorizontalSeperator");

    CLAY(CLAY_ID("FormContainer"), {
         .layout = {
           .sizing = {CLAY_SIZING_GROW(0),CLAY_SIZING_GROW(0)},
           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
           .layoutDirection = CLAY_TOP_TO_BOTTOM,
           .childGap = 24
         }
       }) {
      InputComponent("MapPath", "MAP PATH",
                     &configs->map_file);

      CLAY(CLAY_ID("Player1InputContainer"), {
           .layout = {
             .sizing = {.width = CLAY_SIZING_GROW(0)},
             .layoutDirection = CLAY_LEFT_TO_RIGHT,
             .childGap = 16
           }
         }) {
        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0, 200)}}}) {
          InputComponent("Player1Name", "Player 1 name",
                         &configs->players.items[0].name);
        }
        InputComponent("Player1Command", "Player 1 command",
                       &configs->players.items[0].as.bot.start_command);
      }

      CLAY(CLAY_ID("Player2InputContainer"), {
           .layout = {
             .sizing = {.width = CLAY_SIZING_GROW(0)},
             .layoutDirection = CLAY_LEFT_TO_RIGHT,
             .childGap = 16
           }
         }) {
        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0, 200)}}}) {
          InputComponent("Player2Name", "Player 2 name",
                         &configs->players.items[1].name);
        }
        InputComponent("Player2Command", "Player 2 command",
                       &configs->players.items[1].as.bot.start_command);
      }
    }

    CLAY(CLAY_ID("StateButtonsContainer"), {
           .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }}
         }) {
      if (Component_Button(CLAY_STRING("Back"), false)) sub_menu = MENU_MAIN;
      SpacerComponent("Spacer");
      if (Component_Button(CLAY_STRING("Start match"), false)) {
        StartMatch();
      }
    }
  }
  // clang-format on
}

void MainMenuView() {
  // clang-format off
  CLAY(CLAY_ID("TitleContainer"), {
      .layout = {
        .layoutDirection = CLAY_TOP_TO_BOTTOM,
        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
        .childGap = 0,
      } 
     }) {
    CLAY_TEXT(CLAY_STRING("PLANET WARS"), {.fontSize = 64, .fontId = 3, .textColor = CLAY_COLOR(WHITE)});
    CLAY_TEXT(CLAY_STRING("Conquering galaxies since 1972!"), {.fontSize = 24, .fontId = 2, .textColor = CLAY_COLOR(GRAY)});
  }
  CLAY(CLAY_ID("OptionsContainer"), {
    .layout = {
      .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)},
      .padding = CLAY_PADDING_ALL(32),
      .childGap = 16,
      .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER},
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
     },
  }) {
    if(Component_Button(CLAY_STRING("PLAY MATCH"), true)) {
      sub_menu = MENU_PLAY_MATCH;
    }
    MenuButton(CLAY_STRING("REPLAY MATCH"), BUTTON_REPLAY);
  }
// clang-format on  
}

void MenuDraw() {
  StarsShaderDraw((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});
  // clang-format off
  CLAY(CLAY_ID("OuterContainer"), {
       .layout = {
         .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
         .padding = CLAY_PADDING_ALL(32),
         .childGap = 16,
         .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
       },
   }) {
    // clang-format on
    switch (sub_menu) {
    default:
      NOB_UNREACHABLE("Impossible menu sub menu");

    case MENU_MAIN:
      MainMenuView();
      break;
    case MENU_REPLAY:
      break;
    case MENU_PLAY_MATCH:
      PlayMatchView();
      break;
    }
    // clang-format off
  }
  // clang-format on
}

void MenuInit() {
  assert(configs);
  sub_menu = MENU_MAIN;
  memset(&play_match_data, 0, sizeof play_match_data);
  StarsShaderInit((StarsShaderConfig){
      .size = 0.4,
      .brightness = 0.3,
      .density = 0.5,
      .time_scale = 0.3,
      .seed = 28,
  });
  // Clay_SetDebugModeEnabled(true);
}

void MenuDestroy() {
  configs = NULL;
  StarsShaderDestroy();
}

void SetConfig(Configs *new_configs) { configs = new_configs; }

const UIScreen menu_screen = {
    .init = MenuInit,
    .draw = MenuDraw,
    .destroy = MenuDestroy,
};
