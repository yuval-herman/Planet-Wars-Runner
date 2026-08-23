#ifndef UI_SCREEN_H
#define UI_SCREEN_H

// We don't expect screens to be instantiated more then once, therfore they are
// all expected to have inner state and manage their own resources or expose
// functions that do that.
typedef struct {
  void (*init)();
  void (*draw)();
  void (*destroy)();
} UIScreen;

#endif // UI_SCREEN_H
