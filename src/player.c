#include "player.h"

Player DeepCopyPlayer(Player player) {
  Player new_player = {
      .type = player.type,
      .name = DupeString(player.name),
  };

  switch (player.type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    new_player.as.bot = DeepCopyBot(player.as.bot);
    break;
  case PLAYER_HUMAN:
    // currently empty
    break;
  case PLAYER_REPLAY:
    // has no special data
    break;
  }
  return new_player;
}

void FreeInnerPlayer(Player player) {
  free(player.name);

  switch (player.type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    FreeInnerBot(player.as.bot);
    break;
  case PLAYER_HUMAN:
    // currently empty
    break;
  case PLAYER_REPLAY:
    // has no special data
    break;
  }
}

PlayerDA DeepCopyPlayerDA(PlayerDA players) {
  PlayerDA new_players = {
      .items = malloc(sizeof *players.items * players.count),
      .count = players.count,
      .capacity = players.count,
  };
  for (unsigned i = 0; i < players.count; i++) {
    new_players.items[i] = DeepCopyPlayer(players.items[i]);
  }
  return new_players;
}

void FreeInnerPlayerDA(PlayerDA players) {
  nob_da_foreach(Player, player, &players) { FreeInnerPlayer(*player); }
  nob_da_free(players);
}

bool StartPlayer(Player *player) {
  if (IsPlayerActive(*player))
    return true;

  switch (player->type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    return StartBot(&player->as.bot);
  case PLAYER_HUMAN:
    // currently empty
    return true;
  case PLAYER_REPLAY:
    // has no special data
    return true;
  }
}

bool StopPlayer(Player *player) {
  if (!IsPlayerActive(*player))
    return true;

  switch (player->type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    return StopBot(player->as.bot);
  case PLAYER_HUMAN:
    // currently empty
    return true;
  case PLAYER_REPLAY:
    // has no special data
    return true;
  }
}

bool IsPlayerActive(Player player) {
  switch (player.type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    return IsBotAlive(player.as.bot);
    break;
  case PLAYER_HUMAN:
    return true;
    break;
  case PLAYER_REPLAY:
    return true;
    break;
  }
}

bool SendMessageToPlayer(Player player, char *message, unsigned length) {
  switch (player.type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    return SendMessageToBot(player.as.bot, message, length);
    break;
  case PLAYER_HUMAN:
    return true;
    break;
  case PLAYER_REPLAY:
    return true;
    break;
  }
}

bool GetPlayerMessage(Player player, Nob_String_Builder *sb) {
  switch (player.type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    return GetBotMessage(player.as.bot, sb);
    break;
  case PLAYER_HUMAN:
    sb->count = 0;
    return true;
    break;
  case PLAYER_REPLAY:
    sb->count = 0;
    return true;
    break;
  }
}

void GetPlayerDebugMessage(Player player, Nob_String_Builder *sb) {
  switch (player.type) {
  default:
    NOB_UNREACHABLE("Impossible player type");
  case PLAYER_BOT:
    GetBotDebugMessage(player.as.bot, sb);
    break;
  case PLAYER_HUMAN:
    sb->count = 0;
    break;
  case PLAYER_REPLAY:
    sb->count = 0;
    break;
  }
}
