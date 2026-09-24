#include "clay.h"
#include "raylib.h"
#include "tinyfiledialogs.h"

#include "../configs.h"
#include "../game.h"
#include "../game_log.h"
#include "../lua_scripts.h"
#include "../runner.h"
#include "nob.h"

#include "components.h"
#include "human_game.h"
#include "menu.h"
#include "stars_shader.h"
#include "ui.h"
#include "ui_utils.h"
#include "viewer.h"

enum SubMenu {
  MENU_MAIN,
  MENU_REPLAY,
  MENU_PLAY_MATCH,
  MENU_BEAT_BOTS,
};

#define STRING_AND_LENGTH(field, string)                                       \
  STRING_AND_LENGTH_INNER(field, string, _length)
#define STRING_AND_LENGTH_INNER(field, string, length_name)                    \
  .field = string, .field##length_name = NOB_ARRAY_LEN(string)

const struct {
  const Clay_String name;
  const Clay_String descriptions;
  const char *source;
  const unsigned source_length;
} embedded_bots[] = {
    {
        .name = CLAY_STRING("Demo bot"),
        .descriptions = CLAY_STRING(
            "A simple bot that can only attack one planet at a time.\n"
            "Made as an example of how to make bots."),
        .source = (const char *)DemoBot_source,
        .source_length = DemoBot_size,
    },
};

#undef STRING_AND_LENGTH

static Configs *configs = NULL;
static enum SubMenu sub_menu = MENU_MAIN;
static Nob_String_Builder error_message = {0};

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
              {.fontId = CLAY_FONT(FiraCode_Bold, 16), .textColor = C_GRAY});  \
    Component_TextEdit(                                                        \
        sb, &inputs_data.input_states[inputs_data.current_input].cursor,       \
        &inputs_data.input_states[inputs_data.current_input].focused);         \
    inputs_data.current_input++;                                               \
  }

#define PlayerInputComponent(player_number)                                    \
  CLAY(CLAY_ID("Player" #player_number "InputContainer"),                      \
       {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0)},                   \
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,                      \
                   .childGap = 16,                                             \
                   .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM}}}) {           \
    CLAY_AUTO_ID(                                                              \
        {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0, 200)}}}) {         \
      InputComponent("Player" #player_number "Name",                           \
                     "Player " #player_number " name",                         \
                     &configs->players.items[player_number - 1].name);         \
    }                                                                          \
    CLAY(                                                                      \
        CLAY_ID("Player" #player_number "TypeControls"),                       \
        {                                                                      \
            .cornerRadius = CLAY_CORNER_RADIUS_MAX(),                          \
            .border = {.color = C_LIGHTGRAY, .width = CLAY_BORDER_OUTSIDE(1)}, \
        }) {                                                                   \
      if (Component_Button(CLAY_STRING("BOT"), BUTTON_STYLE_CONTROLLER_NAKED,  \
                           configs->players.items[player_number - 1].type ==   \
                               PLAYER_BOT)) {                                  \
        configs->players.items[player_number - 1].type = PLAYER_BOT;           \
      }                                                                        \
      if (Component_Button(CLAY_STRING("HUMAN"),                               \
                           BUTTON_STYLE_CONTROLLER_NAKED,                      \
                           configs->players.items[player_number - 1].type ==   \
                               PLAYER_HUMAN)) {                                \
        configs->players.items[player_number - 1].type = PLAYER_HUMAN;         \
      }                                                                        \
      if (Component_Button(CLAY_STRING("LUA_BOT"),                             \
                           BUTTON_STYLE_CONTROLLER_NAKED,                      \
                           configs->players.items[player_number - 1].type ==   \
                               PLAYER_LUA_BOT)) {                              \
        configs->players.items[player_number - 1].type = PLAYER_LUA_BOT;       \
      }                                                                        \
    }                                                                          \
    if (configs->players.items[player_number - 1].type == PLAYER_BOT) {        \
      InputComponent(                                                          \
          "Player" #player_number "Command",                                   \
          "Player " #player_number " command",                                 \
          &configs->players.items[player_number - 1].as.bot.start_command);    \
    } else if (configs->players.items[player_number - 1].type ==               \
               PLAYER_LUA_BOT) {                                               \
      InputComponent(                                                          \
          "Player" #player_number "Lua",                                       \
          "Player " #player_number " lua script path",                         \
          &configs->players.items[player_number - 1].as.lua_bot.script_path);  \
    }                                                                          \
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

#define ShowErrorPopup(message, ...)                                           \
  do {                                                                         \
    if (error_message.count) {                                                 \
      nob_log(NOB_WARNING,                                                     \
              "Overwriting error message popup. Previous message was: %.*s",   \
              (int)error_message.count, error_message.items);                  \
      error_message.count = 0;                                                 \
    }                                                                          \
    nob_sb_appendf(&error_message, message, ##__VA_ARGS__);                    \
    nob_log(NOB_ERROR, "%.*s", (int)error_message.count, error_message.items); \
  } while (0)

void StartReplay() {
  GameLog game_log = {0};
  sub_menu = MENU_REPLAY;
  EnsureNullTerminated(&configs->save_file);
  FILE *save_file = fopen(configs->save_file.items, "rb");
  if (!save_file) {
    ShowErrorPopup("Failed opening save file \"%s\": %s.",
                   configs->save_file.items, strerror(errno));
    return;
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
    CLAY_TEXT(CLAY_STRING("REPLAY SETUP"), { .fontId = CLAY_FONT(FiraCode_Bold, 32), .textColor = C_WHITE});
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
        if (Component_Button(CLAY_STRING("Browse..."), BUTTON_STYLE_SETTINGS, false)) {
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
                   .fontId = CLAY_FONT(FiraCode_Bold, 16),
                   .textColor = (Clay_Color){160,160,160, 255}
                 });

         SpacerFixedComponent("Spacer", 16);

         CLAY_TEXT(CLAY_STRING("Players: TBD"), {
                   .fontId = CLAY_FONT(Cousine_Regular, 16),
                   .textColor = C_WHITE
                 });
         CLAY_TEXT(CLAY_STRING("Winner: TBD"), {
                   .fontId = CLAY_FONT(Cousine_Regular, 16),
                   .textColor = C_WHITE
                 });
         CLAY_TEXT(CLAY_STRING("Turn Count: TBD"), {
                   .fontId = CLAY_FONT(Cousine_Regular, 16),
                   .textColor = C_WHITE
                 });
       }
    }
    CLAY(CLAY_ID("StateButtonsContainer"), {
           .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }}
         }) {
      if (Component_Button(CLAY_STRING("Back"), BUTTON_STYLE_SUB_MENU, false)) sub_menu = MENU_MAIN;
      SpacerComponent("Spacer");
      if (Component_Button(CLAY_STRING("Launch Replay"), BUTTON_STYLE_SUB_MENU, false)) {
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
    bool has_human = false;
    nob_da_foreach(Player, player, &configs->players) {
      if (player->type == PLAYER_HUMAN) {
        has_human = true;
        break;
      }
    }
    if (has_human) {
      SetGameState(state);
      SetPlayers(configs->players);
      ChangeScreen(SCREEN_HUMAN_GAME);
    } else {
      if (RunMatch(&game_log, &state, configs->players)) {
        if (configs->write_save) {
          FILE *file = fopen("game.plws", "wb");
          WriteGameLogToFile(file, game_log);
          fclose(file);
        }

        SetGameLog(game_log);
        ChangeScreen(SCREEN_VIEWER);
      } else {
        // TODO We need a way to know why the match failed and report a more
        // meaningful error.
        ShowErrorPopup("Failed running match.");
      }
    }
  } else {
    // TODO We need a way to know why the match failed and report a more
    // meaningful error.
    ShowErrorPopup("Failed starting the game.");
  }

  FreeInnerGameState(state);
  FreeInnerGameLog(game_log);
}

void PlayMatchView() {
  inputs_data.current_input = 0;

  // clang-format off
  SubMenuContainer("PlayMatchContainer") {
    CLAY_TEXT(CLAY_STRING("MATCH SETUP"), { .fontId = CLAY_FONT(FiraCode_Bold, 32), .textColor = C_WHITE});
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
        if (Component_Button(CLAY_STRING("Browse..."), BUTTON_STYLE_SETTINGS, false)) {
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

      PlayerInputComponent(1);
      PlayerInputComponent(2);
    }

    CLAY(CLAY_ID("StateButtonsContainer"), {
           .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }}
         }) {
      if (Component_Button(CLAY_STRING("Back"), BUTTON_STYLE_SUB_MENU, false)) sub_menu = MENU_MAIN;
      SpacerComponent("Spacer");
      if (Component_Button(CLAY_STRING("Start match"), BUTTON_STYLE_SUB_MENU, false)) {
        StartMatch();
      }
    }
  }
  // clang-format on
}

void StartBeatBotsMatch() {
  GameLog game_log = {0};
  GameState state = {0};
  configs->players.items[0].type = PLAYER_HUMAN;
  EnsureNullTerminated(&configs->map_file);
  if (MakeGame(&state, configs->map_file.items, configs->players.count)) {
    SetGameState(state);
    SetPlayers(configs->players);
    ChangeScreen(SCREEN_HUMAN_GAME);
  } else {
    // TODO We need a way to know why the match failed and report a more
    // meaningful error.
    ShowErrorPopup("Failed starting the game.");
  }

  FreeInnerGameState(state);
  FreeInnerGameLog(game_log);
}

void PlayBeatBotsView() {
  inputs_data.current_input = 0;
  static unsigned selected_bot = 0;

  // clang-format off
  SubMenuContainer("PlayMatchContainer") {
    CLAY_TEXT(CLAY_STRING("BEAT BOTS"), { .fontId = CLAY_FONT(FiraCode_Bold, 32), .textColor = C_WHITE});
    HorizontalSeperatorComponent("HorizontalSeperator");

    CLAY(CLAY_ID("FormContainer"), {
         .layout = {
           .sizing = {CLAY_SIZING_GROW(0),CLAY_SIZING_GROW(0)},
           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
           .layoutDirection = CLAY_TOP_TO_BOTTOM,
           .childGap = 24
         }
       }) {
      InputComponent("PlayerName", "PLAYER NAME", &configs->players.items[0].name);

      CLAY_TEXT(CLAY_STRING("SELECT OPPONENT"), {.fontId = CLAY_FONT(FiraCode_Bold, 16),
                                       .textColor = C_GRAY});
      for (unsigned i = 0; i < NOB_ARRAY_LEN(embedded_bots); i++) {
        CLAY(CLAY_IDI("BotContainer", i), {
             .layout = {
               .sizing = {
                 .width = CLAY_SIZING_GROW(0)
               },
               .childGap = 16,
               .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
               .padding = {16,16,8,8},
             },
             .cornerRadius = CLAY_CORNER_RADIUS(8),
             .backgroundColor = (Clay_Color){255,255,255,20},
             .border = {
               .color = (Clay_Color){255,255,255,i==selected_bot ? 220 : 30},
               .width = CLAY_BORDER_OUTSIDE(i==selected_bot ? 2 : 1),
             }
           }) {
          // clang-format on
          if (Clay_Hovered() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selected_bot = i;
          }
          // clang-format off
          CLAY(CLAY_IDI("radio", i), {
               .layout = {
                 .sizing = {.height = CLAY_SIZING_PERCENT(0.5)}
               },
               .aspectRatio = { 1 },
               .cornerRadius = CLAY_CORNER_RADIUS_MAX(),
               .border = {
                 .color = C_WHITE,
                 .width = CLAY_BORDER_OUTSIDE(i==selected_bot ? 5 : 1),
               }
             });
          CLAY(CLAY_IDI("BotInfoCOntainer", i), {
               .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM }
             }) {
            CLAY_TEXT(embedded_bots[i].name, {
                      .fontId = CLAY_FONT(FiraCode_Bold, 24),
                      .textColor = C_WHITE
                    });
            CLAY_TEXT(embedded_bots[i].descriptions, {
                      .fontId = CLAY_FONT(FiraCode_Regular, 16),
                      .textColor = C_GRAY
                    });
          }
        }
      }
    }

    CLAY(CLAY_ID("StateButtonsContainer"), {
           .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) }}
         }) {
      if (Component_Button(CLAY_STRING("Back"), BUTTON_STYLE_SUB_MENU, false)) sub_menu = MENU_MAIN;
      SpacerComponent("Spacer");
      // clang-format on
      if (Component_Button(CLAY_STRING("Start match"), BUTTON_STYLE_SUB_MENU,
                           false)) {
        configs->players.items[1].type = PLAYER_LUA_BOT;
        configs->players.items[1].name.count = 0;
        nob_sb_append_buf(&configs->players.items[1].name,
                          embedded_bots[selected_bot].name.chars,
                          embedded_bots[selected_bot].name.length);
        configs->players.items[1].as.lua_bot = (LuaBot){0};
        nob_sb_append_buf(&configs->players.items[1].as.lua_bot.script_code,
                          embedded_bots[selected_bot].source,
                          embedded_bots[selected_bot].source_length);
        StartBeatBotsMatch();
      }
      // clang-format off
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
    CLAY_TEXT(CLAY_STRING("PLANET WARS"), { .fontId = CLAY_FONT(Cousine_Regular, 64), .textColor = C_WHITE});
    CLAY_TEXT(CLAY_STRING("Conquering galaxies since 1972!"), { .fontId = CLAY_FONT(FiraCode_Regular, 24), .textColor = C_GRAY});
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
    if(Component_Button(CLAY_STRING("BEAT BOTS"), BUTTON_STYLE_MENU, false)) {
      sub_menu = MENU_BEAT_BOTS;
    }
    if(Component_Button(CLAY_STRING("PLAY MATCH"), BUTTON_STYLE_MENU, false)) {
      sub_menu = MENU_PLAY_MATCH;
    }
    if(Component_Button(CLAY_STRING("REPLAY MATCH"), BUTTON_STYLE_MENU, false)) {
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
    if (error_message.count != 0)
      if (Component_MessageBox(SB_TO_CLAY(error_message))) {
        error_message.count = 0;
      }
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
      for (unsigned id = 0; configs->players.count < 2; id++) {
        const Player empty_player = {.type = PLAYER_BOT, .id = id};
        nob_da_append(&configs->players, empty_player);
      }
      PlayMatchView();
      break;
    case MENU_BEAT_BOTS:
      // initialize if empty
      for (unsigned id = 0; configs->players.count < 2; id++) {
        const Player empty_player = {.type = PLAYER_BOT, .id = id};
        nob_da_append(&configs->players, empty_player);
      }
      PlayBeatBotsView();
      break;
    }
    // clang-format off
  }
  // clang-format on
}

void MenuInit() {
  assert(configs);
  sub_menu = MENU_MAIN;
  error_message.count = 0;
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
  nob_sb_free(error_message);
}

void SetConfig(Configs *new_configs) { configs = new_configs; }

const UIScreen menu_screen = {
    .init = MenuInit,
    .draw = MenuDraw,
    .destroy = MenuDestroy,
};
