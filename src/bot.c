#include "bot.h"

// This does not copy the bot process! If you want to start another process for
// the bot, you must do so manually.
Bot DeepCopyBot(Bot bot) {
  Bot new_bot = {
      .start_command = DupeString(bot.start_command),
      .process = NULL,
  };
  return new_bot;
}

// This DOES stop and free the bot process.
void FreeInnerBot(Bot bot) {
  StopBot(bot);
  free(bot.start_command);
  free(bot.process);
}

BotsDA DeepCopyBotsDA(BotsDA bots) {
  BotsDA new_bots = {
      .items = malloc(sizeof *bots.items * bots.count),
      .count = bots.count,
      .capacity = bots.count,
  };
  for (unsigned i = 0; i < bots.count; i++) {
    new_bots.items[i] = DeepCopyBot(bots.items[i]);
  }
  return new_bots;
}

void FreeInnerBotsDA(BotsDA bots) {
  nob_da_foreach(Bot, bot, &bots) {
    FreeInnerBot(*bot);
    bot->process = NULL;
  }
  nob_da_free(bots);
}

void StopBot(Bot bot) {
  if (bot.process == NULL)
    return;
  if (subprocess_alive(bot.process)) {
    if (subprocess_terminate(bot.process) != 0 ||
        subprocess_join(bot.process, NULL) != 0) {
      nob_log(NOB_WARNING, "Failed terminating bot process: %s.",
              bot.start_command);
    }
  }
  subprocess_destroy(bot.process);
}

bool IsBotAlive(Bot bot) {
  return bot.process != NULL && subprocess_alive(bot.process);
}

void StartBot(Bot *bot) {
  Nob_Cmd split_command = SplitStringByDelim(bot->start_command, ' ');
  // Required by subprocess.h
  nob_cmd_append(&split_command, NULL);

  if (bot->process == NULL) {
    bot->process = malloc(sizeof *bot->process);
  }
  int result = subprocess_create(split_command.items,
                                 subprocess_option_search_user_path |
                                     subprocess_option_inherit_environment |
                                     subprocess_option_enable_async |
                                     subprocess_option_enable_async_no_wait,
                                 bot->process);

  if (0 != result) {
    nob_log(NOB_ERROR, "ERROR: Failed to launch bot number %d: %s\n%s", 2,
            bot->start_command,
#ifdef _WIN32
            nob_win32_error_message(GetLastError())
#else
            strerror(errno)
#endif
    );
    // See comment on previous exit
    exit(1);
  }

  FreeMultiDString((char **)split_command.items, split_command.count);
}

bool GetBotMessage(Bot bot, Nob_String_Builder *sb) {
  if (!subprocess_alive(bot.process)) {
    nob_log(NOB_INFO,
            "Bot %s should be disqualified since it's process crashed.",
            bot.start_command);
    sb->count = 0;
    return false;
  }

  const unsigned max_chunk_length = 512;
  bool message_ended = false;

  uint64_t start = nob_nanos_since_unspecified_epoch();
  while (!message_ended) {
    nob_da_reserve(sb, sb->count + max_chunk_length);
    // Remove null terminator if it exists
    if (sb->count > 0 && nob_da_last(sb) == '\0') {
      nob_log(NOB_DEBUG, "removed null terminator");
      sb->count--;
    }
    unsigned int received = subprocess_read_stdout(
        bot.process, sb->items + sb->count, sb->capacity - sb->count);
    if (received == 0) {
      if (nob_nanos_since_unspecified_epoch() - start > MAX_BOT_RESPONSE_TIME) {
        nob_log(NOB_INFO,
                "Bot %s should be disqualified for taking too long to reply.",
                bot.start_command);
        sb->count = 0;
        return false;
      }
      sleep_ns(WAIT_SLEEP_TIME);
      continue;
    }
    sb->count += received;

    nob_log(NOB_DEBUG, "bot %s sent: |%.*s|", bot.start_command,
            (unsigned)sb->count, sb->items);

    // Excluding null terminator
    const unsigned delimeter_length = NOB_ARRAY_LEN(MESSAGE_DELIMETER) - 1;
    // We need to check sb.count is at least `delimeter_length` to make sure
    // memcmp does not access OOB memory
    if (sb->count >= delimeter_length &&
        memcmp(sb->items + sb->count - delimeter_length, MESSAGE_DELIMETER,
               delimeter_length) == 0) {
      message_ended = true;
      nob_log(NOB_DEBUG, "bot %s message ended", bot.start_command);
    }
  }

  return true;
}

void GetBotDebugMessage(Bot bot, Nob_String_Builder *sb) {
  const unsigned max_chunk_length = 512;
  sb->count = 0;
  bool message_ended = false;

  while (!message_ended) {
    nob_da_reserve(sb, sb->count + max_chunk_length);
    // Remove null terminator if it exists
    if (sb->count > 0 && nob_da_last(sb) == '\0') {
      nob_log(NOB_DEBUG, "removed null terminator");
      sb->count--;
    }
    unsigned int received = subprocess_read_stderr(
        bot.process, sb->items + sb->count, sb->capacity - sb->count);
    if (received == 0) {
      message_ended = true;
    }
    sb->count += received;
  }
}

bool SendMessageToBot(Bot bot, char *message, unsigned length) {
  if (!subprocess_alive(bot.process)) {
    nob_log(NOB_INFO, "Bot %s has crashed.", bot.start_command);
    return false;
  }
  FILE *bot_stdin = subprocess_stdin(bot.process);
  fwrite(message, sizeof *message, length, bot_stdin);
  fflush(bot_stdin);
  return true;
}
