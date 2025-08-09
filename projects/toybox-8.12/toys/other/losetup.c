/* losetup.c - Loopback setup
 *
 * Copyright 2012 Rob Landley <rob@landley.net>
 *
 * No standard. (Sigh.)

USE_LOSETUP(NEWTOY(losetup, ">2S(sizelimit)#s(show)ro#j:fdcaD[!afj]", TOYFLAG_SBIN))

config LOSETUP
  bool "losetup"
  default y
  help
    usage: losetup [-cdrs] [-o OFFSET] [-S SIZE] {-d DEVICE...|-j FILE|-af|{DEVICE FILE}}

    Associate a loopback device with a file, or show current file (if any)
    associated with a loop device.

    Instead of a device:
    -a	Iterate through all loopback devices
    -f	Find first unused loop device (may create one)
    -j FILE	Iterate through all loopback devices associated with FILE

    existing:
    -c	Check capacity (file size changed)
    -d DEV	Detach loopback device
    -D	Detach all loopback devices

    new:
    -s	Show device name (alias --show)
    -o OFF	Start association at offset OFF into FILE
    -r	Read only
    -S SIZE	Limit SIZE of loopback association (alias --sizelimit)
*/

#define FOR_losetup
#include "toys.h"
#include <linux/loop.h>

GLOBALS(
  char *j;
  long o, S;

  int openflags;
  dev_t jdev;
  ino_t jino;
  char *dir;
)

// -f: *device is NULL

// Perform requested operation on one device. Returns 1 if handled, 0 if error
static int loopback_setup(char *device, char *file)
{
  struct loop_info64 *loop = (void *)(toybuf+32);
  int lfd = -1, ffd = -1;
  int racy = !device;

  // Open file (ffd) and loop device (lfd)

  if (file) ffd = xopen(file, TT.openflags);
  if (!device) {
    int i, cfd = open("/dev/loop-control", O_RDWR);

    // We assume /dev is devtmpfs so device creation has no lag. Otherwise
    // just preallocate loop devices and stay within them.

    // mount -o loop depends on found device being at the start of toybuf.
    if (cfd != -1) {
      if (0 <= (i = ioctl(cfd, LOOP_CTL_GET_FREE))) {
        sprintf(device = toybuf, "%s/loop%d", TT.dir, i);
      }
      close(cfd);
    }
    if (CFG_TOYBOX_ON_ANDROID && device) {
      // ANDROID SPECIFIC: /dev is not devtmpfs, instead an userspace daemon
      // ueventd is responsible for creating the loop devices under /dev.
      // Wait for the uevent to be processed to avoid race.
      long long timeout = millitime() + 5000;
      do {
        if (!access(device, F_OK) || errno != ENOENT) break;
        msleep(20);
      } while (millitime() < timeout);
    }
  }

  if (device) lfd = open(device, TT.openflags);

  // Stat the loop device to see if there's a current association.
  memset(loop, 0, sizeof(struct loop_info64));
  if (-1 == lfd || ioctl(lfd, LOOP_GET_STATUS64, loop)) {
    if (errno == ENXIO && (FLAG(a) || FLAG(j))) goto done;
    // ENXIO expected if we're just trying to print the first unused device.
    if (errno == ENXIO && FLAG(f) && !file) {
      puts(device);
      goto done;
    }
    if (errno != ENXIO || !file) {
      perror_msg_raw(device ? device : "-f");
      goto done;
    }
  }

  // Skip -j filtered devices
  if (TT.j && (loop->lo_device != TT.jdev || loop->lo_inode != TT.jino))
    goto done;

  // Check size of file or delete existing association
  if (FLAG(c) || FLAG(d)) {
    // The constant is LOOP_SET_CAPACITY
    if (ioctl(lfd, FLAG(c) ? 0x4C07 : LOOP_CLR_FD, 0)) {
      perror_msg_raw(device);
      goto done;
    }
  // Associate file with this device?
  } else if (file) {
    char *f_path = xabspath(file, ABS_PATH);

    if (!f_path) perror_exit("%s", file); // already opened but if deleted since
    if (ioctl(lfd, LOOP_SET_FD, ffd)) {
      free(f_path);
      if (racy && errno == EBUSY) return 1;
      perror_exit("%s=%s", device, file);
    }
    xstrncpy((char *)loop->lo_file_name, f_path, LO_NAME_SIZE);
    free(f_path);
    loop->lo_offset = TT.o;
    loop->lo_sizelimit = TT.S;
    if (ioctl(lfd, LOOP_SET_STATUS64, loop)) perror_exit("%s=%s", device, file);
    if (FLAG(s)) puts(device);
  }
  else {
    xprintf("%s: [%lld]:%llu (%s)", device, (long long)loop->lo_device,
      (long long)loop->lo_inode, loop->lo_file_name);
    if (loop->lo_offset) xprintf(", offset %llu",
      (unsigned long long)loop->lo_offset);
    if (loop->lo_sizelimit) xprintf(", sizelimit %llu",
      (unsigned long long)loop->lo_sizelimit);
    xputc('\n');
  }

done:
  xclose(ffd);
  xclose(lfd);
  return 0;
}

// Perform an action on all currently existing loop devices
static int dash_a(struct dirtree *node)
{
  char *s = node->name;

  // Initial /dev node needs to recurse down one level, then only loop[0-9]*
  if (!node->parent) return DIRTREE_RECURSE;
  if (strncmp(s, "loop", 4) || !isdigit(s[4])) return 0;

  s = dirtree_path(node, 0);
  loopback_setup(s, 0);
  free(s);

  return 0;
}

void losetup_main(void)
{
  char **s;

  TT.dir = CFG_TOYBOX_ON_ANDROID ? "/dev/block" : "/dev";
  TT.openflags = FLAG(r) ? O_RDONLY : O_RDWR;

  if (TT.j) {
    struct stat st;

    xstat(TT.j, &st);
    TT.jdev = st.st_dev;
    TT.jino = st.st_ino;
  }

  // With just device, display current association
  // -a, -f substitute for device
  // -j substitute for device

  // new association: S size o offset rs - need a file
  // existing association: cd

  // -f(dc FILE)

  if (FLAG(D)) toys.optflags |= FLAG_a | FLAG_d;

  if (FLAG(f)) {
    if (toys.optc > 1) perror_exit("max 1 arg");
    while (loopback_setup(NULL, *toys.optargs));
  } else if (FLAG(a) || FLAG(j)) {
    if (toys.optc) error_exit("bad args");
    dirtree_read(TT.dir, dash_a);
  // Do we need one DEVICE argument?
  } else {
    char *file = (FLAG(c) || FLAG(d)) ? NULL : toys.optargs[1];

    if (!toys.optc || (file && toys.optc != 2))
      help_exit("needs %d arg%s", 1+!!file, file ? "s" : "");
    for (s = toys.optargs; *s; s++) {
      loopback_setup(*s, file);
      if (file) break;
    }
  }
}
