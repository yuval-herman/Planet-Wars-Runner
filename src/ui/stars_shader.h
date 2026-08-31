#ifndef STARS_SHADER_H
#define STARS_SHADER_H

#include "raylib.h"

typedef struct {
  float size;       // 1.0 is default, >1.0 makes stars bigger
  float brightness; // 1.0 is default, >1.0 boosts brightness
  float density;    // 1.0 is default, >1.0 creates more star clusters
  float time_scale; // Time multiplier for animation speed (default 1.0,
                    // 0 = frozen)
  float seed;
} StarsShaderConfig;

void StarsShaderInit(StarsShaderConfig config);
void StarsShaderDraw(Rectangle rect);
void StarsShaderDestroy();

#endif // STARS_SHADER_H
