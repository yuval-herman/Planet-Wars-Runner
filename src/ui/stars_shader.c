#include "stars_shader.h"
#include "shaders.h"

#include <assert.h>
// for NULL definition
#include <stddef.h>

static Shader stars_shader;

static float timeScale;

static int timeLoc;
static int resLoc;
static int sizeLoc;
static int brightnessLoc;
static int densityLoc;
static int seedLoc;

void StarsShaderInit(StarsShaderConfig config) {
  assert(stars_shader.id == 0 &&
         "There can be only one stars shader instance active at once. Did you "
         "forget to call destroy on a previous call?");

  stars_shader = LoadShaderFromMemory(NULL, (char *)stars_source);

  timeScale = config.time_scale;

  resLoc = GetShaderLocation(stars_shader, "uResolution");
  sizeLoc = GetShaderLocation(stars_shader, "uStarSize");
  brightnessLoc = GetShaderLocation(stars_shader, "uStarBrightness");
  densityLoc = GetShaderLocation(stars_shader, "uStarDensity");
  seedLoc = GetShaderLocation(stars_shader, "uSeed");

  timeLoc = GetShaderLocation(stars_shader, "uTime");

  SetShaderValue(stars_shader, sizeLoc, &config.size, SHADER_UNIFORM_FLOAT);
  SetShaderValue(stars_shader, brightnessLoc, &config.brightness,
                 SHADER_UNIFORM_FLOAT);
  SetShaderValue(stars_shader, densityLoc, &config.density,
                 SHADER_UNIFORM_FLOAT);
  SetShaderValue(stars_shader, seedLoc, &config.seed, SHADER_UNIFORM_FLOAT);
}

void StarsShaderDraw(Rectangle rect) {
  float resolution[2] = {rect.width, rect.height};
  SetShaderValue(stars_shader, resLoc, resolution, SHADER_UNIFORM_VEC2);

  float time = (float)GetTime() * timeScale;
  SetShaderValue(stars_shader, timeLoc, &time, SHADER_UNIFORM_FLOAT);

  BeginShaderMode(stars_shader);
  DrawRectangleRec(rect, WHITE);
  EndShaderMode();
}

void StarsShaderDestroy() {
  UnloadShader(stars_shader);
  stars_shader = (Shader){0};
}
