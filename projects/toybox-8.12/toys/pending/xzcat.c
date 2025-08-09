/* xzcat.c - Simple XZ decoder command line tool
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 * Modified for toybox by Isaac Dunham
 *
 * See http://tukaani.org/xz/xz-file-format.txt
USE_XZCAT(NEWTOY(xzcat, NULL, TOYFLAG_USR|TOYFLAG_BIN))

config XZCAT
  bool "xzcat"
  default n
  help
    usage: xzcat [FILE...]

    Decompress listed files to stdout. Use stdin if no files listed.

*/
#define FOR_xzcat
#include "toys.h"

// BEGIN xz.h

enum xz_ret {
  // Doing fine, More input or output space needed
  XZ_OK,
  // EOF, Everything went fine
  XZ_STREAM_END,
  // Integrity check type is not supported. Decoding is still possible in
  // multi-call mode by simply calling xz_dec_run() again.  Note that this
  // return value is used only if XZ_DEC_ANY_CHECK was defined at build
  // time, which is not used in the kernel. Unsupported check types return
  // XZ_OPTIONS_ERROR if XZ_DEC_ANY_CHECK was not defined at build time.
  XZ_UNSUPPORTED_CHECK,
  // Cant allocate memory
  XZ_MEM_ERROR,
  // OOM
  XZ_MEMLIMIT_ERROR,
  // Not a xz file
  XZ_FORMAT_ERROR,
  // Compression option not available
  XZ_OPTIONS_ERROR,
  // Corrupt Data
  XZ_DATA_ERROR,
  // Can't make progress
  XZ_BUF_ERROR,
  // XZ_BUF_ERROR is returned when two consecutive calls to XZ code cannot
  // consume any input and cannot produce any new output. This happens when
  // there is no new input available, or the output buffer is full while at
  // least one output byte is still pending. Assuming your code is not buggy,
  // you can get this error only when decoding a compressed stream that is
  // truncated or otherwise corrupt.
};

// Passing input and output buffers to XZ code
// Only the contents of the output buffer from out[out_pos] onward, and
// the variables in_pos and out_pos are modified by the XZ code.
struct xz_buf {
  // buffer begins (Can be 0 IF pos == size)
  const char *in;
  // buffer position
  size_t in_pos;
  // buffer size
  size_t in_size;

  char *out;
  size_t out_pos;
  size_t out_size;
};

// Opaque type to hold the XZ decoder state
struct xz_dec;

// Update CRC32 value using the polynomial from IEEE-802.3. To start a new
// calculation, the third argument must be zero. To continue the calculation,
// the previously returned value is passed as the third argument.
static unsigned xz_crc32_table[256];

unsigned xz_crc32(const char *buf, size_t size, unsigned crc)
{
  crc = ~crc;

  while (size) {
    crc = xz_crc32_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
    --size;
  }

  return ~crc;
}

static uint64_t xz_crc64_table[256];


// END xz.h
// BEGIN xz_private.h

#define memeq(a, b, size) (!memcmp(a, b, size))

/* Inline functions to access unaligned unsigned 32-bit integers */
static unsigned get_unaligned_le32(const char *buf)
{
  return (unsigned)buf[0]
      | ((unsigned)buf[1] << 8)
      | ((unsigned)buf[2] << 16)
      | ((unsigned)buf[3] << 24);
}

static unsigned get_unaligned_be32(const char *buf)
{
  return (unsigned)(buf[0] << 24)
      | ((unsigned)buf[1] << 16)
      | ((unsigned)buf[2] << 8)
      | (unsigned)buf[3];
}

static void put_unaligned_le32(unsigned val, char *buf)
{
  buf[0] = (char)val;
  buf[1] = (char)(val >> 8);
  buf[2] = (char)(val >> 16);
  buf[3] = (char)(val >> 24);
}

static void put_unaligned_be32(unsigned val, char *buf)
{
  buf[0] = (char)(val >> 24);
  buf[1] = (char)(val >> 16);
  buf[2] = (char)(val >> 8);
  buf[3] = (char)val;
}

// Allocate memory for LZMA2 decoder. xz_dec_lzma2_reset() must be used
// before calling xz_dec_lzma2_run().
struct xz_dec_lzma2 *xz_dec_lzma2_create(unsigned dict_max);

// Decode the LZMA2 properties (one byte) and reset the decoder. Return
// XZ_OK on success, XZ_MEMLIMIT_ERROR if the preallocated dictionary is not
// big enough, and XZ_OPTIONS_ERROR if props indicates something that this
// decoder doesn't support.
enum xz_ret xz_dec_lzma2_reset(struct xz_dec_lzma2 *s,
           char props);

/* Decode raw LZMA2 stream from b->in to b->out. */
enum xz_ret xz_dec_lzma2_run(struct xz_dec_lzma2 *s,
               struct xz_buf *b);

// END "xz_private.h"




// Branch/Call/Jump (BCJ) filter decoders

struct xz_dec_bcj {
  /* Type of the BCJ filter being used */
  enum {
    BCJ_X86 = 4,        /* x86 or x86-64 */
    BCJ_POWERPC = 5,    /* Big endian only */
    BCJ_IA64 = 6,       /* Big or little endian */
    BCJ_ARM = 7,        /* Little endian only */
    BCJ_ARMTHUMB = 8,   /* Little endian only */
    BCJ_SPARC = 9       /* Big or little endian */
  } type;

  /* Return value of the next filter in the chain. We need to preserve
   * this information across calls, because we must not call the next
   * filter anymore once it has returned XZ_STREAM_END.
   */
  enum xz_ret ret;

  /* Absolute position relative to the beginning of the uncompressed
   * data (in a single .xz Block). We care only about the lowest 32
   * bits so this doesn't need to be uint64_t even with big files.
   */
  unsigned pos;

  /* x86 filter state */
  unsigned x86_prev_mask;

  /* Temporary space to hold the variables from struct xz_buf */
  char *out;
  size_t out_pos;
  size_t out_size;

  struct {
    /* Amount of already filtered data in the beginning of buf */
    size_t filtered;

    /* Total amount of data currently stored in buf  */
    size_t size;

    /*
     * Buffer to hold a mix of filtered and unfiltered data. This
     * needs to be big enough to hold Alignment + 2 * Look-ahead:
     *
     * Type         Alignment   Look-ahead
     * x86              1           4
     * PowerPC          4           0
     * IA-64           16           0
     * ARM              4           0
     * ARM-Thumb        2           2
     * SPARC            4           0
     */
    char buf[16];
  } temp;
};

// This is used to test the most significant byte of a memory address
// in an x86 instruction.
static int bcj_x86_test_msbyte(char b)
{
  return !b || !~b;
}

static size_t bcj_x86(struct xz_dec_bcj *s, char *buf, size_t size)
{
  static const int mask_to_allowed_status[8]
    = { 1,1,1,0,1,0,0,0 };

  static const char mask_to_bit_num[8] = { 0, 1, 2, 2, 3, 3, 3, 3 };

  size_t i;
  size_t prev_pos = (size_t)-1;
  unsigned prev_mask = s->x86_prev_mask;
  unsigned src;
  unsigned dest;
  unsigned j;
  char b;

  if (size <= 4)
    return 0;

  size -= 4;
  for (i = 0; i < size; ++i) {
    if ((buf[i] & 0xFE) != 0xE8)
      continue;

    prev_pos = i - prev_pos;
    if (prev_pos > 3) {
      prev_mask = 0;
    } else {
      prev_mask = (prev_mask << (prev_pos - 1)) & 7;
      if (prev_mask) {
        b = buf[i + 4 - mask_to_bit_num[prev_mask]];
        if (!mask_to_allowed_status[prev_mask]
            || bcj_x86_test_msbyte(b)) {
          prev_pos = i;
          prev_mask = (prev_mask << 1) | 1;
          continue;
        }
      }
    }

    prev_pos = i;

    if (bcj_x86_test_msbyte(buf[i + 4])) {
      src = get_unaligned_le32(buf + i + 1);
      for (;;) {
        dest = src - (s->pos + (unsigned)i + 5);
        if (!prev_mask)
          break;

        j = mask_to_bit_num[prev_mask] * 8;
        b = (char)(dest >> (24 - j));
        if (!bcj_x86_test_msbyte(b))
          break;

        src = dest ^ (((unsigned)1 << (32 - j)) - 1);
      }

      dest &= 0x01FFFFFF;
      dest |= (unsigned)0 - (dest & 0x01000000);
      put_unaligned_le32(dest, buf + i + 1);
      i += 4;
    } else {
      prev_mask = (prev_mask << 1) | 1;
    }
  }

  prev_pos = i - prev_pos;
  s->x86_prev_mask = prev_pos > 3 ? 0 : prev_mask << (prev_pos - 1);
  return i;
}

static size_t bcj_powerpc(struct xz_dec_bcj *s, char *buf, size_t size)
{
  size_t i;
  unsigned instr;

  for (i = 0; i + 4 <= size; i += 4) {
    instr = get_unaligned_be32(buf + i);
    if ((instr & 0xFC000003) == 0x48000001) {
      instr &= 0x03FFFFFC;
      instr -= s->pos + (unsigned)i;
      instr &= 0x03FFFFFC;
      instr |= 0x48000001;
      put_unaligned_be32(instr, buf + i);
    }
  }

  return i;
}

static size_t bcj_ia64(struct xz_dec_bcj *s, char *buf, size_t size)
{
  static const char branch_table[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    4, 4, 6, 6, 0, 0, 7, 7,
    4, 4, 0, 0, 4, 4, 0, 0
  };

  /*
   * The local variables take a little bit stack space, but it's less
   * than what LZMA2 decoder takes, so it doesn't make sense to reduce
   * stack usage here without doing that for the LZMA2 decoder too.
   */

  /* Loop counters */
  size_t i;
  size_t j;

  /* Instruction slot (0, 1, or 2) in the 128-bit instruction word */
  unsigned slot;

  /* Bitwise offset of the instruction indicated by slot */
  unsigned bit_pos;

  /* bit_pos split into byte and bit parts */
  unsigned byte_pos;
  unsigned bit_res;

  /* Address part of an instruction */
  unsigned addr;

  /* Mask used to detect which instructions to convert */
  unsigned mask;

  /* 41-bit instruction stored somewhere in the lowest 48 bits */
  uint64_t instr;

  /* Instruction normalized with bit_res for easier manipulation */
  uint64_t norm;

  for (i = 0; i + 16 <= size; i += 16) {
    mask = branch_table[buf[i] & 0x1F];
    for (slot = 0, bit_pos = 5; slot < 3; ++slot, bit_pos += 41) {
      if (!((mask >> slot) & 1))
        continue;

      byte_pos = bit_pos >> 3;
      bit_res = bit_pos & 7;
      instr = 0;
      for (j = 0; j < 6; ++j)
        instr |= (uint64_t)(buf[i + j + byte_pos])
            << (8 * j);

      norm = instr >> bit_res;

      if (((norm >> 37) & 0x0F) == 5
          && !((norm >> 9) & 7)) {
        addr = (norm >> 13) & 0x0FFFFF;
        addr |= ((unsigned)(norm >> 36) & 1) << 20;
        addr <<= 4;
        addr -= s->pos + (unsigned)i;
        addr >>= 4;

        norm &= ~((uint64_t)0x8FFFFF << 13);
        norm |= (uint64_t)(addr & 0x0FFFFF) << 13;
        norm |= (uint64_t)(addr & 0x100000)
            << (36 - 20);

        instr &= (1 << bit_res) - 1;
        instr |= norm << bit_res;

        for (j = 0; j < 6; j++)
          buf[i + j + byte_pos]
            = (char)(instr >> (8 * j));
      }
    }
  }

  return i;
}

static size_t bcj_arm(struct xz_dec_bcj *s, char *buf, size_t size)
{
  size_t i;
  unsigned addr;

  for (i = 0; i + 4 <= size; i += 4) {
    if (buf[i + 3] == 0xEB) {
      addr = (unsigned)buf[i] | ((unsigned)buf[i + 1] << 8)
          | ((unsigned)buf[i + 2] << 16);
      addr <<= 2;
      addr -= s->pos + (unsigned)i + 8;
      addr >>= 2;
      buf[i] = (char)addr;
      buf[i + 1] = (char)(addr >> 8);
      buf[i + 2] = (char)(addr >> 16);
    }
  }

  return i;
}

static size_t bcj_armthumb(struct xz_dec_bcj *s, char *buf, size_t size)
{
  size_t i;
  unsigned addr;

  for (i = 0; i + 4 <= size; i += 2) {
    if ((buf[i + 1] & 0xF8) == 0xF0
        && (buf[i + 3] & 0xF8) == 0xF8) {
      addr = (((unsigned)buf[i + 1] & 7) << 19)
          | ((unsigned)buf[i] << 11)
          | (((unsigned)buf[i + 3] & 7) << 8)
          | (unsigned)buf[i + 2];
      addr <<= 1;
      addr -= s->pos + (unsigned)i + 4;
      addr >>= 1;
      buf[i + 1] = (char)(0xF0 | ((addr >> 19) & 7));
      buf[i] = (char)(addr >> 11);
      buf[i + 3] = (char)(0xF8 | ((addr >> 8) & 7));
      buf[i + 2] = (char)addr;
      i += 2;
    }
  }

  return i;
}

static size_t bcj_sparc(struct xz_dec_bcj *s, char *buf, size_t size)
{
  size_t i;
  unsigned instr;

  for (i = 0; i + 4 <= size; i += 4) {
    instr = get_unaligned_be32(buf + i);
    if ((instr >> 22) == 0x100 || (instr >> 22) == 0x1FF) {
      instr <<= 2;
      instr -= s->pos + (unsigned)i;
      instr >>= 2;
      instr = ((unsigned)0x40000000 - (instr & 0x400000))
          | 0x40000000 | (instr & 0x3FFFFF);
      put_unaligned_be32(instr, buf + i);
    }
  }

  return i;
}

/*
 * Apply the selected BCJ filter. Update *pos and s->pos to match the amount
 * of data that got filtered.
 *
 * NOTE: This is implemented as a switch statement to avoid using function
 * pointers, which could be problematic in the kernel boot code, which must
 * avoid pointers to static data (at least on x86).
 */
static void bcj_apply(struct xz_dec_bcj *s,
          char *buf, size_t *pos, size_t size)
{
  size_t filtered;

  buf += *pos;
  size -= *pos;

  switch (s->type) {
  case BCJ_X86:
    filtered = bcj_x86(s, buf, size);
    break;
  case BCJ_POWERPC:
    filtered = bcj_powerpc(s, buf, size);
    break;
  case BCJ_IA64:
    filtered = bcj_ia64(s, buf, size);
    break;
  case BCJ_ARM:
    filtered = bcj_arm(s, buf, size);
    break;
  case BCJ_ARMTHUMB:
    filtered = bcj_armthumb(s, buf, size);
    break;
  case BCJ_SPARC:
    filtered = bcj_sparc(s, buf, size);
    break;
  default:
    /* Never reached but silence compiler warnings. */
    filtered = 0;
    break;
  }

  *pos += filtered;
  s->pos += filtered;
}

/*
 * Flush pending filtered data from temp to the output buffer.
 * Move the remaining mixture of possibly filtered and unfiltered
 * data to the beginning of temp.
 */
static void bcj_flush(struct xz_dec_bcj *s, struct xz_buf *b)
{
  size_t copy_size;

  copy_size = minof(s->temp.filtered, b->out_size - b->out_pos);
  memcpy(b->out + b->out_pos, s->temp.buf, copy_size);
  b->out_pos += copy_size;

  s->temp.filtered -= copy_size;
  s->temp.size -= copy_size;
  memmove(s->temp.buf, s->temp.buf + copy_size, s->temp.size);
}

// Decode raw BCJ + LZMA2 stream. This must be used only if there actually is
// a BCJ filter in the chain. If the chain has only LZMA2, xz_dec_lzma2_run()
// must be called directly.
// The BCJ filter functions are primitive in sense that they process the
// data in chunks of 1-16 bytes. To hide this issue, this function does
// some buffering.
enum xz_ret xz_dec_bcj_run(struct xz_dec_bcj *s, struct xz_dec_lzma2 *lzma2,
             struct xz_buf *b)
{
  size_t out_start;

  /*
   * Flush pending already filtered data to the output buffer. Return
   * immediatelly if we couldn't flush everything, or if the next
   * filter in the chain had already returned XZ_STREAM_END.
   */
  if (s->temp.filtered > 0) {
    bcj_flush(s, b);
    if (s->temp.filtered > 0)
      return XZ_OK;

    if (s->ret == XZ_STREAM_END)
      return XZ_STREAM_END;
  }

  /*
   * If we have more output space than what is currently pending in
   * temp, copy the unfiltered data from temp to the output buffer
   * and try to fill the output buffer by decoding more data from the
   * next filter in the chain. Apply the BCJ filter on the new data
   * in the output buffer. If everything cannot be filtered, copy it
   * to temp and rewind the output buffer position accordingly.
   *
   * This needs to be always run when temp.size == 0 to handle a special
   * case where the output buffer is full and the next filter has no
   * more output coming but hasn't returned XZ_STREAM_END yet.
   */
  if (s->temp.size < b->out_size - b->out_pos || !s->temp.size) {
    out_start = b->out_pos;
    memcpy(b->out + b->out_pos, s->temp.buf, s->temp.size);
    b->out_pos += s->temp.size;

    s->ret = xz_dec_lzma2_run(lzma2, b);
    if (s->ret != XZ_STREAM_END
        && (s->ret != XZ_OK ))
      return s->ret;

    bcj_apply(s, b->out, &out_start, b->out_pos);

    /*
     * As an exception, if the next filter returned XZ_STREAM_END,
     * we can do that too, since the last few bytes that remain
     * unfiltered are meant to remain unfiltered.
     */
    if (s->ret == XZ_STREAM_END)
      return XZ_STREAM_END;

    s->temp.size = b->out_pos - out_start;
    b->out_pos -= s->temp.size;
    memcpy(s->temp.buf, b->out + b->out_pos, s->temp.size);

    /*
     * If there wasn't enough input to the next filter to fill
     * the output buffer with unfiltered data, there's no point
     * to try decoding more data to temp.
     */
    if (b->out_pos + s->temp.size < b->out_size)
      return XZ_OK;
  }

  /*
   * We have unfiltered data in temp. If the output buffer isn't full
   * yet, try to fill the temp buffer by decoding more data from the
   * next filter. Apply the BCJ filter on temp. Then we hopefully can
   * fill the actual output buffer by copying filtered data from temp.
   * A mix of filtered and unfiltered data may be left in temp; it will
   * be taken care on the next call to this function.
   */
  if (b->out_pos < b->out_size) {
    /* Make b->out{,_pos,_size} temporarily point to s->temp. */
    s->out = b->out;
    s->out_pos = b->out_pos;
    s->out_size = b->out_size;
    b->out = s->temp.buf;
    b->out_pos = s->temp.size;
    b->out_size = sizeof(s->temp.buf);

    s->ret = xz_dec_lzma2_run(lzma2, b);

    s->temp.size = b->out_pos;
    b->out = s->out;
    b->out_pos = s->out_pos;
    b->out_size = s->out_size;

    if (s->ret != XZ_OK && s->ret != XZ_STREAM_END)
      return s->ret;

    bcj_apply(s, s->temp.buf, &s->temp.filtered, s->temp.size);

    /*
     * If the next filter returned XZ_STREAM_END, we mark that
     * everything is filtered, since the last unfiltered bytes
     * of the stream are meant to be left as is.
     */
    if (s->ret == XZ_STREAM_END)
      s->temp.filtered = s->temp.size;

    bcj_flush(s, b);
    if (s->temp.filtered > 0)
      return XZ_OK;
  }

  return s->ret;
}

/*
 * Decode the Filter ID of a BCJ filter. This implementation doesn't
 * support custom start offsets, so no decoding of Filter Properties
 * is needed. Returns XZ_OK if the given Filter ID is supported.
 * Otherwise XZ_OPTIONS_ERROR is returned.
 */
enum xz_ret xz_dec_bcj_reset(struct xz_dec_bcj *s, char id)
{
  s->type = id;
  s->ret = XZ_OK;
  s->pos = 0;
  s->x86_prev_mask = 0;
  s->temp.filtered = 0;
  s->temp.size = 0;

  return XZ_OK;
}

/*
 * LZMA2 decoder
 */


// BEGIN xz_lzma2.h
/*
 * LZMA2 definitions
 *
 */


/* Range coder constants */
#define RC_SHIFT_BITS 8
#define RC_TOP_BITS 24
#define RC_TOP_VALUE (1 << RC_TOP_BITS)
#define RC_BIT_MODEL_TOTAL_BITS 11
#define RC_BIT_MODEL_TOTAL (1 << RC_BIT_MODEL_TOTAL_BITS)
#define RC_MOVE_BITS 5

/*
 * Maximum number of position states. A position state is the lowest pb
 * number of bits of the current uncompressed offset. In some places there
 * are different sets of probabilities for different position states.
 */
#define POS_STATES_MAX (1 << 4)

/*
 * This enum is used to track which LZMA symbols have occurred most recently
 * and in which order. This information is used to predict the next symbol.
 *
 * Symbols:
 *  - Literal: One 8-bit byte
 *  - Match: Repeat a chunk of data at some distance
 *  - Long repeat: Multi-byte match at a recently seen distance
 *  - Short repeat: One-byte repeat at a recently seen distance
 *
 * The symbol names are in from STATE_oldest_older_previous. REP means
 * either short or long repeated match, and NONLIT means any non-literal.
 */
enum lzma_state {
  STATE_LIT_LIT,
  STATE_MATCH_LIT_LIT,
  STATE_REP_LIT_LIT,
  STATE_SHORTREP_LIT_LIT,
  STATE_MATCH_LIT,
  STATE_REP_LIT,
  STATE_SHORTREP_LIT,
  STATE_LIT_MATCH,
  STATE_LIT_LONGREP,
  STATE_LIT_SHORTREP,
  STATE_NONLIT_MATCH,
  STATE_NONLIT_REP
};

/* Total number of states */
#define STATES 12

/* The lowest 7 states indicate that the previous state was a literal. */
#define LIT_STATES 7

/* Indicate that the latest symbol was a literal. */
static void lzma_state_literal(enum lzma_state *state)
{
  if (*state <= STATE_SHORTREP_LIT_LIT)
    *state = STATE_LIT_LIT;
  else if (*state <= STATE_LIT_SHORTREP)
    *state -= 3;
  else
    *state -= 6;
}

/* Indicate that the latest symbol was a match. */
static void lzma_state_match(enum lzma_state *state)
{
  *state = *state < LIT_STATES ? STATE_LIT_MATCH : STATE_NONLIT_MATCH;
}

/* Indicate that the latest state was a long repeated match. */
static void lzma_state_long_rep(enum lzma_state *state)
{
  *state = *state < LIT_STATES ? STATE_LIT_LONGREP : STATE_NONLIT_REP;
}

/* Indicate that the latest symbol was a short match. */
static void lzma_state_short_rep(enum lzma_state *state)
{
  *state = *state < LIT_STATES ? STATE_LIT_SHORTREP : STATE_NONLIT_REP;
}

/* Each literal coder is divided in three sections:
 *   - 0x001-0x0FF: Without match byte
 *   - 0x101-0x1FF: With match byte; match bit is 0
 *   - 0x201-0x2FF: With match byte; match bit is 1
 *
 * Match byte is used when the previous LZMA symbol was something else than
 * a literal (that is, it was some kind of match).
 */
#define LITERAL_CODER_SIZE 0x300

/* Maximum number of literal coders */
#define LITERAL_CODERS_MAX (1 << 4)

/* Minimum length of a match is two bytes. */
#define MATCH_LEN_MIN 2

/* Match length is encoded with 4, 5, or 10 bits.
 *
 * Length   Bits
 *  2-9      4 = Choice=0 + 3 bits
 * 10-17     5 = Choice=1 + Choice2=0 + 3 bits
 * 18-273   10 = Choice=1 + Choice2=1 + 8 bits
 */
#define LEN_LOW_BITS 3
#define LEN_LOW_SYMBOLS (1 << LEN_LOW_BITS)
#define LEN_MID_BITS 3
#define LEN_MID_SYMBOLS (1 << LEN_MID_BITS)
#define LEN_HIGH_BITS 8
#define LEN_HIGH_SYMBOLS (1 << LEN_HIGH_BITS)
#define LEN_SYMBOLS (LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS + LEN_HIGH_SYMBOLS)

/*
 * Maximum length of a match is 273 which is a result of the encoding
 * described above.
 */
#define MATCH_LEN_MAX (MATCH_LEN_MIN + LEN_SYMBOLS - 1)

/*
 * Different sets of probabilities are used for match distances that have
 * very short match length: Lengths of 2, 3, and 4 bytes have a separate
 * set of probabilities for each length. The matches with longer length
 * use a shared set of probabilities.
 */
#define DIST_STATES 4

/*
 * Get the index of the appropriate probability array for decoding
 * the distance slot.
 */
static unsigned lzma_get_dist_state(unsigned len)
{
  return len < DIST_STATES + MATCH_LEN_MIN
      ? len - MATCH_LEN_MIN : DIST_STATES - 1;
}

/*
 * The highest two bits of a 32-bit match distance are encoded using six bits.
 * This six-bit value is called a distance slot. This way encoding a 32-bit
 * value takes 6-36 bits, larger values taking more bits.
 */
#define DIST_SLOT_BITS 6
#define DIST_SLOTS (1 << DIST_SLOT_BITS)

/* Match distances up to 127 are fully encoded using probabilities. Since
 * the highest two bits (distance slot) are always encoded using six bits,
 * the distances 0-3 don't need any additional bits to encode, since the
 * distance slot itself is the same as the actual distance. DIST_MODEL_START
 * indicates the first distance slot where at least one additional bit is
 * needed.
 */
#define DIST_MODEL_START 4

/*
 * Match distances greater than 127 are encoded in three pieces:
 *   - distance slot: the highest two bits
 *   - direct bits: 2-26 bits below the highest two bits
 *   - alignment bits: four lowest bits
 *
 * Direct bits don't use any probabilities.
 *
 * The distance slot value of 14 is for distances 128-191.
 */
#define DIST_MODEL_END 14

/* Distance slots that indicate a distance <= 127. */
#define FULL_DISTANCES_BITS (DIST_MODEL_END / 2)
#define FULL_DISTANCES (1 << FULL_DISTANCES_BITS)

/*
 * For match distances greater than 127, only the highest two bits and the
 * lowest four bits (alignment) is encoded using probabilities.
 */
#define ALIGN_BITS 4
#define ALIGN_SIZE (1 << ALIGN_BITS)
#define ALIGN_MASK (ALIGN_SIZE - 1)

/* Total number of all probability variables */
#define PROBS_TOTAL (1846 + LITERAL_CODERS_MAX * LITERAL_CODER_SIZE)

/*
 * LZMA remembers the four most recent match distances. Reusing these
 * distances tends to take less space than re-encoding the actual
 * distance value.
 */
#define REPS 4


// END xz_lzma2.h

/*
 * Range decoder initialization eats the first five bytes of each LZMA chunk.
 */
#define RC_INIT_BYTES 5

/*
 * Minimum number of usable input buffer to safely decode one LZMA symbol.
 * The worst case is that we decode 22 bits using probabilities and 26
 * direct bits. This may decode at maximum of 20 bytes of input. However,
 * lzma_main() does an extra normalization before returning, thus we
 * need to put 21 here.
 */
#define LZMA_IN_REQUIRED 21

/*
 * Dictionary (history buffer)
 *
 * These are always true:
 *    start <= pos <= full <= end
 *    pos <= limit <= end
 *    end == size
 *    size <= size_max
 *    allocated <= size
 *
 * Most of these variables are size_t as a relic of single-call mode,
 * in which the dictionary variables address the actual output
 * buffer directly.
 */
struct dictionary {
  // Beginning of the history buffer
  char *buf;
  // Old position in buf (before decoding more data)
  size_t start;
  // Position in buf
  size_t pos;
  // How full dictionary is. This is used to detect corrupt input that
  // would read beyond the beginning of the uncompressed stream.
  size_t full;
  /* Write limit; we don't write to buf[limit] or later bytes. */
  size_t limit;
  // End of the dictionary buffer. This is the same as the dictionary size.
  size_t end;
  // Size of the dictionary as specified in Block Header. This is used
  // together with "full" to detect corrupt input that would make us
  // read beyond the beginning of the uncompressed stream.
  unsigned size;
  // Maximum allowed dictionary size.
  unsigned size_max;
  // Amount of memory currently allocated for the dictionary.
  unsigned allocated;
};

/* Range decoder */
struct rc_dec {
  unsigned range;
  unsigned code;

  /*
   * Number of initializing bytes remaining to be read
   * by rc_read_init().
   */
  unsigned init_bytes_left;

  /*
   * Buffer from which we read our input. It can be either
   * temp.buf or the caller-provided input buffer.
   */
  const char *in;
  size_t in_pos;
  size_t in_limit;
};

/* Probabilities for a length decoder. */
struct lzma_len_dec {
  /* Probability of match length being at least 10 */
  uint16_t choice;

  /* Probability of match length being at least 18 */
  uint16_t choice2;

  /* Probabilities for match lengths 2-9 */
  uint16_t low[POS_STATES_MAX][LEN_LOW_SYMBOLS];

  /* Probabilities for match lengths 10-17 */
  uint16_t mid[POS_STATES_MAX][LEN_MID_SYMBOLS];

  /* Probabilities for match lengths 18-273 */
  uint16_t high[LEN_HIGH_SYMBOLS];
};

struct lzma_dec {
  /* Distances of latest four matches */
  unsigned rep0;
  unsigned rep1;
  unsigned rep2;
  unsigned rep3;

  /* Types of the most recently seen LZMA symbols */
  enum lzma_state state;

  /*
   * Length of a match. This is updated so that dict_repeat can
   * be called again to finish repeating the whole match.
   */
  unsigned len;

  /*
   * LZMA properties or related bit masks (number of literal
   * context bits, a mask dervied from the number of literal
   * position bits, and a mask dervied from the number
   * position bits)
   */
  unsigned lc;
  unsigned literal_pos_mask; /* (1 << lp) - 1 */
  unsigned pos_mask;         /* (1 << pb) - 1 */

  /* If 1, it's a match. Otherwise it's a single 8-bit literal. */
  uint16_t is_match[STATES][POS_STATES_MAX];

  /* If 1, it's a repeated match. The distance is one of rep0 .. rep3. */
  uint16_t is_rep[STATES];

  /*
   * If 0, distance of a repeated match is rep0.
   * Otherwise check is_rep1.
   */
  uint16_t is_rep0[STATES];

  /*
   * If 0, distance of a repeated match is rep1.
   * Otherwise check is_rep2.
   */
  uint16_t is_rep1[STATES];

  /* If 0, distance of a repeated match is rep2. Otherwise it is rep3. */
  uint16_t is_rep2[STATES];

  /*
   * If 1, the repeated match has length of one byte. Otherwise
   * the length is decoded from rep_len_decoder.
   */
  uint16_t is_rep0_long[STATES][POS_STATES_MAX];

  /*
   * Probability tree for the highest two bits of the match
   * distance. There is a separate probability tree for match
   * lengths of 2 (i.e. MATCH_LEN_MIN), 3, 4, and [5, 273].
   */
  uint16_t dist_slot[DIST_STATES][DIST_SLOTS];

  /*
   * Probility trees for additional bits for match distance
   * when the distance is in the range [4, 127].
   */
  uint16_t dist_special[FULL_DISTANCES - DIST_MODEL_END];

  /*
   * Probability tree for the lowest four bits of a match
   * distance that is equal to or greater than 128.
   */
  uint16_t dist_align[ALIGN_SIZE];

  /* Length of a normal match */
  struct lzma_len_dec match_len_dec;

  /* Length of a repeated match */
  struct lzma_len_dec rep_len_dec;

  /* Probabilities of literals */
  uint16_t literal[LITERAL_CODERS_MAX][LITERAL_CODER_SIZE];
};

struct lzma2_dec {
  /* Position in xz_dec_lzma2_run(). */
  enum lzma2_seq {
    SEQ_CONTROL,
    SEQ_UNCOMPRESSED_1,
    SEQ_UNCOMPRESSED_2,
    SEQ_COMPRESSED_0,
    SEQ_COMPRESSED_1,
    SEQ_PROPERTIES,
    SEQ_LZMA_PREPARE,
    SEQ_LZMA_RUN,
    SEQ_COPY
  } sequence;

  /* Next position after decoding the compressed size of the chunk. */
  enum lzma2_seq next_sequence;

  /* Uncompressed size of LZMA chunk (2 MiB at maximum) */
  unsigned uncompressed;

  /*
   * Compressed size of LZMA chunk or compressed/uncompressed
   * size of uncompressed chunk (64 KiB at maximum)
   */
  unsigned compressed;

  /*
   * True if dictionary reset is needed. This is false before
   * the first chunk (LZMA or uncompressed).
   */
  int need_dict_reset;

  /*
   * True if new LZMA properties are needed. This is false
   * before the first LZMA chunk.
   */
  int need_props;
};

struct xz_dec_lzma2 {
  /*
   * The order below is important on x86 to reduce code size and
   * it shouldn't hurt on other platforms. Everything up to and
   * including lzma.pos_mask are in the first 128 bytes on x86-32,
   * which allows using smaller instructions to access those
   * variables. On x86-64, fewer variables fit into the first 128
   * bytes, but this is still the best order without sacrificing
   * the readability by splitting the structures.
   */
  struct rc_dec rc;
  struct dictionary dict;
  struct lzma2_dec lzma2;
  struct lzma_dec lzma;

  /*
   * Temporary buffer which holds small number of input bytes between
   * decoder calls. See lzma2_lzma() for details.
   */
  struct {
    unsigned size;
    char buf[3 * LZMA_IN_REQUIRED];
  } temp;
};

/**************
 * Dictionary *
 **************/

/* Reset the dictionary state. */
static void dict_reset(struct dictionary *dict)
{
  dict->start = 0;
  dict->pos = 0;
  dict->limit = 0;
  dict->full = 0;
}

/* Set dictionary write limit */
static void dict_limit(struct dictionary *dict, size_t out_max)
{
  if (dict->end - dict->pos <= out_max)
    dict->limit = dict->end;
  else
    dict->limit = dict->pos + out_max;
}

/* Return true if at least one byte can be written into the dictionary. */
static int dict_has_space(const struct dictionary *dict)
{
  return dict->pos < dict->limit;
}

/*
 * Get a byte from the dictionary at the given distance. The distance is
 * assumed to valid, or as a special case, zero when the dictionary is
 * still empty. This special case is needed for single-call decoding to
 * avoid writing a '\0' to the end of the destination buffer.
 */
static unsigned dict_get(const struct dictionary *dict, unsigned dist)
{
  size_t offset = dict->pos - dist - 1;

  if (dist >= dict->pos)
    offset += dict->end;

  return dict->full > 0 ? dict->buf[offset] : 0;
}

/*
 * Put one byte into the dictionary. It is assumed that there is space for it.
 */
static void dict_put(struct dictionary *dict, char byte)
{
  dict->buf[dict->pos++] = byte;

  if (dict->full < dict->pos)
    dict->full = dict->pos;
}

/*
 * Repeat given number of bytes from the given distance. If the distance is
 * invalid, false is returned. On success, true is returned and *len is
 * updated to indicate how many bytes were left to be repeated.
 */
static int dict_repeat(struct dictionary *dict, unsigned *len, unsigned dist)
{
  size_t back;
  unsigned left;

  if (dist >= dict->full || dist >= dict->size) return 0;

  left = minof(dict->limit - dict->pos, *len);
  *len -= left;

  back = dict->pos - dist - 1;
  if (dist >= dict->pos)
    back += dict->end;

  do {
    dict->buf[dict->pos++] = dict->buf[back++];
    if (back == dict->end)
      back = 0;
  } while (--left > 0);

  if (dict->full < dict->pos)
    dict->full = dict->pos;

  return 1;
}

/* Copy uncompressed data as is from input to dictionary and output buffers. */
static void dict_uncompressed(struct dictionary *dict, struct xz_buf *b,
            unsigned *left)
{
  size_t copy_size;

  while (*left > 0 && b->in_pos < b->in_size
      && b->out_pos < b->out_size) {
    copy_size = minof(b->in_size - b->in_pos,
        b->out_size - b->out_pos);
    if (copy_size > dict->end - dict->pos)
      copy_size = dict->end - dict->pos;
    if (copy_size > *left)
      copy_size = *left;

    *left -= copy_size;

    memcpy(dict->buf + dict->pos, b->in + b->in_pos, copy_size);
    dict->pos += copy_size;

    if (dict->full < dict->pos)
      dict->full = dict->pos;

    if (dict->pos == dict->end)
      dict->pos = 0;

    memcpy(b->out + b->out_pos, b->in + b->in_pos,
        copy_size);

    dict->start = dict->pos;

    b->out_pos += copy_size;
    b->in_pos += copy_size;
  }
}

/*
 * Flush pending data from dictionary to b->out. It is assumed that there is
 * enough space in b->out. This is guaranteed because caller uses dict_limit()
 * before decoding data into the dictionary.
 */
static unsigned dict_flush(struct dictionary *dict, struct xz_buf *b)
{
  size_t copy_size = dict->pos - dict->start;

  if (dict->pos == dict->end)
    dict->pos = 0;

  memcpy(b->out + b->out_pos, dict->buf + dict->start,
      copy_size);

  dict->start = dict->pos;
  b->out_pos += copy_size;
  return copy_size;
}

/*****************
 * Range decoder *
 *****************/

/* Reset the range decoder. */
static void rc_reset(struct rc_dec *rc)
{
  rc->range = (unsigned)-1;
  rc->code = 0;
  rc->init_bytes_left = RC_INIT_BYTES;
}

/*
 * Read the first five initial bytes into rc->code if they haven't been
 * read already. (Yes, the first byte gets completely ignored.)
 */
static int rc_read_init(struct rc_dec *rc, struct xz_buf *b)
{
  while (rc->init_bytes_left > 0) {
    if (b->in_pos == b->in_size) return 0;

    rc->code = (rc->code << 8) + b->in[b->in_pos++];
    --rc->init_bytes_left;
  }

  return 1;
}

/* Return true if there may not be enough input for the next decoding loop. */
static int rc_limit_exceeded(const struct rc_dec *rc)
{
  return rc->in_pos > rc->in_limit;
}

/* Read the next input byte if needed. */
static void rc_normalize(struct rc_dec *rc)
{
  if (rc->range < RC_TOP_VALUE) {
    rc->range <<= RC_SHIFT_BITS;
    rc->code = (rc->code << RC_SHIFT_BITS) + rc->in[rc->in_pos++];
  }
}

/*
 * Decode one bit. In some versions, this function has been splitted in three
 * functions so that the compiler is supposed to be able to more easily avoid
 * an extra branch. In this particular version of the LZMA decoder, this
 * doesn't seem to be a good idea (tested with GCC 3.3.6, 3.4.6, and 4.3.3
 * on x86). Using a non-splitted version results in nicer looking code too.
 *
 * NOTE: This must return an int. Do not make it return a bool or the speed
 * of the code generated by GCC 3.x decreases 10-15 %. (GCC 4.3 doesn't care,
 * and it generates 10-20 % faster code than GCC 3.x from this file anyway.)
 */
static int rc_bit(struct rc_dec *rc, uint16_t *prob)
{
  unsigned bound;
  int bit;

  rc_normalize(rc);
  bound = (rc->range >> RC_BIT_MODEL_TOTAL_BITS) * *prob;
  if (rc->code < bound) {
    rc->range = bound;
    *prob += (RC_BIT_MODEL_TOTAL - *prob) >> RC_MOVE_BITS;
    bit = 0;
  } else {
    rc->range -= bound;
    rc->code -= bound;
    *prob -= *prob >> RC_MOVE_BITS;
    bit = 1;
  }

  return bit;
}

/* Decode a bittree starting from the most significant bit. */
static unsigned rc_bittree(struct rc_dec *rc,
             uint16_t *probs, unsigned limit)
{
  unsigned symbol = 1;

  do {
    if (rc_bit(rc, &probs[symbol]))
      symbol = (symbol << 1) + 1;
    else
      symbol <<= 1;
  } while (symbol < limit);

  return symbol;
}

/* Decode a bittree starting from the least significant bit. */
static void rc_bittree_reverse(struct rc_dec *rc,
                 uint16_t *probs,
                 unsigned *dest, unsigned limit)
{
  unsigned symbol = 1;
  unsigned i = 0;

  do {
    if (rc_bit(rc, &probs[symbol])) {
      symbol = (symbol << 1) + 1;
      *dest += 1 << i;
    } else {
      symbol <<= 1;
    }
  } while (++i < limit);
}

/* Decode direct bits (fixed fifty-fifty probability) */
static void rc_direct(struct rc_dec *rc, unsigned *dest, unsigned limit)
{
  unsigned mask;

  do {
    rc_normalize(rc);
    rc->range >>= 1;
    rc->code -= rc->range;
    mask = (unsigned)0 - (rc->code >> 31);
    rc->code += rc->range & mask;
    *dest = (*dest << 1) + (mask + 1);
  } while (--limit > 0);
}

/********
 * LZMA *
 ********/

/* Get pointer to literal coder probability array. */
static uint16_t *lzma_literal_probs(struct xz_dec_lzma2 *s)
{
  unsigned prev_byte = dict_get(&s->dict, 0);
  unsigned low = prev_byte >> (8 - s->lzma.lc);
  unsigned high = (s->dict.pos & s->lzma.literal_pos_mask) << s->lzma.lc;
  return s->lzma.literal[low + high];
}

/* Decode a literal (one 8-bit byte) */
static void lzma_literal(struct xz_dec_lzma2 *s)
{
  uint16_t *probs;
  unsigned symbol;
  unsigned match_byte;
  unsigned match_bit;
  unsigned offset;
  unsigned i;

  probs = lzma_literal_probs(s);

  if (s->lzma.state < LIT_STATES) {
    symbol = rc_bittree(&s->rc, probs, 0x100);
  } else {
    symbol = 1;
    match_byte = dict_get(&s->dict, s->lzma.rep0) << 1;
    offset = 0x100;

    do {
      match_bit = match_byte & offset;
      match_byte <<= 1;
      i = offset + match_bit + symbol;

      if (rc_bit(&s->rc, &probs[i])) {
        symbol = (symbol << 1) + 1;
        offset &= match_bit;
      } else {
        symbol <<= 1;
        offset &= ~match_bit;
      }
    } while (symbol < 0x100);
  }

  dict_put(&s->dict, (char)symbol);
  lzma_state_literal(&s->lzma.state);
}

/* Decode the length of the match into s->lzma.len. */
static void lzma_len(struct xz_dec_lzma2 *s, struct lzma_len_dec *l,
         unsigned pos_state)
{
  uint16_t *probs;
  unsigned limit;

  if (!rc_bit(&s->rc, &l->choice)) {
    probs = l->low[pos_state];
    limit = LEN_LOW_SYMBOLS;
    s->lzma.len = MATCH_LEN_MIN;
  } else {
    if (!rc_bit(&s->rc, &l->choice2)) {
      probs = l->mid[pos_state];
      limit = LEN_MID_SYMBOLS;
      s->lzma.len = MATCH_LEN_MIN + LEN_LOW_SYMBOLS;
    } else {
      probs = l->high;
      limit = LEN_HIGH_SYMBOLS;
      s->lzma.len = MATCH_LEN_MIN + LEN_LOW_SYMBOLS
          + LEN_MID_SYMBOLS;
    }
  }

  s->lzma.len += rc_bittree(&s->rc, probs, limit) - limit;
}

/* Decode a match. The distance will be stored in s->lzma.rep0. */
static void lzma_match(struct xz_dec_lzma2 *s, unsigned pos_state)
{
  uint16_t *probs;
  unsigned dist_slot;
  unsigned limit;

  lzma_state_match(&s->lzma.state);

  s->lzma.rep3 = s->lzma.rep2;
  s->lzma.rep2 = s->lzma.rep1;
  s->lzma.rep1 = s->lzma.rep0;

  lzma_len(s, &s->lzma.match_len_dec, pos_state);

  probs = s->lzma.dist_slot[lzma_get_dist_state(s->lzma.len)];
  dist_slot = rc_bittree(&s->rc, probs, DIST_SLOTS) - DIST_SLOTS;

  if (dist_slot < DIST_MODEL_START) {
    s->lzma.rep0 = dist_slot;
  } else {
    limit = (dist_slot >> 1) - 1;
    s->lzma.rep0 = 2 + (dist_slot & 1);

    if (dist_slot < DIST_MODEL_END) {
      s->lzma.rep0 <<= limit;
      probs = s->lzma.dist_special + s->lzma.rep0
          - dist_slot - 1;
      rc_bittree_reverse(&s->rc, probs,
          &s->lzma.rep0, limit);
    } else {
      rc_direct(&s->rc, &s->lzma.rep0, limit - ALIGN_BITS);
      s->lzma.rep0 <<= ALIGN_BITS;
      rc_bittree_reverse(&s->rc, s->lzma.dist_align,
          &s->lzma.rep0, ALIGN_BITS);
    }
  }
}

/*
 * Decode a repeated match. The distance is one of the four most recently
 * seen matches. The distance will be stored in s->lzma.rep0.
 */
static void lzma_rep_match(struct xz_dec_lzma2 *s, unsigned pos_state)
{
  unsigned tmp;

  if (!rc_bit(&s->rc, &s->lzma.is_rep0[s->lzma.state])) {
    if (!rc_bit(&s->rc, &s->lzma.is_rep0_long[
        s->lzma.state][pos_state])) {
      lzma_state_short_rep(&s->lzma.state);
      s->lzma.len = 1;
      return;
    }
  } else {
    if (!rc_bit(&s->rc, &s->lzma.is_rep1[s->lzma.state])) {
      tmp = s->lzma.rep1;
    } else {
      if (!rc_bit(&s->rc, &s->lzma.is_rep2[s->lzma.state])) {
        tmp = s->lzma.rep2;
      } else {
        tmp = s->lzma.rep3;
        s->lzma.rep3 = s->lzma.rep2;
      }

      s->lzma.rep2 = s->lzma.rep1;
    }

    s->lzma.rep1 = s->lzma.rep0;
    s->lzma.rep0 = tmp;
  }

  lzma_state_long_rep(&s->lzma.state);
  lzma_len(s, &s->lzma.rep_len_dec, pos_state);
}

/* LZMA decoder core */
static int lzma_main(struct xz_dec_lzma2 *s)
{
  unsigned pos_state;

  /*
   * If the dictionary was reached during the previous call, try to
   * finish the possibly pending repeat in the dictionary.
   */
  if (dict_has_space(&s->dict) && s->lzma.len > 0)
    dict_repeat(&s->dict, &s->lzma.len, s->lzma.rep0);

  /*
   * Decode more LZMA symbols. One iteration may consume up to
   * LZMA_IN_REQUIRED - 1 bytes.
   */
  while (dict_has_space(&s->dict) && !rc_limit_exceeded(&s->rc)) {
    pos_state = s->dict.pos & s->lzma.pos_mask;

    if (!rc_bit(&s->rc, &s->lzma.is_match[
        s->lzma.state][pos_state])) {
      lzma_literal(s);
    } else {
      if (rc_bit(&s->rc, &s->lzma.is_rep[s->lzma.state]))
        lzma_rep_match(s, pos_state);
      else
        lzma_match(s, pos_state);

      if (!dict_repeat(&s->dict, &s->lzma.len, s->lzma.rep0))
        return 0;
    }
  }

  /*
   * Having the range decoder always normalized when we are outside
   * this function makes it easier to correctly handle end of the chunk.
   */
  rc_normalize(&s->rc);

  return 1;
}

/*
 * Reset the LZMA decoder and range decoder state. Dictionary is nore reset
 * here, because LZMA state may be reset without resetting the dictionary.
 */
static void lzma_reset(struct xz_dec_lzma2 *s)
{
  uint16_t *probs;
  size_t i;

  s->lzma.state = STATE_LIT_LIT;
  s->lzma.rep0 = 0;
  s->lzma.rep1 = 0;
  s->lzma.rep2 = 0;
  s->lzma.rep3 = 0;

  /*
   * All probabilities are initialized to the same value. This hack
   * makes the code smaller by avoiding a separate loop for each
   * probability array.
   *
   * This could be optimized so that only that part of literal
   * probabilities that are actually required. In the common case
   * we would write 12 KiB less.
   */
  probs = s->lzma.is_match[0];
  for (i = 0; i < PROBS_TOTAL; ++i)
    probs[i] = RC_BIT_MODEL_TOTAL / 2;

  rc_reset(&s->rc);
}

/*
 * Decode and validate LZMA properties (lc/lp/pb) and calculate the bit masks
 * from the decoded lp and pb values. On success, the LZMA decoder state is
 * reset and true is returned.
 */
static int lzma_props(struct xz_dec_lzma2 *s, char props)
{
  if (props > (4 * 5 + 4) * 9 + 8)
    return 0;

  s->lzma.pos_mask = 0;
  while (props >= 9 * 5) {
    props -= 9 * 5;
    ++s->lzma.pos_mask;
  }

  s->lzma.pos_mask = (1 << s->lzma.pos_mask) - 1;

  s->lzma.literal_pos_mask = 0;
  while (props >= 9) {
    props -= 9;
    ++s->lzma.literal_pos_mask;
  }

  s->lzma.lc = props;

  if (s->lzma.lc + s->lzma.literal_pos_mask > 4)
    return 0;

  s->lzma.literal_pos_mask = (1 << s->lzma.literal_pos_mask) - 1;

  lzma_reset(s);

  return 1;
}

/*********
 * LZMA2 *
 *********/

/*
 * The LZMA decoder assumes that if the input limit (s->rc.in_limit) hasn't
 * been exceeded, it is safe to read up to LZMA_IN_REQUIRED bytes. This
 * wrapper function takes care of making the LZMA decoder's assumption safe.
 *
 * As long as there is plenty of input left to be decoded in the current LZMA
 * chunk, we decode directly from the caller-supplied input buffer until
 * there's LZMA_IN_REQUIRED bytes left. Those remaining bytes are copied into
 * s->temp.buf, which (hopefully) gets filled on the next call to this
 * function. We decode a few bytes from the temporary buffer so that we can
 * continue decoding from the caller-supplied input buffer again.
 */
static int lzma2_lzma(struct xz_dec_lzma2 *s, struct xz_buf *b)
{
  size_t in_avail;
  unsigned tmp;

  in_avail = b->in_size - b->in_pos;
  if (s->temp.size > 0 || !s->lzma2.compressed) {
    tmp = 2 * LZMA_IN_REQUIRED - s->temp.size;
    if (tmp > s->lzma2.compressed - s->temp.size)
      tmp = s->lzma2.compressed - s->temp.size;
    if (tmp > in_avail)
      tmp = in_avail;

    memcpy(s->temp.buf + s->temp.size, b->in + b->in_pos, tmp);

    if (s->temp.size + tmp == s->lzma2.compressed) {
      memset(s->temp.buf + s->temp.size + tmp, 0,
          sizeof(s->temp.buf)
            - s->temp.size - tmp);
      s->rc.in_limit = s->temp.size + tmp;
    } else if (s->temp.size + tmp < LZMA_IN_REQUIRED) {
      s->temp.size += tmp;
      b->in_pos += tmp;
      return 1;
    } else {
      s->rc.in_limit = s->temp.size + tmp - LZMA_IN_REQUIRED;
    }

    s->rc.in = s->temp.buf;
    s->rc.in_pos = 0;

    if (!lzma_main(s) || s->rc.in_pos > s->temp.size + tmp)
      return 0;

    s->lzma2.compressed -= s->rc.in_pos;

    if (s->rc.in_pos < s->temp.size) {
      s->temp.size -= s->rc.in_pos;
      memmove(s->temp.buf, s->temp.buf + s->rc.in_pos,
          s->temp.size);
      return 1;
    }

    b->in_pos += s->rc.in_pos - s->temp.size;
    s->temp.size = 0;
  }

  in_avail = b->in_size - b->in_pos;
  if (in_avail >= LZMA_IN_REQUIRED) {
    s->rc.in = b->in;
    s->rc.in_pos = b->in_pos;

    if (in_avail >= s->lzma2.compressed + LZMA_IN_REQUIRED)
      s->rc.in_limit = b->in_pos + s->lzma2.compressed;
    else
      s->rc.in_limit = b->in_size - LZMA_IN_REQUIRED;

    if (!lzma_main(s))
      return 0;

    in_avail = s->rc.in_pos - b->in_pos;
    if (in_avail > s->lzma2.compressed) return 0;

    s->lzma2.compressed -= in_avail;
    b->in_pos = s->rc.in_pos;
  }

  in_avail = b->in_size - b->in_pos;
  if (in_avail < LZMA_IN_REQUIRED) {
    if (in_avail > s->lzma2.compressed)
      in_avail = s->lzma2.compressed;

    memcpy(s->temp.buf, b->in + b->in_pos, in_avail);
    s->temp.size = in_avail;
    b->in_pos += in_avail;
  }

  return 1;
}

/*
 * Take care of the LZMA2 control layer, and forward the job of actual LZMA
 * decoding or copying of uncompressed chunks to other functions.
 */
enum xz_ret xz_dec_lzma2_run(struct xz_dec_lzma2 *s, struct xz_buf *b)
{
  unsigned tmp;

  while (b->in_pos < b->in_size || s->lzma2.sequence == SEQ_LZMA_RUN) {
    switch (s->lzma2.sequence) {
    case SEQ_CONTROL:
      /*
       * LZMA2 control byte
       *
       * Exact values:
       *   0x00   End marker
       *   0x01   Dictionary reset followed by
       *          an uncompressed chunk
       *   0x02   Uncompressed chunk (no dictionary reset)
       *
       * Highest three bits (s->control & 0xE0):
       *   0xE0   Dictionary reset, new properties and state
       *          reset, followed by LZMA compressed chunk
       *   0xC0   New properties and state reset, followed
       *          by LZMA compressed chunk (no dictionary
       *          reset)
       *   0xA0   State reset using old properties,
       *          followed by LZMA compressed chunk (no
       *          dictionary reset)
       *   0x80   LZMA chunk (no dictionary or state reset)
       *
       * For LZMA compressed chunks, the lowest five bits
       * (s->control & 1F) are the highest bits of the
       * uncompressed size (bits 16-20).
       *
       * A new LZMA2 stream must begin with a dictionary
       * reset. The first LZMA chunk must set new
       * properties and reset the LZMA state.
       *
       * Values that don't match anything described above
       * are invalid and we return XZ_DATA_ERROR.
       */


      if (!(tmp = b->in[b->in_pos++]))
        return XZ_STREAM_END;

      if (tmp >= 0xE0 || tmp == 1) {
        s->lzma2.need_props = 1;
        s->lzma2.need_dict_reset = 0;
        dict_reset(&s->dict);
      } else if (s->lzma2.need_dict_reset) {
        return XZ_DATA_ERROR;
      }

      if (tmp >= 0x80) {
        s->lzma2.uncompressed = (tmp & 0x1F) << 16;
        s->lzma2.sequence = SEQ_UNCOMPRESSED_1;

        if (tmp >= 0xC0) {
          /*
           * When there are new properties,
           * state reset is done at
           * SEQ_PROPERTIES.
           */
          s->lzma2.need_props = 0;
          s->lzma2.next_sequence
              = SEQ_PROPERTIES;

        } else if (s->lzma2.need_props) {
          return XZ_DATA_ERROR;

        } else {
          s->lzma2.next_sequence
              = SEQ_LZMA_PREPARE;
          if (tmp >= 0xA0)
            lzma_reset(s);
        }
      } else {
        if (tmp > 2)
          return XZ_DATA_ERROR;

        s->lzma2.sequence = SEQ_COMPRESSED_0;
        s->lzma2.next_sequence = SEQ_COPY;
      }

      break;

    case SEQ_UNCOMPRESSED_1:
      s->lzma2.uncompressed
          += (unsigned)b->in[b->in_pos++] << 8;
      s->lzma2.sequence = SEQ_UNCOMPRESSED_2;
      break;

    case SEQ_UNCOMPRESSED_2:
      s->lzma2.uncompressed
          += (unsigned)b->in[b->in_pos++] + 1;
      s->lzma2.sequence = SEQ_COMPRESSED_0;
      break;

    case SEQ_COMPRESSED_0:
      s->lzma2.compressed
          = (unsigned)b->in[b->in_pos++] << 8;
      s->lzma2.sequence = SEQ_COMPRESSED_1;
      break;

    case SEQ_COMPRESSED_1:
      s->lzma2.compressed
          += (unsigned)b->in[b->in_pos++] + 1;
      s->lzma2.sequence = s->lzma2.next_sequence;
      break;

    case SEQ_PROPERTIES:
      if (!lzma_props(s, b->in[b->in_pos++]))
        return XZ_DATA_ERROR;

      s->lzma2.sequence = SEQ_LZMA_PREPARE;

    case SEQ_LZMA_PREPARE:
      if (s->lzma2.compressed < RC_INIT_BYTES)
        return XZ_DATA_ERROR;

      if (!rc_read_init(&s->rc, b))
        return XZ_OK;

      s->lzma2.compressed -= RC_INIT_BYTES;
      s->lzma2.sequence = SEQ_LZMA_RUN;

    case SEQ_LZMA_RUN:
      /*
       * Set dictionary limit to indicate how much we want
       * to be encoded at maximum. Decode new data into the
       * dictionary. Flush the new data from dictionary to
       * b->out. Check if we finished decoding this chunk.
       * In case the dictionary got full but we didn't fill
       * the output buffer yet, we may run this loop
       * multiple times without changing s->lzma2.sequence.
       */
      dict_limit(&s->dict, minof(b->out_size - b->out_pos,
          s->lzma2.uncompressed));
      if (!lzma2_lzma(s, b))
        return XZ_DATA_ERROR;

      s->lzma2.uncompressed -= dict_flush(&s->dict, b);

      if (!s->lzma2.uncompressed) {
        if (s->lzma2.compressed > 0 || s->lzma.len > 0
            || s->rc.code)
          return XZ_DATA_ERROR;

        rc_reset(&s->rc);
        s->lzma2.sequence = SEQ_CONTROL;

      } else if (b->out_pos == b->out_size
          || (b->in_pos == b->in_size
            && s->temp.size
            < s->lzma2.compressed)) {
        return XZ_OK;
      }

      break;

    case SEQ_COPY:
      dict_uncompressed(&s->dict, b, &s->lzma2.compressed);
      if (s->lzma2.compressed > 0)
        return XZ_OK;

      s->lzma2.sequence = SEQ_CONTROL;
      break;
    }
  }

  return XZ_OK;
}

struct xz_dec_lzma2 *xz_dec_lzma2_create(unsigned dict_max)
{
  struct xz_dec_lzma2 *s = malloc(sizeof(*s));
  if (!s)
    return NULL;

  s->dict.size_max = dict_max;
  s->dict.buf = NULL;
  s->dict.allocated = 0;

  return s;
}

enum xz_ret xz_dec_lzma2_reset(struct xz_dec_lzma2 *s, char props)
{
  /* This limits dictionary size to 3 GiB to keep parsing simpler. */
  if (props > 39)
    return XZ_OPTIONS_ERROR;

  s->dict.size = 2 + (props & 1);
  s->dict.size <<= (props >> 1) + 11;

  if (s->dict.size > s->dict.size_max)
    return XZ_MEMLIMIT_ERROR;

  s->dict.end = s->dict.size;

  if (s->dict.allocated < s->dict.size) {
    free(s->dict.buf);
    s->dict.buf = malloc(s->dict.size);
    if (s->dict.buf == NULL) {
      s->dict.allocated = 0;
      return XZ_MEM_ERROR;
    }
  }

  s->lzma.len = 0;

  s->lzma2.sequence = SEQ_CONTROL;
  s->lzma2.need_dict_reset = 1;

  s->temp.size = 0;

  return XZ_OK;
}

/*
 * .xz Stream decoder
 */


// BEGIN xz_stream.h
/*
 * Definitions for handling the .xz file format
 */


#define STREAM_HEADER_SIZE 12

#define HEADER_MAGIC "\3757zXZ"
#define HEADER_MAGIC_SIZE 6

#define FOOTER_MAGIC "YZ"
#define FOOTER_MAGIC_SIZE 2

#define VLI_MAX ((uint64_t)-1 / 2)
#define VLI_UNKNOWN (-1ULL)

/* Maximum encoded size of a VLI */
#define VLI_BYTES_MAX (sizeof(uint64_t) * 8 / 7)

/* Integrity Check types */
enum xz_check {
  XZ_CHECK_NONE = 0,
  XZ_CHECK_CRC32 = 1,
  XZ_CHECK_CRC64 = 4,
  XZ_CHECK_SHA256 = 10
};

/* Maximum possible Check ID */
#define XZ_CHECK_MAX 15
// END xz_stream.h

/* Hash used to validate the Index field */
struct xz_dec_hash {
  uint64_t unpadded;
  uint64_t uncompressed;
  unsigned crc32;
};

struct xz_dec {
  /* Position in dec_main() */
  enum {
    SEQ_STREAM_HEADER,
    SEQ_BLOCK_START,
    SEQ_BLOCK_HEADER,
    SEQ_BLOCK_UNCOMPRESS,
    SEQ_BLOCK_PADDING,
    SEQ_BLOCK_CHECK,
    SEQ_INDEX,
    SEQ_INDEX_PADDING,
    SEQ_INDEX_CRC32,
    SEQ_STREAM_FOOTER
  } sequence;

  /* Position in variable-length integers and Check fields */
  unsigned pos;

  /* Variable-length integer decoded by dec_vli() */
  uint64_t vli;

  /* Saved in_pos and out_pos */
  size_t in_start;
  size_t out_start;

  /* CRC32 or CRC64 value in Block or CRC32 value in Index */
  uint64_t crc;

  /* Type of the integrity check calculated from uncompressed data */
  enum xz_check check_type;

  /*
   * True if the next call to xz_dec_run() is allowed to return
   * XZ_BUF_ERROR.
   */
  int allow_buf_error;

  /* Information stored in Block Header */
  struct {
    /* Value stored in the Compressed Size field, or
     * VLI_UNKNOWN if Compressed Size is not present. */
    uint64_t compressed;

    /* Value stored in the Uncompressed Size field, or
     * VLI_UNKNOWN if Uncompressed Size is not present. */
    uint64_t uncompressed;

    /* Size of the Block Header field */
    unsigned size;
  } block_header;

  /* Information collected when decoding Blocks */
  struct {
    /* Observed compressed size of the current Block */
    uint64_t compressed;

    /* Observed uncompressed size of the current Block */
    uint64_t uncompressed;

    uint64_t count; /* Number of Blocks decoded so far */

    // Hash calculated from the Block sizes. This is used to
    // validate the Index field.
    struct xz_dec_hash hash;
  } block;

  /* Variables needed when verifying the Index field */
  struct {
    /* Position in dec_index() */
    enum {
      SEQ_INDEX_COUNT,
      SEQ_INDEX_UNPADDED,
      SEQ_INDEX_UNCOMPRESSED
    } sequence;

    // Size of the Index in bytes
    uint64_t size;

    // Number of Records (matches block.count in valid files)
    uint64_t count;

    // Hash calculated from the Records (matches block.hash in
    // valid files).
    struct xz_dec_hash hash;
  } index;

  /*
   * Temporary buffer needed to hold Stream Header, Block Header,
   * and Stream Footer. The Block Header is the biggest (1 KiB)
   * so we reserve space according to that. buf[] has to be aligned
   * to a multiple of four bytes; the size_t variables before it
   * should guarantee this.
   */
  struct {
    size_t pos;
    size_t size;
    char buf[1024];
  } temp;

  struct xz_dec_lzma2 *lzma2;

  struct xz_dec_bcj *bcj;
  int bcj_active;
};

/* Sizes of the Check field with different Check IDs */
static const char check_sizes[16] = {
  0,
  4, 4, 4,
  8, 8, 8,
  16, 16, 16,
  32, 32, 32,
  64, 64, 64
};

/*
 * Fill s->temp by copying data starting from b->in[b->in_pos]. Caller
 * must have set s->temp.pos to indicate how much data we are supposed
 * to copy into s->temp.buf. Return true once s->temp.pos has reached
 * s->temp.size.
 */
static int fill_temp(struct xz_dec *s, struct xz_buf *b)
{
  size_t copy_size = minof(b->in_size - b->in_pos, s->temp.size - s->temp.pos);

  memcpy(s->temp.buf + s->temp.pos, b->in + b->in_pos, copy_size);
  b->in_pos += copy_size;
  s->temp.pos += copy_size;

  if (s->temp.pos == s->temp.size) {
    s->temp.pos = 0;
    return 1;
  }

  return 0;
}

/* Decode a variable-length integer (little-endian base-128 encoding) */
static enum xz_ret dec_vli(struct xz_dec *s, const char *in,
         size_t *in_pos, size_t in_size)
{
  char byte;

  if (!s->pos)
    s->vli = 0;

  while (*in_pos < in_size) {
    byte = in[*in_pos];
    ++*in_pos;

    s->vli |= (uint64_t)(byte & 0x7F) << s->pos;

    if (!(byte & 0x80)) {
      // Don't allow non-minimal encodings.
      if (!byte && s->pos)
        return XZ_DATA_ERROR;

      s->pos = 0;
      return XZ_STREAM_END;
    }

    s->pos += 7;
    if (s->pos == 7 * VLI_BYTES_MAX)
      return XZ_DATA_ERROR;
  }

  return XZ_OK;
}

/*
 * Decode the Compressed Data field from a Block. Update and validate
 * the observed compressed and uncompressed sizes of the Block so that
 * they don't exceed the values possibly stored in the Block Header
 * (validation assumes that no integer overflow occurs, since uint64_t
 * is normally uint64_t). Update the CRC32 or CRC64 value if presence of
 * the CRC32 or CRC64 field was indicated in Stream Header.
 *
 * Once the decoding is finished, validate that the observed sizes match
 * the sizes possibly stored in the Block Header. Update the hash and
 * Block count, which are later used to validate the Index field.
 */
static enum xz_ret dec_block(struct xz_dec *s, struct xz_buf *b)
{
  enum xz_ret ret;

  s->in_start = b->in_pos;
  s->out_start = b->out_pos;

  if (s->bcj_active)
    ret = xz_dec_bcj_run(s->bcj, s->lzma2, b);
  else
    ret = xz_dec_lzma2_run(s->lzma2, b);

  s->block.compressed += b->in_pos - s->in_start;
  s->block.uncompressed += b->out_pos - s->out_start;

  /*
   * There is no need to separately check for VLI_UNKNOWN, since
   * the observed sizes are always smaller than VLI_UNKNOWN.
   */
  if (s->block.compressed > s->block_header.compressed
      || s->block.uncompressed
        > s->block_header.uncompressed)
    return XZ_DATA_ERROR;

  if (s->check_type == XZ_CHECK_CRC32)
    s->crc = xz_crc32(b->out + s->out_start,
        b->out_pos - s->out_start, s->crc);
  else if (s->check_type == XZ_CHECK_CRC64) {
    s->crc = ~(s->crc);
    size_t size = b->out_pos - s->out_start;
    char *buf = b->out + s->out_start;
    while (size) {
      s->crc = xz_crc64_table[*buf++ ^ (s->crc & 0xFF)] ^ (s->crc >> 8);
      --size;
    }
    s->crc=~(s->crc);
  }

  if (ret == XZ_STREAM_END) {
    if (s->block_header.compressed != VLI_UNKNOWN
        && s->block_header.compressed
          != s->block.compressed)
      return XZ_DATA_ERROR;

    if (s->block_header.uncompressed != VLI_UNKNOWN
        && s->block_header.uncompressed
          != s->block.uncompressed)
      return XZ_DATA_ERROR;

    s->block.hash.unpadded += s->block_header.size
        + s->block.compressed;

    s->block.hash.unpadded += check_sizes[s->check_type];

    s->block.hash.uncompressed += s->block.uncompressed;
    s->block.hash.crc32 = xz_crc32(
        (const char *)&s->block.hash,
        sizeof(s->block.hash), s->block.hash.crc32);

    ++s->block.count;
  }

  return ret;
}

/* Update the Index size and the CRC32 value. */
static void index_update(struct xz_dec *s, const struct xz_buf *b)
{
  size_t in_used = b->in_pos - s->in_start;
  s->index.size += in_used;
  s->crc = xz_crc32(b->in + s->in_start, in_used, s->crc);
}

/*
 * Decode the Number of Records, Unpadded Size, and Uncompressed Size
 * fields from the Index field. That is, Index Padding and CRC32 are not
 * decoded by this function.
 *
 * This can return XZ_OK (more input needed), XZ_STREAM_END (everything
 * successfully decoded), or XZ_DATA_ERROR (input is corrupt).
 */
static enum xz_ret dec_index(struct xz_dec *s, struct xz_buf *b)
{
  enum xz_ret ret;

  do {
    ret = dec_vli(s, b->in, &b->in_pos, b->in_size);
    if (ret != XZ_STREAM_END) {
      index_update(s, b);
      return ret;
    }

    switch (s->index.sequence) {
    case SEQ_INDEX_COUNT:
      s->index.count = s->vli;

      /*
       * Validate that the Number of Records field
       * indicates the same number of Records as
       * there were Blocks in the Stream.
       */
      if (s->index.count != s->block.count)
        return XZ_DATA_ERROR;

      s->index.sequence = SEQ_INDEX_UNPADDED;
      break;

    case SEQ_INDEX_UNPADDED:
      s->index.hash.unpadded += s->vli;
      s->index.sequence = SEQ_INDEX_UNCOMPRESSED;
      break;

    case SEQ_INDEX_UNCOMPRESSED:
      s->index.hash.uncompressed += s->vli;
      s->index.hash.crc32 = xz_crc32(
          (const char *)&s->index.hash,
          sizeof(s->index.hash),
          s->index.hash.crc32);
      --s->index.count;
      s->index.sequence = SEQ_INDEX_UNPADDED;
      break;
    }
  } while (s->index.count > 0);

  return XZ_STREAM_END;
}

/*
 * Validate that the next four or eight input bytes match the value
 * of s->crc. s->pos must be zero when starting to validate the first byte.
 * The "bits" argument allows using the same code for both CRC32 and CRC64.
 */
static enum xz_ret crc_validate(struct xz_dec *s, struct xz_buf *b,
        unsigned bits)
{
  do {
    if (b->in_pos == b->in_size)
      return XZ_OK;

    if (((s->crc >> s->pos) & 0xFF) != b->in[b->in_pos++])
      return XZ_DATA_ERROR;

    s->pos += 8;

  } while (s->pos < bits);

  s->crc = 0;
  s->pos = 0;

  return XZ_STREAM_END;
}

/*
 * Skip over the Check field when the Check ID is not supported.
 * Returns true once the whole Check field has been skipped over.
 */
static int check_skip(struct xz_dec *s, struct xz_buf *b)
{
  while (s->pos < check_sizes[s->check_type]) {
    if (b->in_pos == b->in_size) return 0;

    ++b->in_pos;
    ++s->pos;
  }

  s->pos = 0;

  return 1;
}

/* Decode the Stream Header field (the first 12 bytes of the .xz Stream). */
static enum xz_ret dec_stream_header(struct xz_dec *s)
{
  if (!memeq(s->temp.buf, HEADER_MAGIC, HEADER_MAGIC_SIZE))
    return XZ_FORMAT_ERROR;

  if (xz_crc32(s->temp.buf + HEADER_MAGIC_SIZE, 2, 0)
      != get_unaligned_le32(s->temp.buf + HEADER_MAGIC_SIZE + 2))
    return XZ_DATA_ERROR;

  if (s->temp.buf[HEADER_MAGIC_SIZE])
    return XZ_OPTIONS_ERROR;

  /*
   * Of integrity checks, we support none (Check ID = 0),
   * CRC32 (Check ID = 1), and optionally CRC64 (Check ID = 4).
   * However, if XZ_DEC_ANY_CHECK is defined, we will accept other
   * check types too, but then the check won't be verified and
   * a warning (XZ_UNSUPPORTED_CHECK) will be given.
   */
  s->check_type = s->temp.buf[HEADER_MAGIC_SIZE + 1];

  if (s->check_type > XZ_CHECK_MAX)
    return XZ_OPTIONS_ERROR;

  if (s->check_type > XZ_CHECK_CRC32 && s->check_type != XZ_CHECK_CRC64)
    return XZ_UNSUPPORTED_CHECK;

  return XZ_OK;
}

/* Decode the Stream Footer field (the last 12 bytes of the .xz Stream) */
static enum xz_ret dec_stream_footer(struct xz_dec *s)
{
  if (!memeq(s->temp.buf + 10, FOOTER_MAGIC, FOOTER_MAGIC_SIZE))
    return XZ_DATA_ERROR;

  if (xz_crc32(s->temp.buf + 4, 6, 0) != get_unaligned_le32(s->temp.buf))
    return XZ_DATA_ERROR;

  /*
   * Validate Backward Size. Note that we never added the size of the
   * Index CRC32 field to s->index.size, thus we use s->index.size / 4
   * instead of s->index.size / 4 - 1.
   */
  if ((s->index.size >> 2) != get_unaligned_le32(s->temp.buf + 4))
    return XZ_DATA_ERROR;

  if (s->temp.buf[8] || s->temp.buf[9] != s->check_type)
    return XZ_DATA_ERROR;

  /*
   * Use XZ_STREAM_END instead of XZ_OK to be more convenient
   * for the caller.
   */
  return XZ_STREAM_END;
}

/* Decode the Block Header and initialize the filter chain. */
static enum xz_ret dec_block_header(struct xz_dec *s)
{
  enum xz_ret ret;

  /*
   * Validate the CRC32. We know that the temp buffer is at least
   * eight bytes so this is safe.
   */
  s->temp.size -= 4;
  if (xz_crc32(s->temp.buf, s->temp.size, 0)
      != get_unaligned_le32(s->temp.buf + s->temp.size))
    return XZ_DATA_ERROR;

  s->temp.pos = 2;

  /*
   * Catch unsupported Block Flags. We support only one or two filters
   * in the chain, so we catch that with the same test.
   */
  if (s->temp.buf[1] & 0x3E)
    return XZ_OPTIONS_ERROR;

  /* Compressed Size */
  if (s->temp.buf[1] & 0x40) {
    if (dec_vli(s, s->temp.buf, &s->temp.pos, s->temp.size)
          != XZ_STREAM_END)
      return XZ_DATA_ERROR;

    s->block_header.compressed = s->vli;
  } else {
    s->block_header.compressed = VLI_UNKNOWN;
  }

  /* Uncompressed Size */
  if (s->temp.buf[1] & 0x80) {
    if (dec_vli(s, s->temp.buf, &s->temp.pos, s->temp.size)
        != XZ_STREAM_END)
      return XZ_DATA_ERROR;

    s->block_header.uncompressed = s->vli;
  } else {
    s->block_header.uncompressed = VLI_UNKNOWN;
  }

  /* If there are two filters, the first one must be a BCJ filter. */
  s->bcj_active = s->temp.buf[1] & 0x01;
  if (s->bcj_active) {
    if (s->temp.size - s->temp.pos < 2)
      return XZ_OPTIONS_ERROR;

    ret = xz_dec_bcj_reset(s->bcj, s->temp.buf[s->temp.pos++]);
    if (ret != XZ_OK)
      return ret;

    /*
     * We don't support custom start offset,
     * so Size of Properties must be zero.
     */
    if (s->temp.buf[s->temp.pos++])
      return XZ_OPTIONS_ERROR;
  }

  /* Valid Filter Flags always take at least two bytes. */
  if (s->temp.size - s->temp.pos < 2)
    return XZ_DATA_ERROR;

  /* Filter ID = LZMA2 */
  if (s->temp.buf[s->temp.pos++] != 0x21)
    return XZ_OPTIONS_ERROR;

  /* Size of Properties = 1-byte Filter Properties */
  if (s->temp.buf[s->temp.pos++] != 1)
    return XZ_OPTIONS_ERROR;

  /* Filter Properties contains LZMA2 dictionary size. */
  if (s->temp.size - s->temp.pos < 1)
    return XZ_DATA_ERROR;

  ret = xz_dec_lzma2_reset(s->lzma2, s->temp.buf[s->temp.pos++]);
  if (ret != XZ_OK)
    return ret;

  /* The rest must be Header Padding. */
  while (s->temp.pos < s->temp.size)
    if (s->temp.buf[s->temp.pos++])
      return XZ_OPTIONS_ERROR;

  s->temp.pos = 0;
  s->block.compressed = 0;
  s->block.uncompressed = 0;

  return XZ_OK;
}

static enum xz_ret dec_main(struct xz_dec *s, struct xz_buf *b)
{
  enum xz_ret ret;

  /*
   * Store the start position for the case when we are in the middle
   * of the Index field.
   */
  s->in_start = b->in_pos;

  for (;;) {
    switch (s->sequence) {
    case SEQ_STREAM_HEADER:
      /*
       * Stream Header is copied to s->temp, and then
       * decoded from there. This way if the caller
       * gives us only little input at a time, we can
       * still keep the Stream Header decoding code
       * simple. Similar approach is used in many places
       * in this file.
       */
      if (!fill_temp(s, b))
        return XZ_OK;

      /*
       * If dec_stream_header() returns
       * XZ_UNSUPPORTED_CHECK, it is still possible
       * to continue decoding if working in multi-call
       * mode. Thus, update s->sequence before calling
       * dec_stream_header().
       */
      s->sequence = SEQ_BLOCK_START;

      ret = dec_stream_header(s);
      if (ret != XZ_OK)
        return ret;

    case SEQ_BLOCK_START:
      /* We need one byte of input to continue. */
      if (b->in_pos == b->in_size)
        return XZ_OK;

      /* See if this is the beginning of the Index field. */
      if (!b->in[b->in_pos]) {
        s->in_start = b->in_pos++;
        s->sequence = SEQ_INDEX;
        break;
      }

      /*
       * Calculate the size of the Block Header and
       * prepare to decode it.
       */
      s->block_header.size
        = ((unsigned)b->in[b->in_pos] + 1) * 4;

      s->temp.size = s->block_header.size;
      s->temp.pos = 0;
      s->sequence = SEQ_BLOCK_HEADER;

    case SEQ_BLOCK_HEADER:
      if (!fill_temp(s, b))
        return XZ_OK;

      ret = dec_block_header(s);
      if (ret != XZ_OK)
        return ret;

      s->sequence = SEQ_BLOCK_UNCOMPRESS;

    case SEQ_BLOCK_UNCOMPRESS:
      ret = dec_block(s, b);
      if (ret != XZ_STREAM_END)
        return ret;

      s->sequence = SEQ_BLOCK_PADDING;

    case SEQ_BLOCK_PADDING:
      /*
       * Size of Compressed Data + Block Padding
       * must be a multiple of four. We don't need
       * s->block.compressed for anything else
       * anymore, so we use it here to test the size
       * of the Block Padding field.
       */
      while (s->block.compressed & 3) {
        if (b->in_pos == b->in_size)
          return XZ_OK;

        if (b->in[b->in_pos++])
          return XZ_DATA_ERROR;

        ++s->block.compressed;
      }

      s->sequence = SEQ_BLOCK_CHECK;

    case SEQ_BLOCK_CHECK:
      if (s->check_type == XZ_CHECK_CRC32) {
        ret = crc_validate(s, b, 32);
        if (ret != XZ_STREAM_END)
          return ret;
      }
      else if (s->check_type == XZ_CHECK_CRC64) {
        ret = crc_validate(s, b, 64);
        if (ret != XZ_STREAM_END)
          return ret;
      }
      else if (!check_skip(s, b)) {
        return XZ_OK;
      }

      s->sequence = SEQ_BLOCK_START;
      break;

    case SEQ_INDEX:
      ret = dec_index(s, b);
      if (ret != XZ_STREAM_END)
        return ret;

      s->sequence = SEQ_INDEX_PADDING;

    case SEQ_INDEX_PADDING:
      while ((s->index.size + (b->in_pos - s->in_start))
          & 3) {
        if (b->in_pos == b->in_size) {
          index_update(s, b);
          return XZ_OK;
        }

        if (b->in[b->in_pos++])
          return XZ_DATA_ERROR;
      }

      /* Finish the CRC32 value and Index size. */
      index_update(s, b);

      /* Compare the hashes to validate the Index field. */
      if (!memeq(&s->block.hash, &s->index.hash,
          sizeof(s->block.hash)))
        return XZ_DATA_ERROR;

      s->sequence = SEQ_INDEX_CRC32;

    case SEQ_INDEX_CRC32:
      ret = crc_validate(s, b, 32);
      if (ret != XZ_STREAM_END)
        return ret;

      s->temp.size = STREAM_HEADER_SIZE;
      s->sequence = SEQ_STREAM_FOOTER;

    case SEQ_STREAM_FOOTER:
      if (!fill_temp(s, b))
        return XZ_OK;

      return dec_stream_footer(s);
    }
  }

  /* Never reached */
}

/*
 * xz_dec_run() - Run the XZ decoder
 * @s:          Decoder state allocated using xz_dec_init()
 * @b:          Input and output buffers
 *
 * The possible return values depend on build options and operation mode.
 * See enum xz_ret for details.
 *
 * Note that if an error occurs in single-call mode (return value is not
 * XZ_STREAM_END), b->in_pos and b->out_pos are not modified and the
 * contents of the output buffer from b->out[b->out_pos] onward are
 * undefined. This is true even after XZ_BUF_ERROR, because with some filter
 * chains, there may be a second pass over the output buffer, and this pass
 * cannot be properly done if the output buffer is truncated. Thus, you
 * cannot give the single-call decoder a too small buffer and then expect to
 * get that amount valid data from the beginning of the stream. You must use
 * the multi-call decoder if you don't want to uncompress the whole stream.
 *
 * xz_dec_run() is a wrapper for dec_main() to handle some special cases in
 * multi-call and single-call decoding.
 *
 * In multi-call mode, we must return XZ_BUF_ERROR when it seems clear that we
 * are not going to make any progress anymore. This is to prevent the caller
 * from calling us infinitely when the input file is truncated or otherwise
 * corrupt. Since zlib-style API allows that the caller fills the input buffer
 * only when the decoder doesn't produce any new output, we have to be careful
 * to avoid returning XZ_BUF_ERROR too easily: XZ_BUF_ERROR is returned only
 * after the second consecutive call to xz_dec_run() that makes no progress.
 *
 * In single-call mode, if we couldn't decode everything and no error
 * occurred, either the input is truncated or the output buffer is too small.
 * Since we know that the last input byte never produces any output, we know
 * that if all the input was consumed and decoding wasn't finished, the file
 * must be corrupt. Otherwise the output buffer has to be too small or the
 * file is corrupt in a way that decoding it produces too big output.
 *
 * If single-call decoding fails, we reset b->in_pos and b->out_pos back to
 * their original values. This is because with some filter chains there won't
 * be any valid uncompressed data in the output buffer unless the decoding
 * actually succeeds (that's the price to pay of using the output buffer as
 * the workspace).
 */
enum xz_ret xz_dec_run(struct xz_dec *s, struct xz_buf *b)
{
  size_t in_start;
  size_t out_start;
  enum xz_ret ret;

  in_start = b->in_pos;
  out_start = b->out_pos;
  ret = dec_main(s, b);

  if (ret == XZ_OK && in_start == b->in_pos && out_start == b->out_pos) {
    if (s->allow_buf_error)
      ret = XZ_BUF_ERROR;

    s->allow_buf_error = 1;
  } else {
    s->allow_buf_error = 0;
  }

  return ret;
}

/**
 * xz_dec_reset() - Reset an already allocated decoder state
 * @s:          Decoder state allocated using xz_dec_init()
 *
 * This function can be used to reset the multi-call decoder state without
 * freeing and reallocating memory with xz_dec_end() and xz_dec_init().
 *
 * In single-call mode, xz_dec_reset() is always called in the beginning of
 * xz_dec_run(). Thus, explicit call to xz_dec_reset() is useful only in
 * multi-call mode.
 */
void xz_dec_reset(struct xz_dec *s)
{
  s->sequence = SEQ_STREAM_HEADER;
  s->allow_buf_error = 0;
  s->pos = 0;
  s->crc = 0;
  memset(&s->block, 0, sizeof(s->block));
  memset(&s->index, 0, sizeof(s->index));
  s->temp.pos = 0;
  s->temp.size = STREAM_HEADER_SIZE;
}

/**
 * Allocate and initialize a XZ decoder state
 * @mode:       Operation mode
 * @dict_max:   Maximum size of the LZMA2 dictionary (history buffer) for
 *              multi-call decoding. LZMA2 dictionary is always 2^n bytes
 *              or 2^n + 2^(n-1) bytes (the latter sizes are less common
 *              in practice), so other values for dict_max don't make sense.
 *              In the kernel, dictionary sizes of 64 KiB, 128 KiB, 256 KiB,
 *              512 KiB, and 1 MiB are probably the only reasonable values,
 *              except for kernel and initramfs images where a bigger
 *              dictionary can be fine and useful.
 *
 * dict_max specifies the maximum allowed dictionary size that xz_dec_run()
 * may allocate once it has parsed the dictionary size from the stream
 * headers. This way excessive allocations can be avoided while still
 * limiting the maximum memory usage to a sane value to prevent running the
 * system out of memory when decompressing streams from untrusted sources.
 *
 * returns NULL on failure.
 */
struct xz_dec *xz_dec_init(unsigned dict_max)
{
  struct xz_dec *s = malloc(sizeof(*s));
  if (!s)
    return NULL;

  s->bcj = malloc(sizeof(*s->bcj));
  if (!s->bcj)
    goto error_bcj;

  s->lzma2 = xz_dec_lzma2_create(dict_max);
  if (!s->lzma2)
    goto error_lzma2;

  xz_dec_reset(s);
  return s;

error_lzma2:
  free(s->bcj);
error_bcj:
  free(s);
  return NULL;
}

/**
 * xz_dec_end() - Free the memory allocated for the decoder state
 * @s:          Decoder state allocated using xz_dec_init(). If s is NULL,
 *              this function does nothing.
 */
void xz_dec_end(struct xz_dec *s)
{
  if (s) {
    free((s->lzma2)->dict.buf);
    free(s->lzma2);

    free(s->bcj);
    free(s);
  }
}

static char in[BUFSIZ];
static char out[BUFSIZ];

void do_xzcat(int fd, char *name)
{
  struct xz_buf b;
  struct xz_dec *s;
  enum xz_ret ret;
  const char *msg;

  crc_init(xz_crc32_table, 1);
  const uint64_t poly = 0xC96C5795D7870F42ULL;
  unsigned i;
  unsigned j;
  uint64_t r;

  char *errors[] = {
    "Memory allocation failed",
    "Memory usage limit reached",
    "Not a .xz file",
    "Unsupported options in the .xz headers",
    // 2 things in the enum xz_ret use this
    "File is corrupt",
    "File is corrupt",
  };

  /* initialize CRC64 table*/
  for (i = 0; i < 256; ++i) {
    r = i;
    for (j = 0; j < 8; ++j)
      r = (r >> 1) ^ (poly & ~((r & 1) - 1));

    xz_crc64_table[i] = r;
  }

  /*
   * Support up to 64 MiB dictionary. The actually needed memory
   * is allocated once the headers have been parsed.
   */
  s = xz_dec_init(1 << 26);
  if (!s) {
    msg = "Memory allocation failed\n";
    goto error;
  }

  b.in = in;
  b.in_pos = 0;
  b.in_size = 0;
  b.out = out;
  b.out_pos = 0;
  b.out_size = BUFSIZ;

  for (;;) {
    if (b.in_pos == b.in_size) {
      b.in_size = read(fd, in, sizeof(in));
      b.in_pos = 0;
    }

    ret = xz_dec_run(s, &b);

    if (b.out_pos == sizeof(out)) {
      if (fwrite(out, 1, b.out_pos, stdout) != b.out_pos) {
        msg = "Write error\n";
        goto error;
      }

      b.out_pos = 0;
    }

    if (ret == XZ_OK || ret == XZ_UNSUPPORTED_CHECK)
      continue;

    if (fwrite(out, 1, b.out_pos, stdout) != b.out_pos) {
      msg = "Write error\n";
      goto error;
    }

    if (ret == XZ_STREAM_END) {
      xz_dec_end(s);
      return;
    }

    msg = (ret-3 < ARRAY_LEN(errors)) ? errors[ret-3] : "Bug!";
    goto error;
  }

error:
  xz_dec_end(s);
  error_exit("%s", msg);
}

void xzcat_main(void)
{
  loopfiles(toys.optargs, do_xzcat);
}
