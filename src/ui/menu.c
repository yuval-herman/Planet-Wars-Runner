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
} inputs_data = {0};

#define InputComponent(id, label, sb)                                          \
  CLAY(CLAY_ID(id "InputContainer"),                                           \
       {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)},                   \
                   .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {                  \
    CLAY_TEXT(CLAY_STRING(label),                                              \
              {.fontSize = 16, .fontId = 1, .textColor = C_GRAY});             \
    Component_TextEdit(                                                        \
        sb, &inputs_data.input_states[inputs_data.current_input].cursor,       \
        &inputs_data.input_states[inputs_data.current_input].focused);         \
    inputs_data.current_input++;                                               \
  }

#define SubMenuContainer(id)                                                   \
  CLAY(CLAY_ID(id),                                                            \
       {                                                                       \
           .layout =                                                           \
               {                                                               \
                   .sizing = {.width = CLAY_SIZING_GROW(0),                    \
                              .height = CLAY_SIZING_GROW(0)},                  \
                   .padding = CLAY_PADDING_ALL(32),                            \
                   .childGap = 16,                                             \
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,                      \
               },                                                              \
           .border = {.color = C_GRAY, .width = CLAY_BORDER_OUTSIDE(2)},       \
           .cornerRadius = CLAY_CORNER_RADIUS(10),                             \
       })

void StartReplay() {
  GameLog game_log = {0};
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
  FreeInnerGameLog(game_log);
}

void ReplayView() {
  inputs_data.current_input = 0;
  // clang-format off
  SubMenuContainer("ReplayContainer") {
    CLAY_TEXT(CLAY_STRING("REPLAY SETUP"), {.fontSize = 32, .fontId = 1, .textColor = C_WHITE});
    HorizontalSeperatorComponent("HorizontalSeperator");

    CLAY(CLAY_ID("FormContainer"), {
         .layout = {
           .sizing = {CLAY_SIZING_GROW(0),CLAY_SIZING_GROW(0)},
           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
           .layoutDirection = CLAY_TOP_TO_BOTTOM,
           .childGap = 24
         }
       }) {

      CLAY(CLAY_ID("ReplayFileInputContainer"), {
           .layout = {
             .sizing = {.width = CLAY_SIZING_GROW(0)},
             .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM},
             .layoutDirection = CLAY_LEFT_TO_RIGHT,
             .childGap = 16
           }
         }) {
        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)}}}) {
          InputComponent("ReplayFilePath", "REPLAY FILE (.PLWS)",
                         &configs->save_file);
        }
        if (Component_Button(CLAY_STRING("Browse..."), BUTTON_STYLE_SETTINGS)) {
          char *result = tinyfd_openFileDialog("Select file to replay", NULL, 1,
                                               (const char *[]){"*.plws"},
                                               "Planet Wars Serialization files.",
                                               0);
          if (result) {
            configs->save_file.count = 0;
            nob_sb_append_cstr(&configs->save_file, result);
          }
        }
      }

      CLAY(CLAY_ID("ReplayInfoContainer"), {
             .layout = {
               .padding = CLAY_PADDING_ALL(16),
               .sizing = {CLAY_SIZING_GROW(0),CLAY_SIZING_FIT(0)},
               .layoutDirection = CLAY_TOP_TO_BOTTOM,
             },
             .backgroundColor = (Clay_Color){90,90,90, 40},
             .cornerRadius = CLAY_CORNER_RADIUS(5),
           }) {
         CLAY_TEXT(CLAY_STRING("REPLAY INFO"), {
                   .fontId = 2,
                   .fontSize = 24,
                   .textColor = (Clay_Color){160,160,160, 255}
                 });

         SpacerFixedComponent("Spacer", 16);

         CLAY_TEXT(CLAY_STRING("Players: TBD"), {
                   .fontId = 2,
                   .fontSize = 24,
                   .textColor = C_WHITE
                 });
         CLAY_TEXT(CLAY_STRING("Winner: TBD"), {
                   .fontId = 2,
                   .fontSize = 24,
                   .textColor = C_WHITE
                 });
         CLAY_TEXT(CLAY_STRING("Turn Count: TBD"), {
                   .fontId = 2,
                   .fontSize = 24,
                   .textColor = C_WHITE
                 });
       }
    }
    CLAY(CLAY_ID("StateButtonsContainer"), {
           .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }}
         }) {
      if (Component_Button(CLAY_STRING("Back"), BUTTON_STYLE_SUB_MENU)) sub_menu = MENU_MAIN;
      SpacerComponent("Spacer");
      if (Component_Button(CLAY_STRING("Launch Replay"), BUTTON_STYLE_SUB_MENU)) {
        StartReplay();
      }
    }
  }
  // clang-format on
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
  inputs_data.current_input = 0;

  // clang-format off
  SubMenuContainer("PlayMatchContainer") {
    CLAY_TEXT(CLAY_STRING("MATCH SETUP"), {.fontSize = 32, .fontId = 1, .textColor = C_WHITE});
    HorizontalSeperatorComponent("HorizontalSeperator");

    CLAY(CLAY_ID("FormContainer"), {
         .layout = {
           .sizing = {CLAY_SIZING_GROW(0),CLAY_SIZING_GROW(0)},
           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
           .layoutDirection = CLAY_TOP_TO_BOTTOM,
           .childGap = 24
         }
       }) {
      CLAY(CLAY_ID("ReplayFileInputContainer"), {
           .layout = {
             .sizing = {.width = CLAY_SIZING_GROW(0)},
             .layoutDirection = CLAY_LEFT_TO_RIGHT,
             .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM},
             .childGap = 16
           }
         }) {
        CLAY_AUTO_ID({.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)}}}) {
          InputComponent("MapPath", "MAP PATH",
                         &configs->map_file);
        }
        if (Component_Button(CLAY_STRING("Browse..."), BUTTON_STYLE_SETTINGS)) {
          char *result = tinyfd_openFileDialog("Select map file", NULL, 1,
                                               (const char *[]){"*.txt"},
                                               "Map text files.",
                                               0);
          if (result) {
            configs->map_file.count = 0;
            nob_sb_append_cstr(&configs->map_file, result);
          }
        }
      }

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
      if (Component_Button(CLAY_STRING("Back"), BUTTON_STYLE_SUB_MENU)) sub_menu = MENU_MAIN;
      SpacerComponent("Spacer");
      if (Component_Button(CLAY_STRING("Start match"), BUTTON_STYLE_SUB_MENU)) {
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
    CLAY_TEXT(CLAY_STRING("PLANET WARS"), {.fontSize = 64, .fontId = 3, .textColor = C_WHITE});
    CLAY_TEXT(CLAY_STRING("Conquering galaxies since 1972!"), {.fontSize = 24, .fontId = 2, .textColor = C_GRAY});
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
    if(Component_Button(CLAY_STRING("PLAY MATCH"), BUTTON_STYLE_MENU)) {
      sub_menu = MENU_PLAY_MATCH;
    }
    if(Component_Button(CLAY_STRING("REPLAY MATCH"), BUTTON_STYLE_MENU)) {
      sub_menu = MENU_REPLAY;
    }
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
      ReplayView();
      break;
    case MENU_PLAY_MATCH:
      // initialize if empty
      while (configs->players.count < 2) {
        const Player empty_player = {.type = PLAYER_BOT};
        nob_da_append(&configs->players, empty_player);
      }
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
  memset(&inputs_data, 0, sizeof inputs_data);
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
