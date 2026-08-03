#include <stdio.h>
#include <string.h>

#define FLAG_IMPLEMENTATION
#include "external/flag.h"

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
    nob_cc(&cmd);

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
    nob_cmd_append(&cmd, "-O2");

#if !defined(_WIN32) || defined(__clang__)
    nob_cmd_append(&cmd, "-flto=auto");
#if defined(__clang__)
    nob_cmd_append(&cmd, "-fuse-ld=lld");
#endif // defined(__clang__)
#endif

    nob_cmd_append(&cmd, "-Iexternal/raylib");
    nob_cmd_append(&cmd, "-Iexternal/raylib/external/glfw/include");

    if (!nob_cmd_run(&cmd, .async = &procs))
      return false;

    nob_temp_rewind(mark);
  }

  if (!nob_procs_flush(&procs))
    return false;

#if defined(__clang__)
  nob_cmd_append(&cmd, "llvm-ar", "rcs", RAYLIB_LIB);
#elif defined(__GNUC__)
  nob_cmd_append(&cmd, "gcc-ar", "rcs", RAYLIB_LIB);
#else
  nob_cmd_append(&cmd, "ar", "rcs", RAYLIB_LIB);
#endif // __clang__

  for (size_t i = 0; i < NOB_ARRAY_LEN(module_names); i++) {
    nob_cmd_append(&cmd, nob_temp_sprintf(BUILD_DIR "/%s.o", module_names[i]));
  }

  return nob_cmd_run(&cmd);
}

void Usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS] [--] [ARGS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  const bool *debug_flag =
      flag_bool("debug", false,
                "Enable debug mode compilation. This adds debug symbols and "
                "sanitizers and reduces optimizations.");

  const bool *profile_flag =
      flag_bool("profile", false,
                "Enable profiling mode compilation. This adds debug symbols "
                "but leaves optimizations on, as well as setting other flags "
                "to help clean profiling.");

  const bool *headless_flag =
      flag_bool("headless", false,
                "Enable headless mode compilation. This compiles the "
                "executable without raylib and without UI support at all. This "
                "functionality is meant for server use.");

  const bool *help_flag = flag_bool("help", false, "Show help.");
  if (!flag_parse(argc, argv)) {
    Usage(stderr);
    flag_print_error(stderr);
    return 1;
  } else if (*help_flag) {
    Usage(stderr);
    return 0;
  }
  if (*debug_flag && *profile_flag) {
    nob_log(NOB_ERROR, "Debug and profile flags are mutually exclusive. You "
                       "may specify only one.");
    return 1;
  }

  nob_mkdir_if_not_exists(BUILD_DIR);

  if (!*headless_flag) {
    if (!nob_file_exists(RAYLIB_LIB))
      compile_raylib();
  }

  Nob_Cmd cmd = {0};
  nob_cc(&cmd);
  nob_cmd_append(&cmd, "-Wall", "-Wextra", "-Wno-unused-function"
                 // , "-Wpadded"
  );
  nob_cmd_append(&cmd, "src/main.c", "src/game.c", "src/tournament.c", );
  if (!*headless_flag) {
    nob_cmd_append(&cmd, "src/viewer.c");
  } else {
    nob_cmd_append(&cmd, "-DHEADLESS_MODE");
  }
  nob_cmd_append(&cmd, "-DINI_ALLOW_MULTILINE=0", "-DINI_STOP_ON_FIRST_ERROR=1",
                 "-DINI_HANDLER_LINENO=1",
                 "-DINI_CALL_HANDLER_ON_NEW_SECTION=1", "-DINI_MAX_LINE=1000",
                 "external/inih/ini.c");
  if (!*headless_flag) {
    nob_cmd_append(&cmd, RAYLIB_LIB);
  }
  nob_cmd_append(&cmd, "-isystemexternal/raylib");
  nob_cmd_append(&cmd, "-isystemexternal/inih");
  nob_cmd_append(&cmd, "-isystemexternal");
  nob_cmd_append(&cmd, "-isystem.");
  nob_cmd_append(&cmd, "-std=c11");
#if !defined(_WIN32) || defined(__GNUC__)
  nob_cmd_append(&cmd, "-lm");
#endif
#ifdef _WIN32
  nob_cmd_append(&cmd, "-lws2_32");
#endif // _WIN32
  if (!*headless_flag) {
#ifdef _WIN32
    nob_cmd_append(&cmd, "-lgdi32", "-lwinmm", "-lshcore", "-luser32",
                   "-lshell32");
#else
    nob_cmd_append(&cmd, "-lX11");
#endif // _WIN32
  }
  if (*debug_flag) {
    nob_log(NOB_WARNING, "Compiling program in debug mode");
    nob_cmd_append(&cmd, "-fsanitize=address,undefined", "-g", "-O0");
  } else if (*profile_flag) {
    nob_log(NOB_WARNING, "Compiling program in profiling mode");
    nob_cmd_append(&cmd, "-g", "-O2", "-fno-omit-frame-pointer");
  } else {
#if !defined(_WIN32) || defined(__clang__)
    nob_cmd_append(&cmd, "-flto=auto");
#if defined(__clang__)
    nob_cmd_append(&cmd, "-fuse-ld=lld");
#endif // defined(__clang__)
#endif
    nob_cmd_append(&cmd, "-O2");
  }
#ifdef _WIN32
  nob_cc_output(&cmd, "planet_wars.exe");
#else
  nob_cc_output(&cmd, "planet_wars");
#endif // _WIN32

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
