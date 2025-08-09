/* od.c - Provide octal/hex dumps of data
 *
 * Copyright 2012 Andre Renaud <andre@bluewatersys.com>
 * Copyright 2012 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/od.html

USE_OD(NEWTOY(od, "j#vw#<1=16N#xsodcbA:t*", TOYFLAG_USR|TOYFLAG_BIN))

config OD
  bool "od"
  default y
  help
    usage: od [-bcdosxv] [-j #] [-N #] [-w #] [-A doxn] [-t acdfoux[#]]

    Dump data in octal/hex.

    -A	Address base (decimal, octal, hexadecimal, none)
    -j	Skip this many bytes of input
    -N	Stop dumping after this many bytes
    -t	Output type a(scii) c(har) d(ecimal) f(loat) o(ctal) u(nsigned) (he)x
    	plus optional size in bytes
    	aliases: -b=-t o1, -c=-t c, -d=-t u2, -o=-t o2, -s=-t d2, -x=-t x2
    -v	Don't collapse repeated lines together
    -w	Total line width in bytes (default 16)
*/

#define FOR_od
#include "toys.h"

GLOBALS(
  struct arg_list *t;
  char *A;
  long N, w, j;

  int address_idx;
  unsigned types, leftover, star;
  char *buf; // Points to buffers[0] or buffers[1].
  char *bufs[2]; // Used to detect duplicate lines.
  off_t pos;
)

static char *ascii = "nulsohstxetxeotenqackbel bs ht nl vt ff cr so si"
  "dledc1dc2dc3dc4naksynetbcan emsubesc fs gs rs us sp";

struct odtype {
  int type;
  int size;
};

static int od_out_t(struct odtype *t, char *buf, int *offset)
{
  unsigned k;
  int throw = 0, pad = 0;

  // Handle ascii
  if (t->type < 2) {
    char c = TT.buf[(*offset)++];

    pad += 4;
    if (!t->type) {
      c &= 127;
      if (c<=32) sprintf(buf, "%.3s", ascii+(3*c));
      else if (c==127) strcpy(buf, "del");
      else sprintf(buf, "%c", c);
    } else {
      char *bfnrtav = "\b\f\n\r\t\a\v", *s = strchr(bfnrtav, c);
      if (s) sprintf(buf, "\\%c", "bfnrtav0"[s-bfnrtav]);
      else if (c < 32 || c >= 127) sprintf(buf, "%03o", c);
      else {
        // TODO: this should be UTF8 aware.
        sprintf(buf, "%c", c);
      }
    }
  } else if (CFG_TOYBOX_FLOAT && t->type == 6) {
    long double ld;
    union {float f; double d; long double ld;} fdl;

    memcpy(&fdl, TT.buf+*offset, t->size);
    *offset += t->size;
    if (sizeof(float) == t->size) {
      ld = fdl.f;
      pad += (throw = 8)+7;
    } else if (sizeof(double) == t->size) {
      ld = fdl.d;
      pad += (throw = 17)+8;
    } else if (sizeof(long double) == t->size) {
      ld = fdl.ld;
      pad += (throw = 21)+9;
    } else error_exit("bad -tf '%d'", t->size);

    sprintf(buf, "%.*Le", throw, ld);
  // Integer types
  } else {
    unsigned long long ll = 0, or;
    char *c[] = {"%*lld", "%*llu", "%0*llo", "%0*llx"}, *class = c[t->type-2];

    // Work out width of field
    if (t->size == 8) {
      or = -1LL;
      if (t->type == 2) or >>= 1;
    } else or = (1LL<<(8*t->size))-1;
    throw = sprintf(buf, class, 0, or);

    // Accumulate integer based on size argument
    for (k=0; k < t->size; k++) {
      or = TT.buf[(*offset)++];
      ll |= or << (8*(IS_BIG_ENDIAN ? t->size-k-1 : k));
    }

    // Handle negative values
    if (t->type == 2) {
      or = sizeof(or) - t->size;
      throw++;
      if (or && (ll & (1l<<((8*t->size)-1))))
        ll |= ((or<<(8*or))-1) << (8*t->size);
    }

    sprintf(buf, class, throw, ll);
    pad += throw+1;
  }

  return pad;
}

static void od_outline(void)
{
  char buf[128], *abases[] = {"", "%07lld", "%07llo", "%06llx"};
  struct odtype *types = (struct odtype *)toybuf;
  int i, j, len, pad;

  if (TT.leftover<TT.w) memset(TT.buf+TT.leftover, 0, TT.w-TT.leftover);

  // Handle duplciate lines as *
  if (!FLAG(v) && TT.j != TT.pos && TT.leftover
    && !smemcmp(TT.bufs[0], TT.bufs[1], TT.w))
  {
    if (!TT.star) {
      xputs("*");
      TT.star++;
    }

  // Print line position
  } else {
    TT.star = 0;

    // off_t varies so expand it to largest possible size
    xprintf(abases[TT.address_idx], (long long)TT.pos);
    if (!TT.leftover) {
      if (TT.address_idx) xputc('\n');
      return;
    }
  }

  TT.pos += len = TT.leftover;
  TT.leftover = 0;
  if (TT.star) return;

  // Find largest "pad" of the output types.
  for (i = pad = 0; i<TT.types; i++) {
    int bytes = 0;

    // If more than one byte of input consumed, average rounding up.
    j = od_out_t(types+i, buf, &bytes);
    j = (j+bytes-1)/bytes;

    if (j > pad) pad = j;
  }

  // For each output type, print one line
  for (i=0; i<TT.types; i++) {
    for (j = 0; j<len;) {
      int bytes = j;

      // pad for as many bytes as were consumed, and indent non-numbered lines
      od_out_t(types+i, buf, &bytes);
      xprintf("%*s", pad*(bytes-j) + 7*(!!i)*!j, buf);
      j = bytes;
    }
    xputc('\n');
  }

  // Toggle buffer for "same as last time" check.
  TT.buf = (TT.buf == TT.bufs[0]) ? TT.bufs[1] : TT.bufs[0];
}

// Loop through input files
static void do_od(int fd, char *name)
{
  // Skip input, possibly more than one entire file.
  if (TT.j > TT.pos) {
    off_t pos = TT.j-TT.pos, off = lskip(fd, pos);

    if (off >= 0) TT.pos += pos-off;
    if (TT.j > TT.pos) return;
  }

  for(;;) {
    char *buf = TT.buf + TT.leftover;
    int len = TT.w - TT.leftover;

    if (FLAG(N)) {
      if (!TT.N) break;
      if (TT.N < len) len = TT.N;
    }

    len = readall(fd, buf, len);
    if (len < 0) {
      perror_msg_raw(name);
      break;
    }
    if (TT.N) TT.N -= len;
    TT.leftover += len;
    if (TT.leftover < TT.w) break;

    od_outline();
  }
}

// Handle one -t argument (including implicit ones)
static void append_base(char *base)
{
  char *s = base;
  struct odtype *types = (struct odtype *)toybuf;
  int type;

  for (;;) {
    int size = 1;

    if (!*s) return;
    if (TT.types >= sizeof(toybuf)/sizeof(struct odtype)) break;
    if (-1 == (type = stridx("acduox"USE_TOYBOX_FLOAT("f"), *(s++)))) break;

    if (isdigit(*s)) {
      size = strtol(s, &s, 10);
      if (type < 2 && size != 1) break;
      if (CFG_TOYBOX_FLOAT && type == 6 && size == sizeof(long double));
      else if (size < 1 || size > 8) break;
    } else if (CFG_TOYBOX_FLOAT && type == 6) {
      int sizes[] = {sizeof(float), sizeof(double), sizeof(long double)};
      if (-1 == (size = stridx("FDL", *s))) size = sizeof(double);
      else {
        s++;
        size = sizes[size];
      }
    } else if (type > 1) {
      if (-1 == (size = stridx("CSIL", *s))) size = 4;
      else {
        s++;
        size = 1 << size;
      }
    }

    types[TT.types].type = type;
    types[TT.types].size = size;
    TT.types++;
  }

  error_exit("bad -t %s", base);
}

void od_main(void)
{
  struct arg_list *arg;

  TT.bufs[0] = xzalloc(TT.w);
  TT.bufs[1] = xzalloc(TT.w);
  TT.buf = TT.bufs[0];

  if (!TT.A) TT.address_idx = 2;
  else if (0>(TT.address_idx = stridx("ndox", *TT.A)))
    error_exit("bad -A '%c'", *TT.A);

  // Collect -t entries

  for (arg = TT.t; arg; arg = arg->next) append_base(arg->arg);
  if (FLAG(b)) append_base("o1");
  if (FLAG(c)) append_base("c");
  if (FLAG(d)) append_base("u2");
  if (FLAG(o)) append_base("o2");
  if (FLAG(s)) append_base("d2");
  if (FLAG(x)) append_base("x2");
  if (!TT.types) append_base("o2");

  loopfiles(toys.optargs, do_od);

  if (TT.leftover) od_outline();
  od_outline();

  if (CFG_TOYBOX_FREE) {
    free(TT.bufs[0]);
    free(TT.bufs[1]);
  }
}
