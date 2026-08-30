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
#include "ui.h"
#include "ui_utils.h"
#include "viewer.h"

enum ButtonFunction : size_t {
  // Main menu options
  BUTTON_PLAY_MATCH,
  BUTTON_REPLAY,

  // Play match buttons
  BUTTON_MAP,
  BUTTON_P1_CMD,
  BUTTON_P1_NAME,
  BUTTON_P2_CMD,
  BUTTON_P2_NAME,
  BUTTON_START_MATCH,
};

enum SubMenu {
  MENU_MAIN,
  MENU_REPLAY,
  MENU_PLAY_MATCH,
};

static Configs *configs = NULL;
static enum SubMenu sub_menu = MENU_MAIN;

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
  case BUTTON_MAP: {
    EnsureNullTerminated(&configs->map_file);
    char *result = tinyfd_inputBox(
        "Map file", "Select the map file:", configs->map_file.items);
    if (result) {
      configs->map_file.count = 0;
      nob_sb_append_cstr(&configs->map_file, result);
    }
  } break;
  case BUTTON_P1_CMD: {
    char *result =
        tinyfd_inputBox("Player 1 command", "Select the command for player 1:",
                        configs->players.items[0].as.bot.start_command.items);
    if (result) {
      configs->players.items[0].as.bot.start_command.count = 0;
      nob_sb_append_cstr(&configs->players.items[0].as.bot.start_command,
                         result);
    }
  } break;
  case BUTTON_P2_CMD: {
    char *result =
        tinyfd_inputBox("Player 2 command", "Select the command for player 2:",
                        configs->players.items[1].as.bot.start_command.items);
    if (result) {
      configs->players.items[1].as.bot.start_command.count = 0;
      nob_sb_append_cstr(&configs->players.items[1].as.bot.start_command,
                         result);
    }
  } break;
  case BUTTON_P1_NAME: {
    char *result = tinyfd_inputBox(
        "Player 1 name",
        "Select the name for player 1:", configs->players.items[0].name.items);
    if (result) {
      configs->players.items[0].name.count = 0;
      nob_sb_append_cstr(&configs->players.items[0].name, result);
    }
  } break;
  case BUTTON_P2_NAME: {
    char *result = tinyfd_inputBox(
        "Player 2 name",
        "Select the name for player 2:", configs->players.items[1].name.items);
    if (result) {
      configs->players.items[1].name.count = 0;
      nob_sb_append_cstr(&configs->players.items[1].name, result);
    }
  } break;
  case BUTTON_START_MATCH: {
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

void PlayMatchView() {

  // static unsigned map_cursor = 0;
  // static bool map_focused = false;
  //
  // Clay_String p1_cmd =
  //     SB_TO_CLAY(configs->players.items[0].as.bot.start_command);
  // Clay_String p2_cmd =
  //     SB_TO_CLAY(configs->players.items[1].as.bot.start_command);
  // Clay_String p1_name = SB_TO_CLAY(configs->players.items[0].name);
  // Clay_String p2_name = SB_TO_CLAY(configs->players.items[1].name);

  // clang-format off
  CLAY(CLAY_ID("PlayMatchContainer"), {
    .layout = {
      .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_GROW(0)},
      .padding = CLAY_PADDING_ALL(32),
      .childGap = 16,
      .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_BOTTOM},
      .layoutDirection = CLAY_TOP_TO_BOTTOM,
     },
  }) {

    // Component_TextEdit(&configs->map_file, &map_cursor, &map_focused);
    // MenuButton(p1_cmd, BUTTON_P1_CMD);
    // MenuButton(p2_cmd, BUTTON_P2_CMD);
    // MenuButton(p1_name, BUTTON_P1_NAME);
    // MenuButton(p2_name, BUTTON_P2_NAME);
    // MenuButton(CLAY_STRING("Start match"), BUTTON_START_MATCH);

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
    if(Component_Button(CLAY_STRING("PLAY MATCH"))) {
      sub_menu = MENU_PLAY_MATCH;
    }
    MenuButton(CLAY_STRING("REPLAY MATCH"), BUTTON_REPLAY);
  }
// clang-format on  
}

void MenuDraw() {
  // clang-format off
  CLAY(CLAY_ID("OuterContainer"), {
       .layout = {
         .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
         .padding = CLAY_PADDING_ALL(32),
         .childGap = 16,
         .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
       },
       .backgroundColor = CLAY_COLOR(BLACK)
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
  sub_menu = MENU_PLAY_MATCH;
  // Clay_SetDebugModeEnabled(true);
}

void MenuDestroy() { configs = NULL; }

void SetConfig(Configs *new_configs) { configs = new_configs; }

const UIScreen menu_screen = {
    .init = MenuInit,
    .draw = MenuDraw,
    .destroy = MenuDestroy,
};
