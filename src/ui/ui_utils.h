#ifndef UI_UTILS_H
#define UI_UTILS_H

// Convenient shorthand for Raylib -> Clay color conversion.
#define CLAY_COLOR(color)                                                      \
  (Clay_Color) { .r = color.r, .g = color.g, .b = color.b, .a = color.a }

#define SB_TO_CLAY(sb)                                                         \
  (Clay_String) {                                                              \
    .isStaticallyAllocated = false, .length = sb.count, .chars = sb.items      \
  }

#endif // UI_UTILS_H
