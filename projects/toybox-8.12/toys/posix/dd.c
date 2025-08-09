/* dd.c - convert/copy a file
 *
 * Copyright 2013 Ashwini Kumar <ak.ashwini@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/dd.html
 *
 * Deviations from posix: no conversions, no cbs=
 * TODO: seek=n with unseekable output? (Read output and... write it back?)

USE_DD(NEWTOY(dd, 0, TOYFLAG_USR|TOYFLAG_BIN))

config DD
  bool "dd"
  default y
  help
    usage: dd [if|of=FILE] [ibs|obs|bs|count|seek|skip=N] [conv|status|iflag|oflag=FLAG[,FLAG...]]

    Copy/convert blocks of data from input to output, with the following
    keyword=value modifiers (and their default values):

    if=FILE  Read FILE (stdin)          of=FILE  Write to FILE (stdout)
       bs=N  Block size in bytes (512)  count=N  Stop after copying N blocks (all)
      ibs=N  Input block size (bs=)       obs=N  Output block size (bs=)
     skip=N  Skip N input blocks (0)     seek=N  Skip N output blocks (0)

    Each =N value accepts the normal unit suffixes (see toybox --help).

    These modifiers take a comma separated list of potential options:

    iflag=count_bytes,skip_bytes   count=N or skip=N is in bytes not blocks
    oflag=seek_bytes,append        seek=N is in bytes, append output to file
    status=noxfer,none             don't show transfer rate, no summary info
    conv=
      notrunc  Don't truncate output    noerror  Continue after read errors
      sync     Zero pad short reads     fsync    Flush output to disk at end
      sparse   Seek past zeroed output  excl     Fail if output file exists
      nocreat  Fail if of=FILE missing
*/

#define FOR_dd
#include "toys.h"

GLOBALS(
  // Display fields
  int show_xfer, show_records;
  unsigned long long bytes, in_full, in_part, out_full, out_part, start;
)

struct dd_flag {
  char *name;
};

static const struct dd_flag dd_conv[] = TAGGED_ARRAY(DD_conv,
  {"fsync"}, {"noerror"}, {"notrunc"}, {"sync"}, // TODO sparse excl nocreat
);

static const struct dd_flag dd_iflag[] = TAGGED_ARRAY(DD_iflag,
  {"count_bytes"}, {"skip_bytes"},
);

static const struct dd_flag dd_oflag[] = TAGGED_ARRAY(DD_oflag,
  {"seek_bytes"},
);

static void status()
{
  unsigned long long now = millitime()-TT.start ? : 1, bytes = TT.bytes*1000;

  if (TT.show_records)
    fprintf(stderr, "%llu+%llu records in\n%llu+%llu records out\n",
            TT.in_full, TT.in_part, TT.out_full, TT.out_part);

  if (TT.show_xfer) {
    human_readable(toybuf, TT.bytes, HR_SPACE|HR_B);
    fprintf(stderr, "%llu bytes (%s) copied, ", TT.bytes, toybuf);
    bytes = (bytes>TT.bytes) ? bytes/now : TT.bytes/((now+999)/1000);
    human_readable(toybuf, bytes, HR_SPACE|HR_B);
    fprintf(stderr, "%llu.%03u s, %s/s\n", now/1000, (int)(now%1000), toybuf);
  }
}

static void parse_flags(char *what, char *arg,
    const struct dd_flag* flags, int flag_count, unsigned *result)
{
  char *pre = xstrdup(arg);
  int i;

  for (i = 0; i<flag_count; ++i)
    while (comma_remove(pre, flags[i].name)) *result |= 1<<i;
  if (*pre) error_exit("bad %s=%s", what, pre);
  free(pre);
}

// Multiply detecting overflow.
static unsigned long long overmul(unsigned long long x, unsigned long long y)
{
  unsigned long long ll = x*y;

  if (x && y && (ll<x || ll<y)) error_exit("overflow");

  return ll;
}

// Handle funky posix 1x2x3 syntax.
static unsigned long long argxarg(char *arg, int cap)
{
  long long ll = 1;
  char x QUIET, *s, *new;

  arg = xstrdup(arg);
  for (new = s = arg;; new = s+1) {
    // atolx() handles 0x hex prefixes, so skip past those looking for separator
    if ((s = strchr(new+2*!strncmp(new, "0x", 2), 'x'))) {
      if (s==new) break;
      x = *s;
      *s = 0;
    }
    ll = overmul(ll, atolx(new));
    if (s) *s = x;
    else break;
  }
  if (s || ll<cap || ll>(cap ? LONG_MAX : LLONG_MAX)) error_exit("bad %s", arg);
  free(arg);

  return ll;
}


// Point 1 or 2 iovec at len bytes at buf, starting at "start" and wrapping
// around at buflen.
int iovwrap(char *buf, unsigned long long buflen, unsigned long long start,
  unsigned long long len, struct iovec *iov)
{
  iov[0].iov_base = buf + start;
  iov[0].iov_len = len;
  if (start+len<=buflen) return 1;

  iov[1].iov_len = len-(iov[0].iov_len = buflen-start);
  iov[1].iov_base = buf;

  return 2;
}

void dd_main()
{
  char **args, *arg, *iname = 0, *oname = 0, *buf;
  unsigned long long bs = 0, seek = 0, skip = 0, ibs = 512, obs = 512,
    count = ULLONG_MAX, buflen;
  long long len;
  struct iovec iov[2];
  int opos, olen, ifd = 0, ofd = 1, trunc = O_TRUNC, ii;
  unsigned conv = 0, iflag = 0, oflag = 0;

  TT.show_xfer = TT.show_records = 1;

  for (args = toys.optargs; (arg = *args); args++) {
    if (strstart(&arg, "bs=")) bs = argxarg(arg, 1);
    else if (strstart(&arg, "ibs=")) ibs = argxarg(arg, 1);
    else if (strstart(&arg, "obs=")) obs = argxarg(arg, 1);
    else if (strstart(&arg, "count=")) count = argxarg(arg, 0);
    else if (strstart(&arg, "if=")) iname = arg;
    else if (strstart(&arg, "of=")) oname = arg;
    else if (strstart(&arg, "seek=")) seek = argxarg(arg, 0);
    else if (strstart(&arg, "skip=")) skip = argxarg(arg, 0);
    else if (strstart(&arg, "status=")) {
      if (!strcmp(arg, "noxfer")) TT.show_xfer = 0;
      else if (!strcmp(arg, "none")) TT.show_xfer = TT.show_records = 0;
      else error_exit("unknown status '%s'", arg);
    } else if (strstart(&arg, "conv="))
      parse_flags("conv", arg, dd_conv, ARRAY_LEN(dd_conv), &conv);
    else if (strstart(&arg, "iflag="))
      parse_flags("iflag", arg, dd_iflag, ARRAY_LEN(dd_iflag), &iflag);
    else if (strstart(&arg, "oflag="))
      parse_flags("oflag", arg, dd_oflag, ARRAY_LEN(dd_oflag), &oflag);
    else error_exit("bad arg %s", arg);
  }
  if (bs) ibs = obs = bs; // bs overrides ibs and obs regardless of position

  TT.start = millitime();
  sigatexit(status);
  xsignal(SIGUSR1, status);

  // If bs set, output blocks match input blocks (passing along short reads).
  // Else read ibs blocks and write obs, which worst case requires ibs+obs-1.
  buf = xmalloc(buflen = ibs+obs*!bs);
  if (buflen<ibs || buflen<obs) error_exit("tilt");

  if (conv & _DD_conv_notrunc) trunc = 0;
  if (iname) ifd = xopenro(iname);
  else iname = "stdin";
  if (oname) ofd = xcreate(oname, O_WRONLY|O_CREAT|(trunc*!seek),0666);
  else oname = "stdout";

  // Implement skip=
  if (skip) {
    if (!(iflag & _DD_iflag_skip_bytes)) skip *= ibs;
    if (lseek(ifd, skip, SEEK_CUR) < 0) {
      for (; skip > 0; skip -= len) {
        len = read(ifd, buf, minof(skip, ibs));
        if (len < 0) {
          perror_msg_raw(iname);
          if (conv & _DD_conv_noerror) status();
          else return;
        } else if (!len) return xprintf("%s: Can't skip\n", iname);
      }
    }
  }

  // Implement seek= and truncate as necessary. We handled position zero
  // truncate with O_TRUNC on open, so output to /dev/null etc doesn't error.
  if (!(oflag & _DD_oflag_seek_bytes)) seek *= obs;
  if (seek) {
    struct stat st;

    xlseek(ofd, seek, SEEK_CUR);
    if (trunc && !fstat(ofd, &st) && S_ISREG(st.st_mode) && ftruncate(ofd,seek))
      perror_exit("truncate");
  }

  if (count!=ULLONG_MAX && !(iflag & _DD_iflag_count_bytes))
    count = overmul(count, ibs);

  // output start position, output bytes available
  opos = olen = 0;
  for (;;) {
    // Write as many output blocks as we can. Using writev() avoids memmove()
    // to realign data but is still a single atomic write.
    while (olen>=obs || (olen && (bs || !count))) {
      errno = 0;
      len = writev(ofd, iov, iovwrap(buf, buflen, opos, minof(obs, olen), iov));
      if (len<1) {
        if (errno==EINTR) continue;
        perror_exit("%s: write error", oname);
      }
      TT.bytes += len;
      olen -= len;
      if ((opos += len)>=buflen) opos -= buflen;
      if (len == obs) TT.out_full++;
      else TT.out_part++;
    }
    if (!count) break;

    // Read next block of input. (There MUST be enough space, we sized buf.)
    len = opos+olen;
    if (len>buflen) len -= buflen;
    errno = 0;
    if (2 == (ii = iovwrap(buf, buflen, len, minof(count, ibs), iov)))
      memset(iov[1].iov_base, 0, iov[1].iov_len);
    memset(iov[0].iov_base, 0, iov[0].iov_len);
    len = readv(ifd, iov, ii);
    if (len<1) {
      if (errno==EINTR) continue;
      if (!len) count = 0;
      else {
        //read error case.
        perror_msg("%s: read error", iname);
        if (!(conv & _DD_conv_noerror)) xexit();

        // Complain and try to seek past it
        status();
        lseek(ifd, ibs, SEEK_CUR);
        if (conv & _DD_conv_sync) olen += ibs;
      }

      continue;
    }
    if (len == ibs) TT.in_full++;
    else TT.in_part++;
    if (conv & _DD_conv_sync) len = ibs;
    olen += len;
    count -= minof(len, count);
  }
  if ((conv & _DD_conv_fsync) && fsync(ofd)) perror_exit("%s: fsync", oname);

  if (CFG_TOYBOX_FREE) {
    close(ifd);
    close(ofd);
    free(buf);
  }
}
