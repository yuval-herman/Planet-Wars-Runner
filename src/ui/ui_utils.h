#ifndef UI_UTILS_H
#define UI_UTILS_H

#include "clay.h"

typedef enum { CUSTOM_ELEMENT_TYPE_FUNCTION } CustomElementType;

typedef void (*CustomElementFunction)(Clay_BoundingBox bounding_box);

typedef struct {
  CustomElementType type;
  union {
      CustomElementFunction function;
  } as;
} CustomElementData;

#define SB_TO_CLAY(sb)                                                         \
  (Clay_String) {                                                              \
    .isStaticallyAllocated = false, .length = sb.count, .chars = sb.items      \
  }

// Raylib colors wrapped in clay color structs
// clang-format off
#define C_LIGHTGRAY  (Clay_Color){ 200, 200, 200, 255 }   // Light Gray
#define C_GRAY       (Clay_Color){ 130, 130, 130, 255 }   // Gray
#define C_DARKGRAY   (Clay_Color){ 80, 80, 80, 255 }      // Dark Gray
#define C_YELLOW     (Clay_Color){ 253, 249, 0, 255 }     // Yellow
#define C_GOLD       (Clay_Color){ 255, 203, 0, 255 }     // Gold
#define C_ORANGE     (Clay_Color){ 255, 161, 0, 255 }     // Orange
#define C_PINK       (Clay_Color){ 255, 109, 194, 255 }   // Pink
#define C_RED        (Clay_Color){ 230, 41, 55, 255 }     // Red
#define C_MAROON     (Clay_Color){ 190, 33, 55, 255 }     // Maroon
#define C_GREEN      (Clay_Color){ 0, 228, 48, 255 }      // Green
#define C_LIME       (Clay_Color){ 0, 158, 47, 255 }      // Lime
#define C_DARKGREEN  (Clay_Color){ 0, 117, 44, 255 }      // Dark Green
#define C_SKYBLUE    (Clay_Color){ 102, 191, 255, 255 }   // Sky Blue
#define C_BLUE       (Clay_Color){ 0, 121, 241, 255 }     // Blue
#define C_DARKBLUE   (Clay_Color){ 0, 82, 172, 255 }      // Dark Blue
#define C_PURPLE     (Clay_Color){ 200, 122, 255, 255 }   // Purple
#define C_VIOLET     (Clay_Color){ 135, 60, 190, 255 }    // Violet
#define C_DARKPURPLE (Clay_Color){ 112, 31, 126, 255 }    // Dark Purple
#define C_BEIGE      (Clay_Color){ 211, 176, 131, 255 }   // Beige
#define C_BROWN      (Clay_Color){ 127, 106, 79, 255 }    // Brown
#define C_DARKBROWN  (Clay_Color){ 76, 63, 47, 255 }      // Dark Brown

#define C_WHITE      (Clay_Color){ 255, 255, 255, 255 }   // White
#define C_BLACK      (Clay_Color){ 0, 0, 0, 255 }         // Black
#define C_BLANK      (Clay_Color){ 0, 0, 0, 0 }           // Blank (Transparent)
#define C_MAGENTA    (Clay_Color){ 255, 0, 255, 255 }     // Magenta
#define C_RAYWHITE   (Clay_Color){ 245, 245, 245, 255 }   // My own White (raylib logo)
// clang-format on
#endif // UI_UTILS_H
