#include "bot.h"

// This does not copy the bot process! If you want to start another process for
// the bot, you must do so manually.
Bot DeepCopyBot(Bot bot) {
  Bot new_bot = {
      .name = bot.name ? DupeString(bot.name) : NULL,
      .start_command = DupeString(bot.start_command),
      .process = NULL,
  };
  return new_bot;
}

// This DOES stop and free the bot process.
void FreeInnerBot(Bot bot) {
  StopBot(bot);
  free(bot.name);
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
              bot.name ? bot.name : bot.start_command);
    }
  }
  subprocess_destroy(bot.process);
}

void StartBot(Bot bot) {
  Nob_Cmd split_command = SplitStringByDelim(bot.start_command, ' ');
  // Required by subprocess.h
  nob_cmd_append(&split_command, NULL);

  if (bot.process == NULL) {
    nob_log(NOB_ERROR, "Can't start a bot without a process struct allocated.");
    // TODO maybe rethink exit calls from this function. Since we moved to a
    // single small and self-contained function, exiting might not be right in
    // all cases, perhaps returning a bool would be better.
    exit(1);
  }
  int result = subprocess_create(split_command.items,
                                 subprocess_option_search_user_path |
                                     subprocess_option_inherit_environment |
                                     subprocess_option_enable_async |
                                     subprocess_option_enable_async_no_wait,
                                 bot.process);

  if (0 != result) {
    nob_log(NOB_ERROR, "ERROR: Failed to launch bot number %d: %s\n%s", 2,
            bot.start_command,
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
