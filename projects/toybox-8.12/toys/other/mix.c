/* mix.c - A very basic mixer.
 *
 * Copyright 2014 Brad Conroy, dedicated to the Public Domain.
 *

USE_MIX(NEWTOY(mix, "c:d:l#r#", TOYFLAG_USR|TOYFLAG_BIN))

config MIX
  bool "mix"
  default y
  help
   usage: mix [-d DEV] [-c CHANNEL] [-l VOL] [-r RIGHT]

   List OSS sound channels (module snd-mixer-oss), or set volume(s).

   -c CHANNEL	Set/show volume of CHANNEL (default first channel found)
   -d DEV		Device node (default /dev/mixer)
   -l VOL		Volume level
   -r RIGHT	Volume of right stereo channel (with -r, -l sets left volume)
*/

#define FOR_mix
#include "toys.h"
#include <linux/soundcard.h>

GLOBALS(
   long r, l;
   char *d, *c;
)

void mix_main(void)
{
  const char *channels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;
  int mask, channel = -1, level, fd;

  if (!TT.d) TT.d = "/dev/mixer";
  fd = xopen(TT.d, O_RDWR|O_NONBLOCK);
  xioctl(fd, SOUND_MIXER_READ_DEVMASK, &mask);

  for (channel = 0; channel < SOUND_MIXER_NRDEVICES; channel++) {
    if ((1<<channel) & mask) {
      if (TT.c) {
        if (!strcmp(channels[channel], TT.c)) break;
      } else if (FLAG(l)) break;
      else printf("%s\n", channels[channel]);
    }
  }

  if (!FLAG(c) && !FLAG(l)) return;
  else if (channel == SOUND_MIXER_NRDEVICES) error_exit("bad -c '%s'", TT.c);

  if (!FLAG(l)) {
    xioctl(fd, MIXER_READ(channel), &level);
    if (level > 0xFF)
      xprintf("%s:%s = left:%d\t right:%d\n",
              TT.d, channels[channel], level>>8, level & 0xFF);
    else xprintf("%s:%s = %d\n", TT.d, channels[channel], level);
  } else {
    level = TT.l;
    if (!FLAG(r)) level = TT.r | (level<<8);

    xioctl(fd, MIXER_WRITE(channel), &level);
  }

  if (CFG_TOYBOX_FREE) close(fd);
}
