/* umount.c - Unmount a mount point.
 *
 * Copyright 2012 Rob Landley <rob@landley.net>
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/umount.html
 *
 * Note: -n (/etc/mtab) is obsolete, /proc/mounts replaced it. Neither chroot
 * nor per-process mount namespaces can work sanely with mtab. The kernel
 * tracks mount points now, a userspace application can't do so anymore.

USE_UMOUNT(NEWTOY(umount, "cndDflrat*v[!na]", TOYFLAG_BIN|TOYFLAG_STAYROOT))

config UMOUNT
  bool "umount"
  default y
  help
    usage: umount [-a [-t TYPE[,TYPE...]]] [-vrfD] [DIR...]

    Unmount the listed filesystems.

    -a	Unmount all mounts in /proc/mounts instead of command line list
    -D	Don't free loopback device(s)
    -f	Force unmount
    -l	Lazy unmount (detach from filesystem now, close when last user does)
    -n	Don't use /proc/mounts
    -r	Remount read only if unmounting fails
    -t	Restrict "all" to mounts of TYPE (or use "noTYPE" to skip)
    -v	Verbose
*/

#define FOR_umount
#include "toys.h"

GLOBALS(
  struct arg_list *t;

  char *types;
)

// TODO (done?)
//   borrow df code to identify filesystem?
//   umount -a from fstab
//   umount when getpid() not 0, according to fstab
//   lookup mount: losetup -d, bind, file, block
//   loopback delete
//   fstab -o user

// TODO
// swapon, swapoff

static void do_umount(char *dir, char *dev, int flags)
{
  // is it ok for this user to umount this mount?
  if (CFG_TOYBOX_SUID && getuid()) {
    struct mtab_list *mt = dlist_terminate(xgetmountlist("/etc/fstab"));
    int len, user = 0;

    while (mt) {
      struct mtab_list *mtemp = mt;
      char *s;

      if (!strcmp(mt->dir, dir)) while ((s = comma_iterate(&mt->opts, &len))) {
        if (len == 4 && strncmp(s, "user", 4)) user = 1;
        else if (len == 6 && strncmp(s, "nouser", 6)) user = 0;
      }

      mt = mt->next;
      free(mtemp);
    }

    if (!user) {
      error_msg("not root");

      return;
    }
  }

  if (!umount2(dir, flags)) {
    if (FLAG(v)) xprintf("%s unmounted\n", dir);

    // Attempt to disassociate loopback device. This ioctl should be ignored
    // for anything else, because lanana allocated ioctl range 'L' to loopback
    if (dev && !FLAG(D)) {
      int lfd = open(dev, O_RDONLY);

      if (lfd != -1) {
        // This is LOOP_CLR_FD, fetching it from headers is awkward
        if (!ioctl(lfd, 0x4C01) && FLAG(v)) xprintf("%s cleared\n", dev);
        close(lfd);
      }
    }

    return;
  }

  if (FLAG(r)) {
    if (!mount("", dir, "", MS_REMOUNT|MS_RDONLY, "")) {
      if (FLAG(v)) xprintf("%s remounted ro\n", dir);
      return;
    }
  }

  perror_msg_raw(dir);
}

void umount_main(void)
{
  char **optargs, *pm = "/proc/mounts";
  struct mtab_list *mlsave = 0, *mlrev = 0, *ml;
  int flags=0;

  if (!toys.optc && !FLAG(a)) error_exit("Need 1 arg or -a");

  if (FLAG(f)) flags |= MNT_FORCE;
  if (FLAG(l)) flags |= MNT_DETACH;

  // Load /proc/mounts and get a reversed list (newest first)
  // We use the list both for -a, and to umount /dev/name or do losetup -d
  if (!FLAG(n) && !access(pm, R_OK))
    mlrev = dlist_terminate(mlsave = xgetmountlist(pm));

  // Unmount all: loop through mounted filesystems, skip -t, unmount the rest
  if (FLAG(a)) {
    char *typestr = 0;
    struct arg_list *tal;

    for (tal = TT.t; tal; tal = tal->next) comma_collate(&typestr, tal->arg);
    for (ml = mlrev; ml; ml = ml->prev)
      if (mountlist_istype(ml, typestr)) do_umount(ml->dir, ml->device, flags);
    if (CFG_TOYBOX_FREE) {
      free(typestr);
      llist_traverse(mlsave, free);
    }
  // TODO: under what circumstances do we umount non-absolute path?
  } else for (optargs = toys.optargs; *optargs; optargs++) {
    char *abs = xabspath(*optargs, 0);

    for (ml = abs ? mlrev : 0; ml; ml = ml->prev) {
      if (!strcmp(ml->dir, abs)) break;
      if (!strcmp(ml->device, abs)) {
        free(abs);
        abs = ml->dir;
        break;
      }
    }

    do_umount(abs ? abs : *optargs, ml ? ml->device : 0, flags);
    if (ml && abs != ml->dir) free(abs);
  }
}
