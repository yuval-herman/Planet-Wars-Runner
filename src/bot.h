#ifndef BOT_H
#define BOT_H
#include "utils.h"
#include "subprocess.h"

// Time in nanoseconds, currently set to 100ms
#define MAX_BOT_RESPONSE_TIME (1000 * 1000 * 100)
// How much time to sleep between checks on bots responses in nanoseconds,
// currently set to 100 microseconds
#define WAIT_SLEEP_TIME (1000 * 100)
// A string used to denote the end of a message by the bots and engine
#define MESSAGE_DELIMETER "go" NOB_LINE_END

DefineComplexStruct(Bot, {
  char *name;
  char *start_command;
  struct subprocess_s *process;
});

DefineComplexStruct(BotsDA, {
  Bot *items;
  unsigned count;
  unsigned capacity;
});

void StopBot(Bot bot);
void StartBot(Bot bot);
// Return true if everythin went okay. Return false in case bot should be
// disqualified.
bool GetBotMessage(Bot bot, Nob_String_Builder *sb);

#endif // BOT_H

