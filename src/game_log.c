#include "game_log.h"

// For htonl/ntohl functions
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#define COMPRESSESOR_BUF_SIZE (64 * 1024)
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

static bool FlushDeflateBuffer(CompressedWriter *cw, int flush) {
  cw->stream.next_in = cw->in_buf;
  while (1) {
    cw->stream.next_out = cw->out_buf;
    cw->stream.avail_out = COMPRESSESOR_BUF_SIZE;

    int status = mz_deflate(&cw->stream, flush);
    if (status != MZ_OK && status != MZ_STREAM_END && status != MZ_BUF_ERROR) {
      nob_log(NOB_ERROR, "deflate() failed with status %d.", status);
      return false;
    }

    size_t produced = COMPRESSESOR_BUF_SIZE - cw->stream.avail_out;
    if (produced > 0) {
      if (fwrite(cw->out_buf, 1, produced, cw->file) != produced) {
        nob_log(NOB_ERROR, "Failed writing to save file.");
        return false;
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
  return true;
}

static bool WriteCompressed(CompressedWriter *cw, const void *data,
                            size_t size) {
  const unsigned char *src = (const unsigned char *)data;
  while (size > 0) {
    if (cw->stream.avail_in == COMPRESSESOR_BUF_SIZE) {
      if (!FlushDeflateBuffer(cw, MZ_NO_FLUSH)) {
        return false;
      }
    }
    size_t to_copy = COMPRESSESOR_BUF_SIZE - cw->stream.avail_in;
    if (to_copy > size)
      to_copy = size;
    memcpy(cw->in_buf + cw->stream.avail_in, src, to_copy);
    cw->stream.avail_in += (unsigned int)to_copy;
    src += to_copy;
    size -= to_copy;
  }
  return true;
}

static bool FinishCompressedWriter(CompressedWriter *cw) {
  bool ok = FlushDeflateBuffer(cw, MZ_FINISH);
  if (mz_deflateEnd(&cw->stream) != MZ_OK) {
    nob_log(NOB_ERROR, "deflateEnd() failed.");
    return false;
  }
  return ok;
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

bool WriteGameLogToFile(FILE *file, GameLog game_log) {
  // 1. Write uncompressed header (Magic & Version)
  if (fwrite(magic, 1, sizeof(magic), file) != sizeof(magic)) {
    nob_log(NOB_ERROR, "Failed to write magic header.");
    return false;
  }
  uint16_t version_net = htons((uint16_t)version);
  if (fwrite(&version_net, sizeof(version_net), 1, file) != 1) {
    nob_log(NOB_ERROR, "Failed to write version.");
    return false;
  }

  // 2. Initialize compressed stream writer
  CompressedWriter cw;
  if (!InitCompressedWriter(&cw, file)) {
    nob_log(NOB_ERROR, "deflateInit() failed!\n");
    return false;
  }

  union {
    float f;
    uint32_t u;
  } wrt_32_float;
  uint16_t wrt_16;
  uint32_t wrt_32;

#define WRITE(var) WriteCompressed(&cw, &(var), sizeof(var))
#define WRITE_ERROR_CHK(cond)                                                  \
  do {                                                                         \
    if (!(cond)) {                                                             \
      nob_log(NOB_ERROR, "Writing error while writing to plws file.");         \
      mz_deflateEnd(&cw.stream);                                               \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define WRITE_8(var) WRITE_ERROR_CHK(WRITE(var))
#define WRITE_16(var)                                                          \
  do {                                                                         \
    wrt_16 = htons((uint16_t)(var));                                           \
    WRITE_ERROR_CHK(WRITE(wrt_16));                                            \
  } while (0)
#define WRITE_32(var)                                                          \
  do {                                                                         \
    wrt_32 = htonl((uint32_t)(var));                                           \
    WRITE_ERROR_CHK(WRITE(wrt_32));                                            \
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
    WRITE_16(player->name.count);
    for (uint16_t i = 0; i < player->name.count; i++) {
      WRITE_8(player->name.items[i]);
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

  if (!FinishCompressedWriter(&cw)) {
    return false;
  }

#undef WRITE_float
#undef WRITE_32
#undef WRITE_16
#undef WRITE_8
#undef WRITE_ERROR_CHK
#undef WRITE

  return true;
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
      calloc(game_log->players.count, sizeof *game_log->players.items);
  nob_da_foreach(Player, player, &game_log->players) {
    uint16_t string_length;
    READ_16(string_length);
    player->name.items =
        malloc(sizeof *player->name.items * (string_length + 1));
    READ_ERROR_CHK(ReadCompressed(&cr, player->name.items, string_length));
    player->name.count = string_length;
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
