#include <stdio.h>
#include <string.h>

#define FLAG_IMPLEMENTATION
#include "external/flag.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_DIR "build"
#ifdef _WIN32
#define RAYLIB_LIB BUILD_DIR "/libraylib.lib"
#define PATH_SEP "\\"
#else
#define RAYLIB_LIB BUILD_DIR "/libraylib.a"
#define PATH_SEP "/"
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
  size_t str_len = strlen(source_file_path) - 1;
  size_t i = str_len;
  while (i > 0 && source_file_path[i] != '/') {
    i--;
  }

  return nob_temp_sprintf("build/%.*s.o", (int)(str_len - i - 2),
                          source_file_path + i + 1);
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
    nob_cmd_append(cmd, "-fsanitize=address,undefined",
                   "-fno-sanitize-recover=undefined", "-g", "-O0",
                   "-fno-omit-frame-pointer", "-fno-lto");
  } else if (profile) {
    nob_cmd_append(cmd, "-g", "-O2", "-fno-omit-frame-pointer");
#if defined(__clang__)
    nob_cmd_append(cmd, "-flto", "-fuse-ld=lld");
#elif !defined(_WIN32)
    nob_cmd_append(cmd, "-flto");
#endif
  } else {
#if defined(__clang__)
    nob_cmd_append(cmd, "-flto", "-fuse-ld=lld");
#elif !defined(_WIN32)
    nob_cmd_append(cmd, "-flto");
#endif
    nob_cmd_append(cmd, "-O2");
  }
#ifdef _WIN32
  nob_cmd_append(cmd, "-D_CRT_SECURE_NO_WARNINGS",
                 "-D_CRT_NONSTDC_NO_DEPRECATE");
#endif
}

static void add_linker_flags(Nob_Cmd *cmd, bool headless) {
#ifdef _WIN32
  nob_cmd_append(cmd, "-lws2_32");
#endif

  if (!headless) {
#ifdef _WIN32
    nob_cmd_append(cmd, "-lgdi32", "-lwinmm", "-lshcore", "-luser32",
                   "-lshell32", "-lcomdlg32", "-lole32");
#else
    nob_cmd_append(cmd, "-lX11");
#endif
  }

#if !defined(_WIN32) || defined(__GNUC__)
  nob_cmd_append(cmd, "-lm");
#endif
}

static void add_include_paths(Nob_Cmd *cmd) {
  nob_cmd_append(cmd, "-isystemexternal/raylib");
  nob_cmd_append(cmd, "-isystemexternal/inih");
  nob_cmd_append(cmd, "-isystemexternal/miniz");
  nob_cmd_append(cmd, "-isystemexternal/tinyfiledialogs");
  nob_cmd_append(cmd, "-isystemexternal");
  nob_cmd_append(cmd, "-isystem.");
}

// ---------------------------------------------------------------------------
// Raylib
// ---------------------------------------------------------------------------

static bool compile_raylib(bool wasm) {
  Nob_Cmd cmd = {0};
  Nob_Procs procs = {0};

  const char *module_names[] = {"rcore",   "rshapes", "rtextures", "rtext",
                                "rmodels", "raudio",  "rglfw"};
  unsigned module_names_length = NOB_ARRAY_LEN(module_names);
  // Skip rglfw when compiling wasm
  if (wasm) {
    module_names_length--;
  }

  for (size_t i = 0; i < module_names_length; i++) {
    size_t mark = nob_temp_save();
    if (wasm) {
      nob_cmd_append(&cmd, "emcc");
    } else {
      nob_cc(&cmd);
    }

    nob_cmd_append(&cmd, "-c");
    nob_cmd_append(&cmd,
                   nob_temp_sprintf("external/raylib/%s.c", module_names[i]));
    nob_cmd_append(&cmd, "-o",
                   nob_temp_sprintf(BUILD_DIR "/%s.o", module_names[i]));

    nob_cmd_append(&cmd, "-D_GNU_SOURCE");
    if (strcmp(module_names[i], "rglfw") == 0)
      nob_cmd_append(&cmd, "-U_GNU_SOURCE");

#ifdef _WIN32
    nob_cmd_append(&cmd, "-DUNICODE");
#else
    nob_cmd_append(&cmd, "-fPIC");
#endif

    if (wasm) {
      nob_cmd_append(&cmd, "-std=gnu99");
      nob_cmd_append(&cmd, "-DPLATFORM_WEB");
      nob_cmd_append(&cmd, "-DGRAPHICS_API_OPENGL_ES3");
      nob_cmd_append(&cmd, "-sMAX_WEBGL_VERSION=2");
      nob_cmd_append(&cmd, "-Os");
      nob_cmd_append(&cmd, "-flto");
    } else {
      nob_cmd_append(&cmd, "-std=c99");

      nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP_GLFW");
      nob_cmd_append(&cmd, "-DGRAPHICS_API_OPENGL_33");
#ifndef _WIN32
      nob_cmd_append(&cmd, "-D_GLFW_X11");
#endif

      nob_cmd_append(&cmd, "-O2");

#if defined(__clang__)
      nob_cmd_append(&cmd, "-flto", "-ffat-lto-objects", "-fuse-ld=lld");
#elif !defined(_WIN32)
      nob_cmd_append(&cmd, "-flto", "-ffat-lto-objects");
#endif
    }

    nob_cmd_append(&cmd, "-fno-strict-aliasing");

    nob_cmd_append(&cmd, "-Iexternal/raylib");
    nob_cmd_append(&cmd, "-Iexternal/raylib/external/glfw/include");

    if (!nob_cmd_run(&cmd, .async = &procs))
      return false;

    nob_temp_rewind(mark);
  }

  if (!nob_procs_flush(&procs))
    return false;

  if (wasm) {
    nob_cmd_append(&cmd, "emar", "rcs", RAYLIB_LIB);
  } else {
#if defined(__clang__)
    nob_cmd_append(&cmd, "llvm-ar", "rcs", RAYLIB_LIB);
#elif defined(__GNUC__)
    nob_cmd_append(&cmd, "gcc-ar", "rcs", RAYLIB_LIB);
#else
    nob_cmd_append(&cmd, "ar", "rcs", RAYLIB_LIB);
#endif
  }
  for (size_t i = 0; i < module_names_length; i++) {
    nob_cmd_append(&cmd, nob_temp_sprintf(BUILD_DIR "/%s.o", module_names[i]));
  }

  return nob_cmd_run(&cmd);
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

  if (!nob_file_exists(flag_file_path) ||
      !nob_read_entire_file(flag_file_path, &prev)) {
    ret = true;
  }

  nob_sb_appendf(&curr, "headless=%s\n", headless ? "true" : "false");
  nob_sb_appendf(&curr, "debug=%s\n", debug ? "true" : "false");
  nob_sb_appendf(&curr, "profile=%s\n", profile ? "true" : "false");

  nob_write_entire_file(flag_file_path, curr.items, curr.count);

  // ret == true means recompilation IS required; equal flags → no recompile
  if (!ret)
    ret = prev.count != curr.count;
  if (!ret)
    ret = 0 != memcmp(prev.items, curr.items, prev.count);

  nob_sb_free(curr);
  nob_sb_free(prev);
  return ret;
}

static void create_compile_cmd(Nob_Cmd *cmd, const char *file_path,
                               const char *output_path, bool wasm,
                               bool headless, bool debug, bool profile) {
  if (wasm) {
    nob_cmd_append(cmd, "emcc");
  } else {
    nob_cc(cmd);
  }
  nob_cmd_append(cmd, "-c");
  nob_cmd_append(cmd, "-Wall", "-Wextra", "-Wno-unused-function", "-Wshadow");
  if (output_path) {
    nob_cmd_append(cmd, "-o", output_path);
  }
  nob_cmd_append(cmd, file_path);

  if (headless)
    nob_cmd_append(cmd, "-DHEADLESS_MODE");
  if (wasm)
    nob_cmd_append(cmd, "-DWASM_MODE");

  add_include_paths(cmd);
  nob_cmd_append(cmd, "-std=gnu11");

  add_linker_flags(cmd, headless);

  add_compile_mode_flags(cmd, debug, profile);
}

static void create_test_compile_cmd(Nob_Cmd *cmd, const char *file_path,
                                    const char *output_path, bool debug,
                                    bool profile) {
  nob_cc(cmd);
  nob_cmd_append(cmd, "-c");
  nob_cmd_append(cmd, "-Wall", "-Wextra", "-Wno-unused-function");
  if (output_path) {
    nob_cmd_append(cmd, "-o", output_path);
  }
  nob_cmd_append(cmd, file_path);
  nob_cmd_append(cmd, "-DHEADLESS_MODE");
  add_include_paths(cmd);
  nob_cmd_append(cmd, "-Isrc");
  nob_cmd_append(cmd, "-std=gnu11");
  add_compile_mode_flags(cmd, debug, profile);
}

static void append_compile_command(FILE *f, const char *file_path,
                                   const char *output_path, const Nob_Cmd *cmd,
                                   bool *first_entry) {
  if (!f)
    return;

  if (!*first_entry) {
    fprintf(f, ",\n");
  }
  *first_entry = false;

  fprintf(f, "  {\n");
  fprintf(f, "    \"directory\": \"%s\",\n", nob_get_current_dir_temp());
  fprintf(f, "    \"file\": \"%s\",\n", file_path);
  if (output_path) {
    fprintf(f, "    \"output\": \"%s\",\n", output_path);
  }
  fprintf(f, "    \"arguments\": [");
  for (size_t i = 0; i < cmd->count; ++i) {
    fprintf(f, "%s\"%s\"", (i > 0 ? ", " : ""), cmd->items[i]);
  }
  fprintf(f, "]\n  }");
}

typedef struct {
  FILE *file;
  bool debug;
  bool profile;
  bool *first_entry;
  Nob_Cmd *cmd;
} Test_Walker_Context;

static bool append_test_file_walker(Nob_Walk_Entry entry) {
  Test_Walker_Context *ctx = entry.data;
  if (entry.type == NOB_FILE_REGULAR) {
    Nob_String_View sv = nob_sv_from_cstr(entry.path);
    if (nob_sv_ends_with_cstr(sv, ".c") == 0) {
      ctx->cmd->count = 0;
      create_test_compile_cmd(ctx->cmd, entry.path, NULL, ctx->debug,
                              ctx->profile);
      append_compile_command(ctx->file, entry.path, NULL, ctx->cmd,
                             ctx->first_entry);
    }
  }
  return true;
}

static void append_tests_compile_commands(FILE *f, bool debug, bool profile,
                                          bool *first_entry) {
  Nob_Cmd cmd = {0};
  Test_Walker_Context ctx = {
      .file = f,
      .debug = debug,
      .profile = profile,
      .first_entry = first_entry,
      .cmd = &cmd,
  };
  nob_walk_dir("tests", append_test_file_walker, .data = &ctx);
  cmd_free(cmd);
}

static bool compile_file(Nob_Cmd *cmd, Nob_Procs *procs, const char *file_path,
                         const char *output_path, bool force_recompile) {
  const char *sources[] = {file_path, c_to_h(file_path)};
  unsigned source_count = NOB_ARRAY_LEN(sources);
  // Don't include the .h if it doesn't exist when checking rebuild need
  if (!nob_file_exists(sources[1]))
    source_count--;

  if (force_recompile ||
      nob_needs_rebuild(output_path, sources, source_count)) {
    return nob_cmd_run(cmd, .async = procs);
  } else {
    nob_log(NOB_INFO, "Skipped building %s", file_path);
    cmd->count = 0;
    return true;
  }
}

// ---------------------------------------------------------------------------
// Library compilation
// ---------------------------------------------------------------------------

static bool compile_libraries(bool wasm, bool headless, bool force) {
  Nob_Cmd cmd = {0};
  Nob_Procs procs = {0};

  char *file_path;
  char *output_path;

  // miniz
  file_path = "external/miniz/miniz.c";
  output_path = BUILD_DIR "/miniz.o";
  if (wasm) {
    nob_cmd_append(&cmd, "emcc");
  } else {
    nob_cc(&cmd);
  }
  nob_cmd_append(&cmd, "-c");
  nob_cmd_append(&cmd, file_path);
  nob_cmd_append(&cmd, "-o", output_path);
  nob_cmd_append(&cmd, "-isystemexternal/miniz");
  nob_cmd_append(&cmd, "-std=gnu11");
  if (!compile_file(&cmd, &procs, file_path, output_path, force))
    return false;

  if (!headless) {
    // tinyfiledialogs
    file_path = "external/tinyfiledialogs/tinyfiledialogs.c";
    output_path = BUILD_DIR "/tinyfiledialogs.o";
    if (wasm) {
      nob_cmd_append(&cmd, "emcc");
    } else {
      nob_cc(&cmd);
    }
    nob_cmd_append(&cmd, "-c");
    nob_cmd_append(&cmd, file_path);
    nob_cmd_append(&cmd, "-o", output_path);
    nob_cmd_append(&cmd, "-isystemexternal/tinyfiledialogs");
    nob_cmd_append(&cmd, "-std=gnu11");
    if (!compile_file(&cmd, &procs, file_path, output_path, force))
      return false;
  } else {
    // raymath
    file_path = "external/raylib/raymath.h";
    output_path = BUILD_DIR "/raymath.o";
    if (wasm) {
      nob_cmd_append(&cmd, "emcc");
    } else {
      nob_cc(&cmd);
    }
    nob_cmd_append(&cmd, "-c");
    nob_cmd_append(&cmd, "-x", "c");
    nob_cmd_append(&cmd, "-DRAYMATH_IMPLEMENTATION");
    nob_cmd_append(&cmd, "-isystemexternal/raylib");
    nob_cmd_append(&cmd, file_path);
    nob_cmd_append(&cmd, "-o", output_path);
    nob_cmd_append(&cmd, "-std=gnu11");
    if (!compile_file(&cmd, &procs, file_path, output_path, force))
      return false;
  }

  return nob_procs_flush(&procs);
}

// ---------------------------------------------------------------------------
// Shader embedding
// ---------------------------------------------------------------------------

struct EmbedFilesData {
  FILE *source_file;
  FILE *header_file;
  bool append_null; // If we embedd a string, like a shader
  unsigned amount;  // Amount of files that were embedded. Zero out before
                    // calling the walker!
};

static bool embed_files_walker(Nob_Walk_Entry entry) {
  struct EmbedFilesData *data = entry.data;
  FILE *source_file = data->source_file;
  FILE *header_file = data->header_file;
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

    fprintf(header_file, "extern const unsigned char %.*s_source[];\n",
            (int)sv.count, sv.data);

    fprintf(header_file, "extern const unsigned %.*s_size;\n", (int)sv.count,
            sv.data);

    fprintf(source_file, "const unsigned %.*s_size = %zu;\n", (int)sv.count,
            sv.data, sb.count + (data->append_null));

    fprintf(source_file, "const unsigned char %.*s_source[] = {\n",
            (int)sv.count, sv.data);

    unsigned char bytes_in_line = 0;
    nob_da_foreach(char, byte, &sb) {
      if (bytes_in_line > 10) {
        fputc('\n', source_file);
        bytes_in_line = 0;
      }
      fprintf(source_file, "0x%02x,", (unsigned char)*byte);
      bytes_in_line++;
    }
    if (data->append_null) {
      fputs("0x0", source_file);
    }
    fputs("};\n\n", source_file);

    nob_log(NOB_INFO, "Embedded %s.", entry.path);
    data->amount++;
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
  FILE *shaders_source = fopen("src/ui/shaders.c", "w");
  FILE *shaders_header = fopen("src/ui/shaders.h", "w");
  struct EmbedFilesData data = {
      .source_file = shaders_source,
      .header_file = shaders_header,
      .append_null = true,
      .amount = 0,
  };

  nob_walk_dir("assets/shaders", embed_files_walker, .data = &data);
  fclose(shaders_source);
  fclose(shaders_header);
}

static void embed_fonts(void) {
  FILE *fonts_source = fopen("src/ui/fonts.c", "w");
  FILE *fonts_header = fopen("src/ui/fonts.h", "w");
  struct EmbedFilesData data = {
      .source_file = fonts_source,
      .header_file = fonts_header,
      .append_null = false,
      .amount = 0,
  };

  nob_walk_dir("assets/fonts", embed_files_walker, .data = &data);
  fclose(fonts_source);
  fclose(fonts_header);
}

// ---------------------------------------------------------------------------
// Link steps
// ---------------------------------------------------------------------------

static bool link_main_executable(Nob_Cmd *cmd, const char *source_files[],
                                 unsigned source_files_count,
                                 const char *headed_source_files[],
                                 unsigned headed_source_files_count, bool wasm,
                                 bool headless, bool debug, bool profile) {
  if (wasm) {
    nob_cmd_append(cmd, "emcc");
    nob_cmd_append(cmd, "-sUSE_GLFW=3");
    nob_cmd_append(cmd, "-sMAX_WEBGL_VERSION=2");
  } else {
    nob_cc(cmd);
  }
  for (unsigned i = 0; i < source_files_count; i++) {
    nob_cmd_append(cmd, c_to_o_path(source_files[i]));
  }

  if (!headless) {
    for (unsigned i = 0; i < headed_source_files_count; i++) {
      nob_cmd_append(cmd, c_to_o_path(headed_source_files[i]));
    }
    nob_cmd_append(cmd, RAYLIB_LIB);
    nob_cmd_append(cmd, BUILD_DIR "/tinyfiledialogs.o");
  } else {
    nob_cmd_append(cmd, BUILD_DIR "/raymath.o");
  }

  // miniz is always needed
  nob_cmd_append(cmd, BUILD_DIR "/miniz.o");

  add_compile_mode_flags(cmd, debug, profile);

  add_linker_flags(cmd, headless);

  if (wasm) {
    nob_cc_output(cmd, "planet_wars.html");
  } else {
#ifdef _WIN32
    nob_cc_output(cmd, "planet_wars.exe");
#else
    nob_cc_output(cmd, "planet_wars");
#endif
  }

  return nob_cmd_run(cmd);
}

static bool compile_and_run_tests(Nob_Cmd *cmd, const char *source_files[],
                                  unsigned source_files_count, bool debug,
                                  bool profile) {
  nob_cc(cmd);
  nob_cc_flags(cmd);
  nob_cc_inputs(cmd, "tests/test-runner.c");
  add_compile_mode_flags(cmd, debug, profile);

  // Skip source_files[0] (main.c) — tests provide their own entry point
  for (unsigned i = 1; i < source_files_count; i++) {
    nob_cmd_append(cmd, c_to_o_path(source_files[i]));
  }

  add_include_paths(cmd);
  nob_cmd_append(cmd, "-Isrc", "-Itests");
  nob_cmd_append(cmd, "-lcmocka");

#if !defined(_WIN32) || defined(__GNUC__)
  nob_cmd_append(cmd, "-lm");
#endif

#ifdef _WIN32
  nob_cc_output(cmd, BUILD_DIR "/test.exe");
#else
  nob_cc_output(cmd, BUILD_DIR "/test");
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

static void color_log_handler(Nob_Log_Level level, const char *fmt,
                              va_list args) {
  switch (level) {
  case NOB_DEBUG:
    fprintf(stderr, "[DEBUG] ");
    break;
  case NOB_INFO:
    fprintf(stderr, "\x1b[36m[INFO]\x1b[0m ");
    break;
  case NOB_WARNING:
    fprintf(stderr, "\x1b[33m[WARNING]\x1b[0m ");
    break;
  case NOB_ERROR:
    fprintf(stderr, "\x1b[31m[ERROR]\x1b[0m ");
    break;
  case NOB_NO_LOGS:
    return;
  default:
    NOB_UNREACHABLE("Nob_Log_Level");
  }

  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
  nob_set_log_handler(color_log_handler);

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

  const bool *wasm_flag = flag_bool("wasm", false,
                                    "Compile a wasm object instead of a native "
                                    "executable. This relies on emscripten.");

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

  if (*debug_flag) {
    nob_log(NOB_WARNING, "Compiling program in debug mode");
  } else if (*profile_flag) {
    nob_log(NOB_WARNING, "Compiling program in profiling mode");
  }

  // should_recompile_all() must run before the force flag is OR-ed in, because
  // it also persists the current flags to disk for future comparison.
  bool force_rebuild =
      should_recompile_all(*headless_flag, *debug_flag, *profile_flag) ||
      *force_flag;

  FILE *compile_commands_file = fopen("compile_commands.json", "w");
  bool first_compile_cmd = true;
  if (!compile_commands_file) {
    nob_log(NOB_WARNING, "Failed creating compile_commands.json: %s",
            strerror(errno));
  } else {
    fprintf(compile_commands_file, "[\n");
  }

  embed_shaders();
  embed_fonts();
  nob_mkdir_if_not_exists(BUILD_DIR);

  if (!*headless_flag && !nob_file_exists(RAYLIB_LIB)) {
    if (!compile_raylib(*wasm_flag))
      return 1;
  }

  if (!compile_libraries(*wasm_flag, *headless_flag, *force_flag))
    return 1;

  Nob_Cmd cmd = {0};
  Nob_Procs procs = {0};

  const char *source_files[] = {
      "src/main.c", "src/game.c",   "src/runner.c", "src/configs.c",
      "src/bot.c",  "src/player.c", "src/utils.c",  "src/game_log.c",
  };
  const char *headed_source_files[] = {
      "src/ui/ui.c",
      "src/ui/viewer.c",
      "src/ui/menu.c",
      "src/ui/human_game.c",
      "src/ui/shaders.c",
      "src/ui/fonts.c",
      "src/ui/stars_shader.c",
      "src/ui/components/text_edit.c",
      "src/ui/components/button.c",
      "src/ui/components/game_frame.c",
  };

  for (unsigned i = 0; i < NOB_ARRAY_LEN(source_files); i++) {
    const char *file_path = source_files[i];
    const char *output_path = c_to_o_path(file_path);

    create_compile_cmd(&cmd, file_path, output_path, *wasm_flag, *headless_flag,
                       *debug_flag, *profile_flag);
    append_compile_command(compile_commands_file, file_path, output_path, &cmd,
                           &first_compile_cmd);

    if (!compile_file(&cmd, &procs, file_path, output_path, force_rebuild))
      return 1;
  }

  if (!*headless_flag) {
    for (unsigned i = 0; i < NOB_ARRAY_LEN(headed_source_files); i++) {
      const char *file_path = headed_source_files[i];
      const char *output_path = c_to_o_path(file_path);

      create_compile_cmd(&cmd, file_path, output_path, *wasm_flag,
                         *headless_flag, *debug_flag, *profile_flag);
      append_compile_command(compile_commands_file, file_path, output_path,
                             &cmd, &first_compile_cmd);

      if (!compile_file(&cmd, &procs, file_path, output_path, force_rebuild))
        return 1;
    }
  }

  // Always register test files into the compilation database so editors/LSP
  // work seamlessly
  append_tests_compile_commands(compile_commands_file, *debug_flag,
                                *profile_flag, &first_compile_cmd);

  if (!nob_procs_flush(&procs))
    return 1;

  // Finalize and close the compilation database
  if (compile_commands_file) {
    fprintf(compile_commands_file, "\n]\n");
    fclose(compile_commands_file);
  }

  if (*test_flag) {
    if (!compile_and_run_tests(&cmd, source_files, NOB_ARRAY_LEN(source_files),
                               *debug_flag, *profile_flag))
      return 1;
  } else {
    bool ok = link_main_executable(
        &cmd, source_files, NOB_ARRAY_LEN(source_files), headed_source_files,
        NOB_ARRAY_LEN(headed_source_files), *wasm_flag, *headless_flag,
        *debug_flag, *profile_flag);
    print_banner(ok ? "COMPILATION FINISHED SUCCESSFULLY"
                    : "COMPILATION FAILED");
    if (!ok)
      return 1;
  }

  return 0;
}
