#include "runner.h"

#include <stdio.h>
#include <stdatomic.h>
#include <threads.h>

TournametData DeepCopyTournametData(TournametData tournament) {
  TournametData new_tournament = {
      .capacity = tournament.count,
      .count = tournament.count,
      .items = malloc(sizeof *tournament.items * tournament.count),
  };
  for (unsigned i = 0; i < tournament.count; i++) {
    new_tournament.items[i] = DeepCopyGameLog(tournament.items[i]);
  }
  return new_tournament;
}

void FreeInnerTournametData(TournametData tournament) {
  for (unsigned i = 0; i < tournament.count; i++) {
    FreeInnerGameLog(tournament.items[i]);
  }
  nob_da_free(tournament);
}

#define sbAppendPlayerName(sb, idx)                                            \
  if (match_args->players.items[idx].name)                                     \
    nob_sb_append_cstr(sb, match_args->players.items[idx].name);               \
  else                                                                         \
    nob_sb_appendf(sb, "Player %u", idx + 1);

typedef struct {
  unsigned p1_idx;
  unsigned p2_idx;
  PlayerDA players;
  mtx_t *idx_mtx;
  mtx_t *file_write_mtx;
  mtx_t *tournament_data_mtx;
  FILE *tournament_data_file;
  const char *map_file_path;
  TournametData *tournament_data;
} MatchRunnerArgs;

static bool AdvancePlayerIndex(MatchRunnerArgs *args, unsigned *out_p1,
                               unsigned *out_p2) {
  mtx_lock(args->idx_mtx);

  if (args->p1_idx >= args->players.count - 1) {
    mtx_unlock(args->idx_mtx);
    return false;
  }

  *out_p1 = args->p1_idx;
  *out_p2 = args->p2_idx;

  args->p2_idx++;
  if (args->p2_idx >= args->players.count) {
    args->p1_idx++;
    args->p2_idx = args->p1_idx + 1;
  }

  mtx_unlock(args->idx_mtx);
  return true;
}

int ThrdMatchRunner(void *args) {
  MatchRunnerArgs *match_args = args;
  Nob_String_Builder sb = {0};
  Nob_String_Builder temp_sb = {0};
  PlayerDA playing_players = {0};
  playing_players.count = 2;
  playing_players.capacity = playing_players.count;
  playing_players.items =
      malloc(sizeof *playing_players.items * playing_players.count);

  unsigned p1_idx;
  unsigned p2_idx;

  while (AdvancePlayerIndex(match_args, &p1_idx, &p2_idx)) {
    sb.count = 0;
    temp_sb.count = 0;

    nob_sb_append_cstr(&sb, "match:\n");
    sbAppendPlayerName(&sb, p1_idx);
    nob_sb_append(&sb, '\n');
    sbAppendPlayerName(&sb, p2_idx);
    nob_sb_append(&sb, '\n');

    playing_players.items[0] = match_args->players.items[p1_idx];
    playing_players.items[1] = match_args->players.items[p2_idx];
    // nob_log(NOB_INFO, "Running match between %s and %s", GetBotName(p1_idx),
    //         GetBotName(p2_idx));

    // Run a full game, silence normal logging so we don't clog the terminal
    // TODO split the `MakeGame` function to more sub-functions so we can load
    // maps, bots and other stuff once instead of on every match.
    GameState state = MakeGame(match_args->map_file_path, playing_players);
    RunMatch(&state);

    GameLog game_log_copy = DeepCopyGameLog(state.game_log);
    // Free as soon as possible to destroy bot threads
    FreeInnerGameState(state);

    if (game_log_copy.draw) {
      // nob_log(NOB_INFO, "Game ended in a draw");
      nob_sb_append_cstr(&sb, "draw\n");
    } else {
      unsigned winner_idx = game_log_copy.winning_player == 0 ? p1_idx : p2_idx;
      // nob_log(NOB_INFO, "%s won.", GetBotName(winner_idx));
      nob_sb_append_cstr(&sb, "winner: ");
      sbAppendPlayerName(&sb, winner_idx);
      nob_sb_append(&sb, '\n');
    }

    temp_sb.count = 0;
    nob_sb_append_cstr(&temp_sb, "tournament/match_");
    sbAppendPlayerName(&temp_sb, p1_idx);
    nob_sb_append(&temp_sb, '-');
    sbAppendPlayerName(&temp_sb, p2_idx);
    nob_sb_append_cstr(&temp_sb, ".plws");
    sb_append_null(&temp_sb);

    char *save_file_name = temp_sb.items;
    nob_sb_appendf(&sb, "save file name: %s\n", save_file_name);

    FILE *save_file = fopen(save_file_name, "wb");
    WriteGameLogToFile(save_file, game_log_copy);
    fclose(save_file);

    mtx_lock(match_args->tournament_data_mtx);
    nob_da_append(match_args->tournament_data, game_log_copy);
    mtx_unlock(match_args->tournament_data_mtx);

    mtx_lock(match_args->file_write_mtx);
    fwrite(sb.items, 1, sb.count, match_args->tournament_data_file);
    mtx_unlock(match_args->file_write_mtx);
  }
  nob_da_free(playing_players);
  nob_sb_free(sb);
  nob_sb_free(temp_sb);
  return 0;
}

TournametData RunTournament(const char *map_file_path, PlayerDA players) {
  if (players.count < 3) {
    nob_log(NOB_ERROR, "A tournament cannot be run for less then 3 bots.");
    exit(1);
  }

  nob_log(NOB_INFO, "Creating directory for tournament data.");
  if (!nob_mkdir_if_not_exists("tournament")) {
    nob_log(NOB_ERROR, "Failed creating tournament directory.");
    exit(1);
  }

  FILE *tournament_data_file = fopen("tournament/tournament.txt", "w");
  if (!tournament_data_file) {
    perror("Failed opening tournament file.");
    exit(1);
  }

  TournametData tournament = {0};

  unsigned match_count = players.count * (players.count - 1) / 2;
  // Could be nob_nprocs()-1 because we use the current thread to manage it all,
  // but this thread doesn't do a lot of work, mainly waits around, so I think
  // it fine that way.
  const unsigned proc_count = nob_nprocs();
  const unsigned thread_count =
      match_count < proc_count ? match_count : proc_count;
  thrd_t *thread_pool = malloc(sizeof *thread_pool * thread_count);

  mtx_t file_write_mtx;
  mtx_init(&file_write_mtx, mtx_plain);
  mtx_t tournament_data_mtx;
  mtx_init(&tournament_data_mtx, mtx_plain);
  mtx_t idx_mtx;
  mtx_init(&idx_mtx, mtx_plain);

  MatchRunnerArgs thread_args = {
      .p1_idx = 0,
      .p2_idx = 1,
      .idx_mtx = &idx_mtx,
      .players = players,
      .file_write_mtx = &file_write_mtx,
      .map_file_path = map_file_path,
      .tournament_data = &tournament,
      .tournament_data_mtx = &tournament_data_mtx,
      .tournament_data_file = tournament_data_file,
  };

  nob_log(NOB_INFO,
          "Running tournament between %u bots. This will run %u matches.",
          players.count, match_count);
  // All normal logging must stop as this functions are not thread-safe
  nob_minimal_log_level = NOB_WARNING;
  for (unsigned i = 0; i < thread_count; i++) {
    thrd_create(&thread_pool[i], ThrdMatchRunner, &thread_args);
  }
  for (unsigned i = 0; i < thread_count; i++) {
    thrd_join(thread_pool[i], NULL);
  }
  nob_minimal_log_level = NOB_INFO;
  nob_log(NOB_INFO,
          "Tournament finished, data is saved in `tournament` directory.");

  fclose(tournament_data_file);
  free(thread_pool);
  mtx_destroy(&file_write_mtx);
  mtx_destroy(&tournament_data_mtx);
  mtx_destroy(&idx_mtx);

  return tournament;
}

void sendMapToBot(GameState *state, Nob_String_Builder *sb, unsigned bot_idx) {
  if (bot_idx >= state->players.count) {
    nob_log(NOB_ERROR, "ERROR: Attempting access to non-existent bot process");
    exit(1);
  }

  GetMapRepresentation(state, sb, bot_idx);
  SendMessageToPlayer(state->players.items[bot_idx], sb->items, sb->count);
}

void RunBotCycle(GameState *state, Nob_String_Builder *sb) {
  unsigned bot_num = 0;
  nob_da_foreach(Player, player, &state->players) {
    // Skip disqualified or lost bots.
    if (TestBit(state->player_bit_set, bot_num)) {
      nob_log(NOB_DEBUG, "sending map to bot %u", bot_num);

      sb->count = 0;
      sendMapToBot(state, sb, bot_num);
    }
    bot_num++;
  }

  bot_num = 0;
  bool bot_okay = true;
  nob_da_foreach(Player, player, &state->players) {
    // Skip disqualified or lost bots.
    if (TestBit(state->player_bit_set, bot_num)) {
      sb->count = 0;
      bot_okay = GetPlayerMessage(*player, sb);

      if (bot_okay)
        bot_okay = SendPlayerShipsStr(state, bot_num,
                                      nob_sv_from_parts(sb->items, sb->count));

      if (bot_okay) {
        sb->count = 0;
        GetPlayerDebugMessage(*player, sb);
        if (sb->count)
          nob_log(NOB_INFO, "bot %s says: |%.*s|", player->name,
                  (unsigned)sb->count, sb->items);
      } else {
        // We don't remove the player from the dynamic array because we use the
        // DA index to address different players. Instead it is marked as
        // disqualified and not used.
        StopPlayer(&state->players.items[bot_num]);
      }

      nob_log(NOB_DEBUG, "done with bot %u, advancing to bot %u", bot_num,
              bot_num + 1);
    }
    bot_num++;
  }
}

void RunMatch(GameState *state) {
  // Reusable string builder to hold messages sent and received between the bots
  // and the engine.
  Nob_String_Builder sb = {0};

  for (int sim_turn = 0; state->remaining_players > 1 && sim_turn < 1000;
       sim_turn++) {
    nob_log(NOB_INFO, "Turn %d", sim_turn);
    // Bot communication
    sb.count = 0;
    RunBotCycle(state, &sb);

    // Game logic
    AdvanceTurn(state);
  }
  nob_log(NOB_INFO, "Game ended!");
  int winning_bot = bit_index(state->player_bit_set);
  if (winning_bot == -1) {
    nob_log(NOB_INFO, "It's a draw!");
    state->game_log.draw = true;
  } else {
    state->game_log.winning_player = winning_bot;
    state->game_log.draw = false;
    nob_log(NOB_INFO, "Bot %d won!", winning_bot + 1);
  }

  nob_sb_free(sb);
}

// For htonl/ntohl functions
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#define COMPRESSESOR_BUF_SIZE (1024 * 1024)
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

const unsigned version = 2;
const char magic[4] = {'p', 'l', 'w', 's'};

// ============================================================================
// Compression Helpers for Writing
// ============================================================================

typedef struct {
  FILE *file;
  mz_stream stream;
  unsigned char in_buf[COMPRESSESOR_BUF_SIZE];
  unsigned char out_buf[COMPRESSESOR_BUF_SIZE];
} CompressedWriter;

static bool InitCompressedWriter(CompressedWriter *cw, FILE *file) {
  memset(cw, 0, sizeof(*cw));
  cw->file = file;
  cw->stream.next_in = cw->in_buf;
  cw->stream.avail_in = 0;
  cw->stream.next_out = cw->out_buf;
  cw->stream.avail_out = COMPRESSESOR_BUF_SIZE;

  if (mz_deflateInit(&cw->stream, MZ_UBER_COMPRESSION) != MZ_OK) {
    return false;
  }
  return true;
}

static void FlushDeflateBuffer(CompressedWriter *cw, int flush) {
  cw->stream.next_in = cw->in_buf;
  while (1) {
    cw->stream.next_out = cw->out_buf;
    cw->stream.avail_out = COMPRESSESOR_BUF_SIZE;

    int status = mz_deflate(&cw->stream, flush);
    if (status != MZ_OK && status != MZ_STREAM_END && status != MZ_BUF_ERROR) {
      nob_log(NOB_ERROR, "deflate() failed with status %d.", status);
      exit(1);
    }

    size_t produced = COMPRESSESOR_BUF_SIZE - cw->stream.avail_out;
    if (produced > 0) {
      if (fwrite(cw->out_buf, 1, produced, cw->file) != produced) {
        nob_log(NOB_ERROR, "Failed writing to save file.");
        exit(1);
      }
    }

    if (flush == MZ_FINISH) {
      if (status == MZ_STREAM_END)
        break;
    } else {
      if (cw->stream.avail_in == 0)
        break;
    }
  }
  cw->stream.avail_in = 0;
  cw->stream.next_in = cw->in_buf;
}

static void WriteCompressed(CompressedWriter *cw, const void *data,
                            size_t size) {
  const unsigned char *src = (const unsigned char *)data;
  while (size > 0) {
    if (cw->stream.avail_in == COMPRESSESOR_BUF_SIZE) {
      FlushDeflateBuffer(cw, MZ_NO_FLUSH);
    }
    size_t to_copy = COMPRESSESOR_BUF_SIZE - cw->stream.avail_in;
    if (to_copy > size)
      to_copy = size;
    memcpy(cw->in_buf + cw->stream.avail_in, src, to_copy);
    cw->stream.avail_in += (unsigned int)to_copy;
    src += to_copy;
    size -= to_copy;
  }
}

static void FinishCompressedWriter(CompressedWriter *cw) {
  FlushDeflateBuffer(cw, MZ_FINISH);
  if (mz_deflateEnd(&cw->stream) != MZ_OK) {
    nob_log(NOB_ERROR, "deflateEnd() failed.");
    exit(1);
  }
}

// ============================================================================
// Decompression Helpers for Reading
// ============================================================================

typedef struct {
  FILE *file;
  mz_stream stream;
  unsigned char in_buf[COMPRESSESOR_BUF_SIZE];
  unsigned char out_buf[COMPRESSESOR_BUF_SIZE];
  size_t out_pos;
  size_t out_avail;
} CompressedReader;

static bool InitCompressedReader(CompressedReader *cr, FILE *file) {
  memset(cr, 0, sizeof(*cr));
  cr->file = file;
  if (mz_inflateInit(&cr->stream) != MZ_OK) {
    nob_log(NOB_ERROR, "inflateInit() failed!");
    return false;
  }
  return true;
}

static void FreeCompressedReader(CompressedReader *cr) {
  mz_inflateEnd(&cr->stream);
}

static bool ReadCompressed(CompressedReader *cr, void *dest, size_t size) {
  unsigned char *dst = (unsigned char *)dest;
  while (size > 0) {
    if (cr->out_avail > 0) {
      size_t to_copy = cr->out_avail < size ? cr->out_avail : size;
      memcpy(dst, cr->out_buf + cr->out_pos, to_copy);
      cr->out_pos += to_copy;
      cr->out_avail -= to_copy;
      dst += to_copy;
      size -= to_copy;
      if (size == 0)
        return true;
    }

    if (cr->stream.avail_in == 0) {
      size_t n = fread(cr->in_buf, 1, COMPRESSESOR_BUF_SIZE, cr->file);
      if (n == 0) {
        nob_log(NOB_ERROR, "Unexpected end of file while decompressing log.");
        return false;
      }
      cr->stream.next_in = cr->in_buf;
      cr->stream.avail_in = (unsigned int)n;
    }

    cr->stream.next_out = cr->out_buf;
    cr->stream.avail_out = COMPRESSESOR_BUF_SIZE;
    cr->out_pos = 0;

    int status = mz_inflate(&cr->stream, MZ_NO_FLUSH);
    if (status != MZ_OK && status != MZ_STREAM_END) {
      nob_log(NOB_ERROR, "inflate() failed with status %d.", status);
      return false;
    }

    cr->out_avail = COMPRESSESOR_BUF_SIZE - cr->stream.avail_out;
    if (cr->out_avail == 0 && status == MZ_STREAM_END) {
      nob_log(NOB_ERROR, "Reached end of compressed stream unexpectedly.");
      return false;
    }
  }
  return true;
}

// ============================================================================
// Main Serialization Functions
// ============================================================================

void WriteGameLogToFile(FILE *file, GameLog game_log) {
  // 1. Write uncompressed header (Magic & Version)
  fwrite(magic, 1, sizeof(magic), file);
  uint16_t version_net = htons((uint16_t)version);
  fwrite(&version_net, sizeof(version_net), 1, file);

  // 2. Initialize compressed stream writer
  CompressedWriter cw;
  if (!InitCompressedWriter(&cw, file)) {
    nob_log(NOB_ERROR, "deflateInit() failed!\n");
    exit(1);
  }

  union {
    float f;
    uint32_t u;
  } wrt_32_float;
  uint16_t wrt_16;
  uint32_t wrt_32;

#define WRITE(var) WriteCompressed(&cw, &(var), sizeof(var))
#define WRITE_8(var) WRITE(var)
#define WRITE_16(var)                                                          \
  do {                                                                         \
    wrt_16 = htons((uint16_t)(var));                                           \
    WRITE(wrt_16);                                                             \
  } while (0)
#define WRITE_32(var)                                                          \
  do {                                                                         \
    wrt_32 = htonl((uint32_t)(var));                                           \
    WRITE(wrt_32);                                                             \
  } while (0)
#define WRITE_float(var)                                                       \
  do {                                                                         \
    wrt_32_float.f = (float)(var);                                             \
    WRITE_32(wrt_32_float.u);                                                  \
  } while (0)

  WRITE_8(game_log.draw);
  WRITE_8(game_log.winning_player);
  WRITE_32(game_log.players.count);

  nob_da_foreach(Player, player, &game_log.players) {
    uint16_t string_length;

    string_length = (uint16_t)strlen(player->name);
    WRITE_16(string_length);
    for (uint16_t i = 0; i < string_length; i++) {
      WRITE_8(player->name[i]);
    }
  }

  WRITE_32(game_log.count);

  nob_da_foreach(LogEntry, entry, &game_log) {
    WRITE_32(entry->remaining_players);
    WRITE_32(entry->fleet_count);
    WRITE_32(entry->planet_count);

    for (unsigned i = 0; i < entry->fleet_count; i++) {
      WRITE_8(entry->fleets[i].owner);
      WRITE_8(entry->fleets[i].total);
      WRITE_8(entry->fleets[i].remaining);
      WRITE_16(entry->fleets[i].ships);
      WRITE_16(entry->fleets[i].src_id);
      WRITE_16(entry->fleets[i].dst_id);
    }

    for (unsigned i = 0; i < entry->planet_count; i++) {
      WRITE_8(entry->planets[i].owner);
      WRITE_8(entry->planets[i].growth);
      WRITE_16(entry->planets[i].ships);
      WRITE_float(entry->planets[i].coords.x);
      WRITE_float(entry->planets[i].coords.y);
    }
  }

  FinishCompressedWriter(&cw);

#undef WRITE_float
#undef WRITE_32
#undef WRITE_16
#undef WRITE_8
#undef WRITE
}

bool ReadGameLogFromFile(FILE *file, GameLog *game_log) {
  // 1. Read and verify uncompressed header (Magic & Version)
  char read_magic[sizeof(magic)];
  if (fread(read_magic, 1, sizeof(read_magic), file) != sizeof(read_magic)) {
    nob_log(NOB_ERROR, "Failed to read magic header.");
    return false;
  }
  if (memcmp(read_magic, magic, sizeof(magic)) != 0) {
    nob_log(NOB_ERROR,
            "Provided file is not a Planet Wars serialization file.");
    return false;
  }

  uint16_t read_version;
  if (fread(&read_version, sizeof(read_version), 1, file) != 1) {
    nob_log(NOB_ERROR, "Failed to read version.");
    return false;
  }
  read_version = ntohs(read_version);
  if (read_version != version) {
    nob_log(NOB_ERROR,
            "Serialization file version is unsupported. File version is %u and "
            "reader version is %u",
            read_version, version);
    return false;
  }

  // 2. Initialize compressed stream reader
  CompressedReader cr;
  if (!InitCompressedReader(&cr, file)) {
    return false;
  }

  union {
    float f;
    uint32_t u;
  } read_32_float;
  uint16_t read_16;
  uint32_t read_32;

#define READ(var) ReadCompressed(&cr, &(var), sizeof(var))
#define READ_ERROR_CHK(cond)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      nob_log(NOB_ERROR, "Reading error while reading from plws file.");       \
      FreeCompressedReader(&cr);                                               \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define READ_8(var) READ_ERROR_CHK(READ(var))
#define READ_16(var)                                                           \
  do {                                                                         \
    READ_ERROR_CHK(READ(read_16));                                             \
    var = ntohs(read_16);                                                      \
  } while (0)
#define READ_32(var)                                                           \
  do {                                                                         \
    READ_ERROR_CHK(READ(read_32));                                             \
    var = ntohl(read_32);                                                      \
  } while (0)
#define READ_float(var)                                                        \
  do {                                                                         \
    READ_32(read_32_float.u);                                                  \
    var = read_32_float.f;                                                     \
  } while (0)

  READ_8(game_log->draw);
  READ_8(game_log->winning_player);
  READ_32(game_log->players.count);

  game_log->players.items =
      malloc(sizeof *game_log->players.items * game_log->players.count);
  nob_da_foreach(Player, player, &game_log->players) {
    uint16_t string_length;
    READ_16(string_length);
    player->name = malloc(sizeof *player->name * (string_length + 1));
    READ_ERROR_CHK(ReadCompressed(&cr, player->name, string_length));
    player->name[string_length] = '\0';
    player->type = PLAYER_REPLAY;
  }

  READ_32(game_log->count);
  game_log->items = malloc(sizeof *game_log->items * game_log->count);
  game_log->capacity = game_log->count;

  nob_da_foreach(LogEntry, entry, game_log) {
    READ_32(entry->remaining_players);
    READ_32(entry->fleet_count);
    READ_32(entry->planet_count);
    entry->fleets = malloc(sizeof *entry->fleets * entry->fleet_count);
    entry->planets = malloc(sizeof *entry->planets * entry->planet_count);

    for (unsigned i = 0; i < entry->fleet_count; i++) {
      READ_8(entry->fleets[i].owner);
      READ_8(entry->fleets[i].total);
      READ_8(entry->fleets[i].remaining);
      READ_16(entry->fleets[i].ships);
      READ_16(entry->fleets[i].src_id);
      READ_16(entry->fleets[i].dst_id);
    }

    for (unsigned i = 0; i < entry->planet_count; i++) {
      READ_8(entry->planets[i].owner);
      READ_8(entry->planets[i].growth);
      READ_16(entry->planets[i].ships);
      READ_float(entry->planets[i].coords.x);
      READ_float(entry->planets[i].coords.y);
    }
  }

  FreeCompressedReader(&cr);

#undef READ_float
#undef READ_32
#undef READ_16
#undef READ_8
#undef READ_ERROR_CHK
#undef READ

  return true;
}
