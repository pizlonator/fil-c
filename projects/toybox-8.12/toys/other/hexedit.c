/* hexedit.c - Hexadecimal file editor
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 *
 * No standard.

USE_HEXEDIT(NEWTOY(hexedit, "<1>1r", TOYFLAG_USR|TOYFLAG_BIN))

config HEXEDIT
  bool "hexedit"
  default y
  help
    usage: hexedit [-r] FILE

    Hexadecimal file editor/viewer. All changes are written to disk immediately.

    -r	Read only (display but don't edit)

    Keys:
    Arrows         Move left/right/up/down by one line/column
    PgUp/PgDn      Move up/down by one page
    Home/End       Start/end of line (start/end of file with ctrl)
    0-9, a-f       Change current half-byte to hexadecimal value
    ^J or :        Jump (+/- for relative offset, otherwise absolute address)
    ^F or /        Find string (^G/n: next, ^D/p: previous match)
    u              Undo
    x              Toggle bw/color display
    q/^C/^Q/Esc    Quit
*/

#define FOR_hexedit
#include "toys.h"

GLOBALS(
  char *data, *search, keybuf[16], input[80];
  long long len, base, pos;
  int numlen, undo, undolen, mode;
  unsigned rows, cols;
)

#define UNDO_LEN (sizeof(toybuf)/(sizeof(long long)+1))

static void show_error(char *what)
{
  printf("\e[%dH\e[41m\e[37m\e[K\e[1m%s\e[0m", TT.rows+1, what);
  fflush(0);
  msleep(500);
}

// TODO: support arrow keys, insertion, and scrolling (and reuse in vi)
static int prompt(char *prompt, char *initial_value)
{
  int yes = 0, key, len = strlen(initial_value);

  strcpy(TT.input, initial_value);
  while (1) {
    printf("\e[%dH\e[K\e[1m%s: \e[0m%s\e[?25h", TT.rows+1, prompt, TT.input);
    fflush(0);

    key = scan_key(TT.keybuf, -1);
    if (key < 0 || key == 27) break;
    if (key == '\r') {
      yes = len; // Hitting enter with no input counts as cancellation.
      break;
    }

    if (key == 0x7f && (len > 0)) TT.input[--len] = 0;
    else if (key == 'U'-'@') while (len > 0) TT.input[--len] = 0;
    else if (key >= ' ' && key < 0x7f && len < sizeof(TT.input))
      TT.input[len++] = key;
  }
  printf("\e[?25l");

  return yes;
}

// Render all characters printable, using color to distinguish.
static void draw_char(int ch)
{
  if (ch >= ' ' && ch < 0x7f) {
    putchar(ch);
    return;
  }

  if (TT.mode) {
    if (ch>127) {
      printf("\e[2m");
      ch &= 127;
    }
    if (ch<32 || ch==127) {
      printf("\e[7m");
      if (ch==127) ch = 32;
      else ch += 64;
    }
    xputc(ch);
  } else {
    if (ch < ' ') printf("\e[31m%c", ch + '@');
    else printf("\e[35m?");
  }
  printf("\e[0m");
}

static void draw_status(void)
{
  char line[80];

  printf("\e[%dH\e[K", TT.rows+1);

  snprintf(line, sizeof(line), "\"%s\"%s, %#llx/%#llx", *toys.optargs,
    FLAG(r) ? " [readonly]" : "", TT.pos, TT.len);
  draw_trim(line, -1, TT.cols);
}

static void draw_byte(int byte)
{
  if (byte) printf("%02x", byte);
  else printf("\e[2m00\e[0m");
}

static void draw_line(long long yy)
{
  int x, xx = 16;

  yy = (TT.base+yy)*16;
  if (yy+xx>=TT.len) xx = TT.len-yy;

  if (yy<TT.len) {
    printf("\r\e[%dm%0*llx\e[0m ", 33*!TT.mode, TT.numlen, yy);
    for (x=0; x<xx; x++) {
      putchar(' ');
      draw_byte(TT.data[yy+x]);
    }
    printf("%*s", 2+3*(16-xx), "");
    for (x=0; x<xx; x++) draw_char(TT.data[yy+x]);
    printf("%*s", 16-xx, "");
  }
  printf("\e[K");
}

static void draw_page(void)
{
  int y;

  for (y = 0; y<TT.rows; y++) {
    printf(y ? "\r\n" : "\e[H");
    draw_line(y);
  }
  draw_status();
}

// side: 0 = editing left, 1 = editing right, 2 = clear, 3 = read only
static void highlight(int xx, int yy, int side)
{
  char cc = TT.data[16*(TT.base+yy)+xx];
  int i;

  // Display cursor in hex area.
  printf("\e[%u;%uH\e[%dm", yy+1, TT.numlen+3*(xx+1), 7*(side!=2));
  if (side>1) draw_byte(cc);
  else for (i=0; i<2;) {
    if (side==i) printf("\e[32m");
    printf("%x", (cc>>(4*(1&++i)))&15);
  }

  // Display cursor in text area.
  printf("\e[7m\e[%u;%uH"+4*(side==2), yy+1, 1+TT.numlen+17*3+xx);
  draw_char(cc);
}

static void find_next(int pos)
{
  char *p;

  p = memmem(TT.data+pos, TT.len-pos, TT.search, strlen(TT.search));
  if (p) TT.pos = p - TT.data;
  else show_error("No match!");
}

static void find_prev(int pos)
{
  size_t len = strlen(TT.search);

  for (; pos >= 0; pos--) {
    if (!smemcmp(TT.data+pos, TT.search, len)) {
      TT.pos = pos;
      return;
    }
  }
  show_error("No match!");
}

void hexedit_main(void)
{
  long long y;
  int x, i, side = 0, key, fd;

  // Terminal setup
  TT.cols = 80;
  TT.rows = 24;
  terminal_size(&TT.cols, &TT.rows);
  if (TT.rows) TT.rows--;
  xsignal(SIGWINCH, generic_signal);
  sigatexit(tty_sigreset);
  dprintf(1, "\e[0m\e[?25l");
  xset_terminal(1, 1, 0, 0);

  if (access(*toys.optargs, W_OK)) toys.optflags |= FLAG_r;
  fd = xopen(*toys.optargs, FLAG(r) ? O_RDONLY : O_RDWR);
  if ((TT.len = fdlength(fd))<1) error_exit("bad length");
  if (sizeof(long)==32 && TT.len>SIZE_MAX) TT.len = SIZE_MAX;
  // count file length hex in digits, rounded up to multiple of 4
  for (TT.pos = TT.len, TT.numlen = 0; TT.pos; TT.pos >>= 4, TT.numlen++);
  TT.numlen += (4-TT.numlen)&3;

  TT.data=xmmap(0, TT.len, PROT_READ|(PROT_WRITE*!FLAG(r)), MAP_SHARED, fd, 0);
  close(fd);
  draw_page();

  for (;;) {
    // Scroll display if necessary
    if (TT.pos<0) TT.pos = 0;
    if (TT.pos>=TT.len) TT.pos = TT.len-1;
    x = TT.pos&15;
    y = TT.pos/16;

    // scroll up
    while (y<TT.base) {
      if (TT.base-y>(TT.rows/2)) {
        TT.base = y;
        draw_page();
      } else {
        TT.base--;
        printf("\e[H\e[1L");
        draw_line(0);
      }
    }

    // scroll down
    while (y>=TT.base+TT.rows) {
      if (y-(TT.base+TT.rows)>(TT.rows/2)) {
        TT.base = y-TT.rows-1;
        draw_page();
      } else {
        TT.base++;
        printf("\e[H\e[1M\e[%uH", TT.rows);
        draw_line(TT.rows-1);
      }
    }

    draw_status();
    y -= TT.base;

    // Display cursor and flush output
    highlight(x, y, FLAG(r) ? 3 : side);
    fflush(0);

    // Wait for next key
    key = scan_key(TT.keybuf, -1);

    // Window resized?
    if (key == -3) {
      toys.signal = 0;
      terminal_size(&TT.cols, &TT.rows);
      if (TT.rows) TT.rows--;
      draw_page();
      continue;
    }

    if (key == 'x') {
      TT.mode = !TT.mode;
      printf("\e[0m");
      draw_page();
      continue;
    }

    // Various popular ways to quit...
    if (key==-1||key==('C'-'@')||key==('Q'-'@')||key==27||key=='q') break;
    highlight(x, y, 2);

    if (key == ('J'-'@') || key == ':' || key == '-' || key == '+') {
      // Jump (relative or absolute)
      char initial[2] = {}, *s = 0;
      long long val;

      if (key == '-' || key == '+') *initial = key;
      if (!prompt("Jump to", initial)) continue;

      val = estrtol(TT.input, &s, 0);
      if (!errno && s && !*s) {
        if (*TT.input == '-' || *TT.input == '+') TT.pos += val;
        else TT.pos = val;
      }
      continue;
    } else if (key == ('F'-'@') || key == '/') { // Find
      if (!prompt("Find", TT.search ? TT.search : "")) continue;

      // TODO: parse hex escapes in input, and record length to support \0
      free(TT.search);
      TT.search = xstrdup(TT.input);
      find_next(TT.pos);
    } else if (TT.search && (key == ('G'-'@') || key == 'n')) { // Find next
      if (TT.pos < TT.len) find_next(TT.pos+1);
    } else if (TT.search && (key == ('D'-'@') || key == 'p')) { // Find previous
      if (TT.pos > 0) find_prev(TT.pos-1);
    }

    // Remove cursor
    highlight(x, y, 2);

    // Hex digit?
    if (key>='a' && key<='f') key-=32;
    if (!FLAG(r) && ((key>='0' && key<='9') || (key>='A' && key<='F'))) {
      if (!side) {
        long long *ll = (long long *)toybuf;

        ll[TT.undo] = TT.pos;
        toybuf[(sizeof(long long)*UNDO_LEN)+TT.undo++] = TT.data[TT.pos];
        if (TT.undolen < UNDO_LEN) TT.undolen++;
        TT.undo %= UNDO_LEN;
      }

      i = key - '0';
      if (i>9) i -= 7;
      TT.data[TT.pos] &= 15<<(4*side);
      TT.data[TT.pos] |= i<<(4*!side);

      if (++side==2) {
        highlight(x, y, side);
        side = 0;
        ++TT.pos;
      }
    } else side = 0;
    if (key=='u') {
      if (TT.undolen) {
        long long *ll = (long long *)toybuf;

        TT.undolen--;
        if (!TT.undo) TT.undo = UNDO_LEN;
        TT.pos = ll[--TT.undo];
        TT.data[TT.pos] = toybuf[sizeof(long long)*UNDO_LEN+TT.undo];
      }
    }
    if (key>=256) {
      key -= 256;

      if (key==KEY_UP) TT.pos -= 16;
      else if (key==KEY_DOWN) TT.pos += 16;
      else if (key==KEY_RIGHT) {
        if (TT.pos<TT.len) TT.pos++;
      } else if (key==KEY_LEFT) {
        if (TT.pos>0) TT.pos--;
      } else if (key==KEY_PGUP) {
        TT.pos -= 16*TT.rows;
        if (TT.pos < 0) TT.pos = 0;
        TT.base = TT.pos/16;
        draw_page();
      } else if (key==KEY_PGDN) {
        TT.pos += 16*TT.rows;
        if (TT.pos > TT.len-1) TT.pos = TT.len-1;
        TT.base = TT.pos/16;
        draw_page();
      } else if (key==KEY_HOME) TT.pos &= ~0xf;
      else if (key==KEY_END) TT.pos |= 0xf;
      else if (key==(KEY_CTRL|KEY_HOME)) TT.pos = 0;
      else if (key==(KEY_CTRL|KEY_END)) TT.pos = TT.len-1;
    }
  }
  munmap(TT.data, TT.len);
  tty_reset();
}
