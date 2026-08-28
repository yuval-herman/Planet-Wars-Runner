#ifndef BOT_H
#define BOT_H
#include "subprocess.h"
#include "utils.h"

// Time in nanoseconds, currently set to 100ms
#define MAX_BOT_RESPONSE_TIME (1000 * 1000 * 100)
// How much time to sleep between checks on bots responses in nanoseconds,
// currently set to 100 microseconds
#define WAIT_SLEEP_TIME (1000 * 100)
// A string used to denote the end of a message by the bots and engine
#define MESSAGE_DELIMETER "go" NOB_LINE_END

DefineComplexStruct(Bot, {
  Nob_String_Builder start_command;
  struct subprocess_s *process;
});

DefineComplexStruct(BotsDA, {
  Bot *items;
  unsigned count;
  unsigned capacity;
});

bool StopBot(Bot bot);
bool StartBot(Bot *bot);
bool IsBotAlive(Bot bot);
// Return true if everythin went okay. Return false in case bot should be
// disqualified.
bool GetBotMessage(Bot bot, Nob_String_Builder *sb);
// Write into `sb` from the stderr stream of the bot.
// The original protocol does not specify how a debug message should end, as
// such, we have no way of knowing for sure when a bot message is finished, or
// if a bot is even intending to send a message. We therefore rely on the fact
// bots usually send debug message while processing their next response, even
// though they can technically send them whenever they wish.
void GetBotDebugMessage(Bot bot, Nob_String_Builder *sb);
// Send a message to the bot, return true on success, false otherwise.
bool SendMessageToBot(Bot bot, char *message, unsigned length);
#endif // BOT_H
