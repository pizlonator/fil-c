/* mount.c - mount filesystems
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * See http://refspecs.linuxfoundation.org/LSB_4.1.0/LSB-Core-generic/LSB-Core-generic/mount.html
 *
 * Note: -hV is bad spec, haven't implemented -FsLU yet
 * no mtab (/proc/mounts does it) so -n is NOP.
 * TODO mount -o loop,autoclear (linux git 96c5865559ce)
 * TODO mount jffs2.img dir (block2mtd)
 * TODO fstab user
 * TODO mount [^/]*:def = nfs, \\samba

USE_MOUNT(NEWTOY(mount, "?RO:afnrvwt:o*[-rw]", TOYFLAG_BIN|TOYFLAG_STAYROOT))
//USE_NFSMOUNT(NEWTOY(nfsmount, "<2>2", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_STAYROOT))

config MOUNT
  bool "mount"
  default y
  help
    usage: mount [-afFrsvw] [-t TYPE] [-o OPTION,] [[DEVICE] DIR]

    Mount new filesystem(s) on directories. With no arguments, display existing
    mounts.

    -a	Mount all entries in /etc/fstab (with -t, only entries of that TYPE)
    -O	Only mount -a entries that have this option
    -f	Fake it (don't actually mount)
    -r	Read only (same as -o ro)
    -w	Read/write (default, same as -o rw)
    -t	Specify filesystem type
    -v	Verbose

    OPTIONS is a comma separated list of options, which can also be supplied
    as --longopts.

    Autodetects loopback mounts (a file on a directory) and bind mounts (file
    on file, directory on directory), so you don't need to say --bind or --loop.
    You can also "mount -a /path" to mount everything in /etc/fstab under /path,
    even if it's noauto. DEVICE starting with UUID= is identified by blkid -U,
    and DEVICE starting with LABEL= is identified by blkid -L.

#config SMBMOUNT
#  bool "smbmount"
#  default n
#  helo
#    usage: smbmount SHARE DIR
#
#    Mount smb share with user/pasword prompt as necessary.
#
#config NFSMOUNT
#  bool "nfsmount"
#  default n
#  help
#    usage: nfsmount SHARE DIR
#
#    Invoke an eldrich horror from the dawn of time.
*/

#define FOR_mount
#include "toys.h"

GLOBALS(
  struct arg_list *o;
  char *t, *O;

  unsigned long flags;
  char *opts;
  int okuser;
)

// mount.tests should check for all of this:
// TODO detect existing identical mount (procfs with different dev name?)
// TODO user, users, owner, group, nofail
// TODO -p (passfd)
// TODO -a -t notype,type2
// TODO --subtree
// TODO make "mount --bind,ro old new" work (implicit -o remount)
// TODO mount -a
// TODO mount -o remount
// TODO fstab: lookup default options for mount
// TODO implement -v
// TODO "mount -a -o remount,ro" should detect overmounts
// TODO work out how that differs from "mount -ar"
// TODO what if you --bind mount a block device somewhere (file, dir, dev)
// TODO "touch servername; mount -t cifs servername path"
// TODO mount -o remount a user mount
// TODO mount image.img sub (auto-loopback) then umount image.img
// TODO mount UUID=blah

// Strip flags out of comma separated list of options, return flags,.
// TODO: flip order and it's tagged array?
static long flag_opts(char *new, long flags, char **more)
{
  struct {
    char *name;
    long flags;
  } opts[] = {
    {"loop", 0}, {"defaults", 0}, {"quiet", 0}, // NOPs
    {"user", 0}, {"nouser", 0}, // checked in fstab, ignored in -o
    {"bind", MS_REC}, {"rbind", ~MS_REC}, // Autodetected but override defaults
    {"ro", MS_RDONLY}, {"rw", ~MS_RDONLY},
    {"nosuid", MS_NOSUID}, {"suid", ~MS_NOSUID},
    {"nodev", MS_NODEV}, {"dev", ~MS_NODEV},
    {"noexec", MS_NOEXEC}, {"exec", ~MS_NOEXEC},
    {"sync", MS_SYNCHRONOUS}, {"async", ~MS_SYNCHRONOUS},
    {"noatime", MS_NOATIME}, {"atime", ~MS_NOATIME},
    {"norelatime", ~MS_RELATIME}, {"relatime", MS_RELATIME},
    {"nodiratime", MS_NODIRATIME}, {"diratime", ~MS_NODIRATIME},
    {"loud", ~MS_SILENT},
    {"shared", MS_SHARED}, {"rshared", MS_SHARED|MS_REC},
    {"slave", MS_SLAVE}, {"rslave", MS_SLAVE|MS_REC},
    {"private", MS_PRIVATE}, {"rprivate", MS_SLAVE|MS_REC},
    {"unbindable", MS_UNBINDABLE}, {"runbindable", MS_UNBINDABLE|MS_REC},
    {"remount", MS_REMOUNT}, {"move", MS_MOVE},
    // mand dirsync rec iversion strictatime
  };

  if (new) for (;;) {
    char *comma = strchr(new, ',');
    int i;

    if (comma) *comma = 0;

    // If we recognize an option, apply flags
    for (i = 0; i < ARRAY_LEN(opts); i++) if (!strcasecmp(opts[i].name, new)) {
      long ll = opts[i].flags;

      if (ll < 0) flags &= ll;
      else flags |= ll;

      break;
    }

    // If we didn't recognize it, keep string version
    if (more && i == ARRAY_LEN(opts)) {
      i = *more ? strlen(*more) : 0;
      *more = xrealloc(*more, i + strlen(new) + 2);
      if (i) (*more)[i++] = ',';
      strcpy(i+*more, new);
    }

    if (!comma) break;
    *comma = ',';
    new = comma + 1;
  }

  return flags;
}

static void mount_filesystem(char *dev, char *dir, char *type,
  unsigned long flags, char *opts)
{
  FILE *fp = 0;
  int rc = EINVAL;
  char *buf = 0;

  if (FLAG(f)) return;

  if (getuid()) {
    if (TT.okuser) TT.okuser = 0;
    else {
      error_msg("'%s' not user mountable in fstab", dev);

      return;
    }
  }

  if (strstart(&dev, "UUID=")) {
    char *s = chomp(xrunread((char *[]){"blkid", "-U", dev, 0}, 0));

    if (!s || strlen(s)>=sizeof(toybuf)) return error_msg("No uuid %s", dev);
    strcpy(dev = toybuf, s);
    free(s);
  } else if (strstart(&dev, "LABEL=")) {
    char *s = chomp(xrunread((char *[]){"blkid", "-L", dev, 0}, 0));

    if (!s || strlen(s)>=sizeof(toybuf)) return error_msg("No label %s", dev);
    strcpy(dev = toybuf, s);
    free(s);
  }

  // Autodetect bind mount or filesystem type

  if (type && (!strcmp(type, "auto") || !strcmp(type, "none"))) type = 0;
  if (flags & MS_MOVE) {
    if (type) error_exit("--move with -t");
  } else if (!type) {
    struct stat stdev, stdir;

    // file on file or dir on dir is a --bind mount.
    if (!stat(dev, &stdev) && !stat(dir, &stdir)
        && ((S_ISREG(stdev.st_mode) && S_ISREG(stdir.st_mode))
            || (S_ISDIR(stdev.st_mode) && S_ISDIR(stdir.st_mode))))
    {
      flags ^= MS_REC;
      flags |= MS_BIND;
    } else fp = xfopen("/proc/filesystems", "r");
  } else if (!strcmp(type, "ignore")) return;
  else if (!strcmp(type, "swap"))
    toys.exitval |= xrun((char *[]){"swapon", "--", dev, 0});

  for (;;) {
    int fd = -1, ro = 0;

    // If type wasn't specified, try all of them in order.
    if (fp && !buf) {
      size_t i;

      if (getline(&buf, &i, fp)<1) {
        error_msg("%s: need -t", dev);
        break;
      }
      type = buf;
      // skip nodev devices
      if (!isspace(*type)) {
        free(buf);
        buf = 0;

        continue;
      }
      // trim whitespace
      while (isspace(*type)) type++;
      i = strlen(type);
      if (i) type[i-1] = 0;
    }
    if (FLAG(v)) printf("try '%s' type '%s' on '%s'\n", dev, type, dir);
    for (;;) {
      rc = mount(dev, dir, type, flags, opts);
      // Did we succeed, fail unrecoverably, or already try read-only?
      if (!rc || (errno != EACCES && errno != EROFS) || (flags&MS_RDONLY))
        break;
      // If we haven't already tried it, use the BLKROSET ioctl to ensure
      // that the underlying device isn't read-only.
      if (fd == -1) {
        if (FLAG(v))
          printf("trying BLKROSET ioctl on '%s'\n", dev);
        if (-1 != (fd = open(dev, O_RDONLY))) {
          rc = ioctl(fd, BLKROSET, &ro);
          close(fd);
          if (!rc) continue;
        }
      }
      fprintf(stderr, "'%s' is read-only\n", dev);
      flags |= MS_RDONLY;
    }

    // Trying to autodetect loop mounts like bind mounts above (file on dir)
    // isn't good enough because "mount -t ext2 fs.img dir" is valid, but if
    // you _do_ accept loop mounts with -t how do you tell "-t cifs" isn't
    // looking for a block device if it's not in /proc/filesystems yet
    // because the fs module won't be loaded until you try the mount, and
    // if you can't then DEVICE existing as a file would cause a false
    // positive loopback mount (so "touch servername" becomes a potential
    // denial of service attack...)
    //
    // Solution: try the mount, let the kernel tell us it wanted a block
    // device, then do the loopback setup and retry the mount.

    if (rc && errno == ENOTBLK) {
      char *losetup[] = {"losetup", (flags&MS_RDONLY)?"-fsr":"-fs", dev, 0};

      if ((dev = chomp(xrunread(losetup, 0)))) continue;
      error_msg("%s failed", *losetup);
      break;
    }

    free(buf);
    buf = 0;
    if (!rc) break;
    if (fp && (errno == EINVAL || errno == EBUSY)) continue;

    perror_msg("'%s'->'%s'", dev, dir);

    break;
  }
  if (fp) fclose(fp);
}

void mount_main(void)
{
  char *opts = 0, *dev = 0, *dir = 0, **ss;
  long flags = MS_SILENT;
  struct arg_list *o;
  struct mtab_list *mtl, *mtl2 = 0, *mm, *remount;

// TODO
// remount
//   - overmounts
// shared subtree
// -o parsed after fstab options
// test if mountpoint already exists (-o noremount?)

  // First pass; just accumulate string, don't parse flags yet. (This is so
  // we can modify fstab entries with -a, or mtab with remount.)
  for (o = TT.o; o; o = o->next) comma_collate(&opts, o->arg);
  if (FLAG(r)) comma_collate(&opts, "ro");
  if (FLAG(w)) comma_collate(&opts, "rw");
  if (FLAG(R)) comma_collate(&opts, "rbind");

  // Treat each --option as -o option
  for (ss = toys.optargs; *ss; ss++) {
    char *sss = *ss;

    // If you realy, really want to mount a file named "--", we support it.
    if (sss[0]=='-' && sss[1]=='-' && sss[2]) comma_collate(&opts, sss+2);
    else if (!dev) dev = sss;
    else if (!dir) dir = sss;
    // same message as lib/args.c ">2" which we can't use because --opts count
    else error_exit("Max 2 arguments\n");
  }

  if (FLAG(a) && dir) error_exit("-a with >1 arg");

  // For remount we need _last_ match (in case of overmounts), so traverse
  // in reverse order. (Yes I'm using remount as a boolean for a bit here,
  // the double cast is to get gcc to shut up about it.)
  remount = (void *)(long)comma_scan(opts, "remount", 0);
  if ((FLAG(a) && !access("/proc/mounts", R_OK)) || remount) {
    mm = dlist_terminate(mtl = mtl2 = xgetmountlist(0));
    if (remount) remount = mm;
  }

  // Do we need to do an /etc/fstab trawl?
  // This covers -a, -o remount, one argument, all user mounts
  if (FLAG(a) || (dev && (!dir || getuid() || remount))) {
    if (!remount) dlist_terminate(mtl = xgetmountlist("/etc/fstab"));

    for (mm = remount ? remount : mtl; mm; mm = (remount ? mm->prev : mm->next))
    {
      char *aopts = 0;
      struct mtab_list *mmm = 0;
      int aflags, noauto, len;

      // Check for noauto and get it out of the option list. (Unknown options
      // that make it to the kernel give filesystem drivers indigestion.)
      noauto = comma_scan(mm->opts, "noauto", 1);

      if (FLAG(a)) {
        // "mount -a /path" to mount all entries under /path
        if (dev) {
           len = strlen(dev);
           if (strncmp(dev, mm->dir, len)
               || (mm->dir[len] && mm->dir[len] != '/')) continue;
        } else if (noauto) continue; // never present in the remount case
        if (!mountlist_istype(mm, TT.t) || !comma_scanall(mm->opts, TT.O))
          continue;
      } else {
        if (dir && strcmp(dir, mm->dir)) continue;
        if (strcmp(dev, mm->device) && (dir || strcmp(dev, mm->dir))) continue;
      }

      // Don't overmount the same dev on the same directory
      // (Unless root explicitly says to in non -a mode.)
      if (mtl2 && !remount)
        for (mmm = mtl2; mmm; mmm = mmm->next)
          if (!strcmp(mm->dir, mmm->dir) && !strcmp(mm->device, mmm->device))
            break;

      // user only counts from fstab, not opts.
      if (!mmm) {
        TT.okuser = comma_scan(mm->opts, "user", 1);
        aflags = flag_opts(mm->opts, flags, &aopts);
        aflags = flag_opts(opts, aflags, &aopts);

        mount_filesystem(mm->device, mm->dir, mm->type, aflags, aopts);
      } // TODO else if (getuid()) error_msg("already there") ?
      free(aopts);

      if (!FLAG(a)) break;
    }
    if (CFG_TOYBOX_FREE) {
      llist_traverse(mtl, free);
      llist_traverse(mtl2, free);
    }
    if (!mm && !FLAG(a))
      error_exit("'%s' not in %s", dir ? dir : dev,
                 remount ? "/proc/mounts" : "fstab");

  // show mounts from /proc/mounts
  } else if (!dev) {
    for (mtl = xgetmountlist(0); mtl && (mm = dlist_pop(&mtl)); free(mm)) {
      char *s = mm->device, *ss = "", *temp;
      struct stat st;

      if (TT.t && strcmp(TT.t, mm->type)) continue;
      if (*s == '/') {
        if (!stat(mm->device, &st) && S_ISBLK(st.st_mode) &&
            dev_major(st.st_rdev)==7)
        {
          temp = xmprintf("/sys/block/loop%d/loop/backing_file",
            dev_minor(st.st_rdev));
          s = chomp(readfile(temp, 0, 0));
          free(temp);
          if (s) {
            ss = xmprintf(",file=%s"+!*mm->opts, s);
            free(s);
          };
        }
        s = xabspath(mm->device, 0);
      }
      xprintf("%s on %s type %s (%s%s)\n", s, mm->dir, mm->type, mm->opts, ss);
      if (s != mm->device) free(s);
      if (*ss) free(ss);
    }

  // two arguments
  } else {
    char *more = 0;

    flags = flag_opts(opts, flags, &more);
    mount_filesystem(dev, dir, TT.t, flags, more);
    if (CFG_TOYBOX_FREE) free(more);
  }
}
