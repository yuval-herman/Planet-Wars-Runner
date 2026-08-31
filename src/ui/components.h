#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "components/button.c"
#include "components/text_edit.c"

#define SpacerComponent(id)                                                    \
  CLAY(CLAY_ID_LOCAL(id),                                                      \
       {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0),                    \
                              .height = CLAY_SIZING_GROW(0)}}}) {}

#define SpacerFixedComponent(id, size)                                         \
  CLAY(CLAY_ID_LOCAL(id),                                                      \
       {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(size),                \
                              .height = CLAY_SIZING_FIXED(size)}}}) {}

#define HorizontalSeperatorComponent(id)                                       \
  CLAY(CLAY_ID_LOCAL(id),                                                      \
       {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0),                    \
                              .height = CLAY_SIZING_FIXED(2)}},                \
        .backgroundColor = (Clay_Color){255, 255, 255, 50}}) {}

#define VerticalSeperatorComponent(id)                                         \
  CLAY(CLAY_ID_LOCAL(id),                                                      \
       {.layout = {.sizing = {.height = CLAY_SIZING_GROW(0),                   \
                              .width = CLAY_SIZING_FIXED(2)}},                 \
        .backgroundColor = (Clay_Color){255, 255, 255, 50}}) {}

#endif // COMPONENTS_H
