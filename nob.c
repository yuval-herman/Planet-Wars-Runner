#include <stdio.h>
#include <string.h>

#define FLAG_IMPLEMENTATION
#include "external/flag.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_DIR "build"
#ifdef _WIN32
#define RAYLIB_LIB BUILD_DIR "/libraylib.lib"
#define PATH_SEP   "\\"
#else
#define RAYLIB_LIB BUILD_DIR "/libraylib.a"
#define PATH_SEP   "/"
#endif

/**
 * All functions returning bool return true on success.
 */

static void print_banner(const char *msg) {
  printf("=============== %s ===============\n", msg);
}

static void print_usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTIONS]\n", flag_program_name());
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

static char *c_to_o_path(const char *source_file_path) {
  return nob_temp_sprintf("build/%.*s.o", (int)strlen(source_file_path + 4) - 2,
                          source_file_path + 4);
}

static char *c_to_h(const char *source_file_path) {
  return nob_temp_sprintf("%.*sh", (int)strlen(source_file_path) - 1,
                          source_file_path);
}

// ---------------------------------------------------------------------------
// Compilation flag helpers
// ---------------------------------------------------------------------------

static void add_compile_mode_flags(Nob_Cmd *cmd, bool debug, bool profile) {
  if (debug) {
    nob_log(NOB_WARNING, "Compiling program in debug mode");
    nob_cmd_append(cmd, "-fsanitize=address,undefined", "-g", "-O0",
                   "-fno-omit-frame-pointer");
  } else if (profile) {
    nob_log(NOB_WARNING, "Compiling program in profiling mode");
    nob_cmd_append(cmd, "-g", "-O2", "-fno-omit-frame-pointer");
  } else {
#if defined(__clang__)
    nob_cmd_append(cmd, "-flto", "-fuse-ld=lld");
#elif !defined(_WIN32)
    nob_cmd_append(cmd, "-flto");
#endif
    nob_cmd_append(cmd, "-O2");
  }
}

static void add_include_paths(Nob_Cmd *cmd) {
  nob_cmd_append(cmd, "-isystemexternal/raylib");
  nob_cmd_append(cmd, "-isystemexternal/inih");
  nob_cmd_append(cmd, "-isystemexternal/miniz");
  nob_cmd_append(cmd, "-isystemexternal");
  nob_cmd_append(cmd, "-isystem.");
}

// ---------------------------------------------------------------------------
// Raylib
// ---------------------------------------------------------------------------

static bool compile_raylib(void) {
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
#endif

    nob_cmd_append(&cmd, "-fno-strict-aliasing");
    nob_cmd_append(&cmd, "-std=c99");
    nob_cmd_append(&cmd, "-O2");

#if defined(__clang__)
    nob_cmd_append(&cmd, "-flto", "-fuse-ld=lld");
#elif !defined(_WIN32)
    nob_cmd_append(&cmd, "-flto");
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
#endif

  for (size_t i = 0; i < NOB_ARRAY_LEN(module_names); i++) {
    nob_cmd_append(&cmd, nob_temp_sprintf(BUILD_DIR "/%s.o", module_names[i]));
  }

  return nob_cmd_run(&cmd);
}

// ---------------------------------------------------------------------------
// Shader embedding
// ---------------------------------------------------------------------------

static bool embed_files_walker(Nob_Walk_Entry entry) {
  FILE *shaders_file = entry.data;
  Nob_String_Builder sb = {0};

  if (entry.type == NOB_FILE_REGULAR) {
    nob_read_entire_file(entry.path, &sb);

    // Strip directory prefix, keeping only the filename
    Nob_String_View sv = {.data = entry.path, .count = strlen(entry.path)};
    unsigned i = 0;
    while (i < sv.count && PATH_SEP[0] != sv.data[sv.count - 1 - i]) {
      i += 1;
    }
    sv.data += sv.count - i;
    sv.count = i;

    // Strip file extension
    sv = nob_sv_chop_by_delim(&sv, '.');

    fprintf(shaders_file, "const char %.*s_shader_source[] = ",
            (int)sv.count, sv.data);

    sv.data = sb.items;
    sv.count = sb.count;
    while (sv.count > 0) {
      Nob_String_View line = nob_sv_chop_by_delim(&sv, '\n');
      fprintf(shaders_file, "\"%.*s\\n\"\n", (int)line.count, line.data);
    }
    fputc(';', shaders_file);

    nob_log(NOB_INFO, "Embedded %s.", entry.path);
  }
  // The first directory we walk is the target directory itself; only warn for
  // unexpected non-regular files at deeper levels.
  else if (entry.level > 1) {
    nob_log(NOB_WARNING, "While traversing directory to embed files, "
                         "encountered a non-regular file. Skipping.");
    *entry.action = NOB_WALK_SKIP;
  }

  nob_sb_free(sb);
  return true;
}

static void embed_shaders(void) {
  FILE *shaders_file = fopen("src/shaders.c", "w");
  nob_walk_dir("shaders", embed_files_walker, .data = shaders_file);
  fclose(shaders_file);
}

// ---------------------------------------------------------------------------
// Compilation database + per-file compilation
// ---------------------------------------------------------------------------

// Return true if all files should be recompiled.
// Checks whether the compilation flags changed since last time.
static bool should_recompile_all(bool headless, bool debug, bool profile) {
  const char *flag_file_path = "prev-comp.txt";
  Nob_String_Builder prev = {0};
  Nob_String_Builder curr = {0};
  bool ret = false;

  if (!nob_file_exists(flag_file_path))
    ret = true;
  if (!nob_read_entire_file(flag_file_path, &prev))
    ret = true;

  nob_sb_appendf(&curr, "headless=%s\n", headless ? "true" : "false");
  nob_sb_appendf(&curr, "debug=%s\n",    debug    ? "true" : "false");
  nob_sb_appendf(&curr, "profile=%s\n",  profile  ? "true" : "false");
  nob_sb_appendf(&curr, "test=%s\n",     "false"); // reserved for future use

  nob_write_entire_file(flag_file_path, curr.items, curr.count);

  // ret == true means recompilation IS required; equal flags → no recompile
  if (!ret) ret = prev.count != curr.count;
  if (!ret) ret = 0 != memcmp(prev.items, curr.items, prev.count);

  nob_sb_free(curr);
  nob_sb_free(prev);
  return ret;
}

static bool compile_file(Nob_Cmd *cmd, Nob_Procs *procs,
                          FILE *compile_commands_file,
                          const char *file_path,
                          bool headless, bool debug, bool profile,
                          bool force_recompile) {
  const char *output_path = c_to_o_path(file_path);

  nob_cc(cmd);
  nob_cmd_append(cmd, "-c");
  nob_cmd_append(cmd, "-Wall", "-Wextra", "-Wno-unused-function");
  nob_cmd_append(cmd, "-o", output_path);
  nob_cmd_append(cmd, file_path);

  if (headless)
    nob_cmd_append(cmd, "-DHEADLESS_MODE");

  add_include_paths(cmd);
  nob_cmd_append(cmd, "-std=gnu11");

#ifdef _WIN32
  nob_cmd_append(cmd, "-lws2_32");
#endif

  if (!headless) {
#ifdef _WIN32
    nob_cmd_append(cmd, "-lgdi32", "-lwinmm", "-lshcore", "-luser32",
                   "-lshell32");
#else
    nob_cmd_append(cmd, "-lX11");
#endif
  }

  add_compile_mode_flags(cmd, debug, profile);

  // Append entry to the compilation database
  fprintf(compile_commands_file,
          "{\"directory\":\"%s\","
          "\"file\":\"%s\","
          "\"output\":\"%s\","
          "\"arguments\":[",
          nob_get_current_dir_temp(), file_path, output_path);
  nob_da_foreach(const char *, arg, cmd) {
    fprintf(compile_commands_file, "\"%s\",", *arg);
  }
  fprintf(compile_commands_file, "]},");

  const char *sources[] = {file_path, c_to_h(file_path)};
  unsigned source_count = NOB_ARRAY_LEN(sources);
  // Don't include the .h if it doesn't exist when checking rebuild need
  if (!nob_file_exists(sources[1]))
    source_count--;

  if (force_recompile || nob_needs_rebuild(output_path, sources, source_count)) {
    return nob_cmd_run(cmd, .async = procs);
  } else {
    nob_log(NOB_INFO, "Skipped building %s", file_path);
    cmd->count = 0;
    return true;
  }
}

// ---------------------------------------------------------------------------
// Link steps
// ---------------------------------------------------------------------------

static bool link_main_executable(Nob_Cmd *cmd,
                                  const char *source_files[],
                                  unsigned source_files_count,
                                  const char *headed_source_files[],
                                  unsigned headed_source_files_count,
                                  FILE *compile_commands_file,
                                  bool headless, bool debug, bool profile) {
  nob_cc(cmd);
  for (unsigned i = 0; i < source_files_count; i++) {
    nob_cmd_append(cmd, c_to_o_path(source_files[i]));
  }

  if (!headless) {
    for (unsigned i = 0; i < headed_source_files_count; i++) {
      nob_cmd_append(cmd, c_to_o_path(headed_source_files[i]));
    }
    nob_cmd_append(cmd, RAYLIB_LIB);
  }

  add_compile_mode_flags(cmd, debug, profile);

#if !defined(_WIN32) || defined(__GNUC__)
  nob_cmd_append(cmd, "-lm");
#endif

#ifdef _WIN32
  nob_cc_output(cmd, "planet_wars.exe");
#else
  nob_cc_output(cmd, "planet_wars");
#endif

  fprintf(compile_commands_file, "]");
  fclose(compile_commands_file);
  return nob_cmd_run(cmd);
}

static bool compile_and_run_tests(Nob_Cmd *cmd,
                                   const char *source_files[],
                                   unsigned source_files_count,
                                   bool debug, bool profile) {
  nob_cc(cmd);
  nob_cc_flags(cmd);
  nob_cc_inputs(cmd, "tests/test-runner.c");
  add_compile_mode_flags(cmd, debug, profile);

  // Skip source_files[0] (main.c) — tests provide their own entry point
  for (unsigned i = 1; i < source_files_count; i++) {
    nob_cmd_append(cmd, c_to_o_path(source_files[i]));
  }

  add_include_paths(cmd);
  nob_cmd_append(cmd, "-Isrc");
  nob_cmd_append(cmd, "-lcmocka");

#if !defined(_WIN32) || defined(__GNUC__)
  nob_cmd_append(cmd, "-lm");
#endif

#ifdef _WIN32
  nob_cc_output(cmd, BUILD_DIR PATH_SEP "test.exe");
#else
  nob_cc_output(cmd, BUILD_DIR PATH_SEP "test");
#endif

  if (!nob_cmd_run(cmd)) {
    nob_log(NOB_ERROR, "Failed to compile tests.");
    return false;
  }

  print_banner("STARTING TESTS");

#ifdef _WIN32
  nob_cmd_append(cmd, ".\\build\\test");
#else
  nob_cmd_append(cmd, "./build/test");
#endif
  return nob_cmd_run(cmd);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

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

  bool *headless_flag =
      flag_bool("headless", false,
                "Enable headless mode compilation. This compiles the "
                "executable without raylib and without UI support at all. This "
                "functionality is meant for server use.");

  const bool *force_flag =
      flag_bool("force", false,
                "Force recompilation of project file, even if they are "
                "unchanged. This does not force recompilation of library "
                "files; to recompile everything from scratch, delete the "
                "build directory.");

  const bool *test_flag =
      flag_bool("test", false,
                "Run project tests. This compiles the test programs and does "
                "not recompile the entire project. Unless you know what you "
                "are doing, it's best to run this together with -force and "
                "-debug.");

  const bool *help_flag = flag_bool("help", false, "Show help.");

  if (!flag_parse(argc, argv)) {
    print_usage(stderr);
    flag_print_error(stderr);
    return 1;
  }
  if (*help_flag) {
    print_usage(stderr);
    return 0;
  }
  if (*debug_flag && *profile_flag) {
    nob_log(NOB_ERROR, "Debug and profile flags are mutually exclusive. "
                       "You may specify only one.");
    return 1;
  }
  if (*test_flag && !*headless_flag) {
    nob_log(NOB_WARNING, "Compiling tests automatically uses headless mode.");
    *headless_flag = true;
  }

  // should_recompile_all() must run before the force flag is OR-ed in, because
  // it also persists the current flags to disk for future comparison.
  bool force_rebuild =
      should_recompile_all(*headless_flag, *debug_flag, *profile_flag) ||
      *force_flag;

  FILE *compile_commands_file = fopen("compile_commands.json", "w");
  if (!compile_commands_file) {
    nob_log(NOB_WARNING, "Failed creating compile_commands.json: %s",
            strerror(errno));
  } else {
    fprintf(compile_commands_file, "[");
  }

  embed_shaders();
  nob_mkdir_if_not_exists(BUILD_DIR);

  if (!*headless_flag && !nob_file_exists(RAYLIB_LIB)) {
    if (!compile_raylib())
      return 1;
  }

  Nob_Cmd cmd = {0};
  Nob_Procs procs = {0};

  const char *source_files[] = {
      "src/main.c", "src/game.c",   "src/runner.c", "src/configs.c",
      "src/bot.c",  "src/player.c", "src/utils.c",  "src/game_log.c"};
  const char *headed_source_files[] = {"src/viewer.c"};

  for (unsigned i = 0; i < NOB_ARRAY_LEN(source_files); i++) {
    if (!compile_file(&cmd, &procs, compile_commands_file, source_files[i],
                      *headless_flag, *debug_flag, *profile_flag, force_rebuild))
      return 1;
  }

  if (!*headless_flag) {
    for (unsigned i = 0; i < NOB_ARRAY_LEN(headed_source_files); i++) {
      if (!compile_file(&cmd, &procs, compile_commands_file,
                         headed_source_files[i], *headless_flag, *debug_flag,
                         *profile_flag, force_rebuild))
        return 1;
    }
  }

  if (!nob_procs_flush(&procs))
    return 1;

  if (*test_flag) {
    if (!compile_and_run_tests(&cmd, source_files, NOB_ARRAY_LEN(source_files),
                                *debug_flag, *profile_flag))
      return 1;
  } else {
    bool ok = link_main_executable(&cmd,
                                   source_files, NOB_ARRAY_LEN(source_files),
                                   headed_source_files,
                                   NOB_ARRAY_LEN(headed_source_files),
                                   compile_commands_file,
                                   *headless_flag, *debug_flag, *profile_flag);
    print_banner(ok ? "COMPILATION FINISHED SUCCESSFULLY" : "COMPILATION FAILED");
    if (!ok) return 1;
  }

  return 0;
}
