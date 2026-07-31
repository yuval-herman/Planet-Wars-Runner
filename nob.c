#include <stdio.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_DIR "build"
#ifdef _WIN32
#define RAYLIB_LIB BUILD_DIR "/libraylib.lib"
#else
#define RAYLIB_LIB BUILD_DIR "/libraylib.a"
#endif // _WIN32

/**
 * All functions returning bool return true on success.
 */

bool compile_raylib() {
  Nob_Cmd cmd = {0};
  Nob_Procs procs = {0};

  const char *module_names[] = {"rcore", "rglfw",   "rshapes", "rtextures",
                                "rtext", "rmodels", "raudio"};

  for (size_t i = 0; i < NOB_ARRAY_LEN(module_names); i++) {
    size_t mark = nob_temp_save();
    nob_cmd_append(&cmd, "gcc");

    nob_cmd_append(&cmd, "-c");
    nob_cmd_append(&cmd,
                   nob_temp_sprintf("external/raylib/%s.c", module_names[i]));
    nob_cmd_append(&cmd, "-o",
                   nob_temp_sprintf(BUILD_DIR "/%s.o", module_names[i]));

    nob_cmd_append(&cmd, "-D_GNU_SOURCE");
    if (strcmp(module_names[i], "rglfw") == 0)
      nob_cmd_append(&cmd, "-U_GNU_SOURCE");
    nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP_GLFW");
    nob_cmd_append(&cmd, "-DGRAPHICS_API_OPENGL_33");

#ifdef _WIN32
    nob_cmd_append(&cmd, "-DUNICODE");
#else
    nob_cmd_append(&cmd, "-D_GLFW_X11");
    nob_cmd_append(&cmd, "-fPIC");
#endif // _WIN32

    nob_cmd_append(&cmd, "-fno-strict-aliasing");
    nob_cmd_append(&cmd, "-std=c99");
    nob_cmd_append(&cmd, "-flto=auto", "-O2");

    nob_cmd_append(&cmd, "-Iexternal/raylib");
    nob_cmd_append(&cmd, "-Iexternal/raylib/external/glfw/include");

    if (!nob_cmd_run(&cmd, .async = &procs))
      return false;

    nob_temp_rewind(mark);
  }

  if (!nob_procs_flush(&procs))
    return false;
  nob_cmd_append(&cmd, "gcc-ar", "rcs", RAYLIB_LIB);
  for (size_t i = 0; i < NOB_ARRAY_LEN(module_names); i++) {
    nob_cmd_append(&cmd, nob_temp_sprintf(BUILD_DIR "/%s.o", module_names[i]));
  }

  return nob_cmd_run(&cmd);
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  nob_mkdir_if_not_exists(BUILD_DIR);

  if (!nob_file_exists(RAYLIB_LIB))
    compile_raylib();

  Nob_Cmd cmd = {0};
  nob_cc(&cmd);
  nob_cmd_append(&cmd, "-Wall", "-Wextra"
                 // , "-Wpadded"
  );
  nob_cmd_append(&cmd, "src/main.c", "src/game.c", "src/viewer.c",
                 "src/tournament.c", );
  nob_cmd_append(&cmd, "-DINI_ALLOW_MULTILINE=0", "-DINI_STOP_ON_FIRST_ERROR=1",
                 "-DINI_HANDLER_LINENO=1",
                 "-DINI_CALL_HANDLER_ON_NEW_SECTION=1", "-DINI_MAX_LINE=1000",
                 "external/inih/ini.c");
  nob_cmd_append(&cmd, RAYLIB_LIB);
  nob_cmd_append(&cmd, "-isystemexternal/raylib");
  nob_cmd_append(&cmd, "-isystemexternal/inih");
  nob_cmd_append(&cmd, "-isystemexternal");
  nob_cmd_append(&cmd, "-isystem.");
  nob_cmd_append(&cmd, "-lm");
#ifdef _WIN32
  nob_cmd_append(&cmd, "-lgdi32", "-lwinmm", "-lshcore");
#else
  nob_cmd_append(&cmd, "-lX11");
#endif // _WIN32
  // Enable debug mode
  if (argc > 1) {
    nob_log(NOB_WARNING, "Compiling program in debug mode");
    nob_cmd_append(&cmd, "-fsanitize=address,undefined", "-g", "-O0");
  } else {
    nob_cmd_append(&cmd, "-flto=auto", "-O2");
  }
  nob_cc_output(&cmd, "planet_wars");

  FILE *compile_flags_file = fopen("compile_flags.txt", "w");
  if (!compile_flags_file) {
    nob_log(NOB_WARNING, "Failed creating compile_flags.txt file: %s",
            strerror(errno));
  } else {
    nob_da_foreach(const char *, line, &cmd) {
      fprintf(compile_flags_file, "%s\n", *line);
    }
    fclose(compile_flags_file);
  }

  if (!nob_cmd_run(&cmd))
    return 1;
  return 0;
}
