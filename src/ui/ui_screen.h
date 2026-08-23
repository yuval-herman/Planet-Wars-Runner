#ifndef UI_SCREEN_H
#define UI_SCREEN_H

// We don't expect screens to be instantiated more then once, therfore they are
// all expected to have inner state.
typedef struct {
  // TODO: Make the params typesafe. Or check it on runtime.
  void (*init)(void *params);
  void (*draw)();
  void (*destroy)();
} UIScreen;

#endif // UI_SCREEN_H
