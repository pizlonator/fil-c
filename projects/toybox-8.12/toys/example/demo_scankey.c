/* demo_scankey.c - collate incoming ansi escape sequences.
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 *
 * TODO sigwinch

USE_DEMO_SCANKEY(NEWTOY(demo_scankey, 0, TOYFLAG_BIN))

config DEMO_SCANKEY
  bool "demo_scankey"
  default n
  help
    usage: demo_scankey

    Move a letter around the screen. Hit ESC to exit.
*/

#define FOR_demo_scankey
#include "toys.h"

void demo_scankey_main(void)
{
  time_t t[2];
  unsigned width, height, tick;
  char c = 'X', scratch[16];
  int key, x, y;

  t[0] = t[1] = x = tick = 0;
  memset(scratch, 0, 16);
  y = 1;

  sigatexit(tty_sigreset);  // Make ctrl-c restore tty
  // hide cursor, reset color to default, clear screen
  xputsn("\e[?25l\e0m\e[2J");
  xset_terminal(1, 1, 0, 0); // Raw mode

  for (;;) {
    printf("\e[%u;%uH%c", y+1, x+1, c);
    t[1&++tick] = time(0);
    if (t[0] != t[1]) terminal_probesize(&width, &height);
    // Don't block first time through, to force header print
    key = scan_key_getsize(scratch, -1*!!t[0], &width, &height);
    printf("\e[HESC to exit: ");
    // Print unknown escape sequence
    if (*scratch) {
      printf("key=[ESC");
      // Fetch rest of sequence after deviation, time gap determines end
      while (0<(key = scan_key_getsize(scratch, 0, &width, &height)))
        printf("%c", key);
      printf("] ");
    } else printf("key=%d ", key);
    printf("x=%d y=%d width=%d height=%d\e[K", x, y, width, height);
    fflush(0);

    if (key == -2) continue;
    if (key <= ' ') break;
    if (key>=256) {
      printf("\e[%u;%uH ", y+1, x+1);

      key -= 256;
      if (key==KEY_UP) y--;
      else if (key==KEY_DOWN) y++;
      else if (key==KEY_RIGHT) x++;
      else if (key==KEY_LEFT) x--;
      else if (key==KEY_PGUP) y = 0;
      else if (key==KEY_PGDN) y = 999;
      else if (key==KEY_HOME) x = 0;
      else if (key==KEY_END) x = 999;
      if (y<1) y = 1;
      if (y>=height) y = height-1;
      if (x<0) x = 0;
      if (x>=width) x = width-1;
    } else c = key;
  }
  tty_reset();
}
