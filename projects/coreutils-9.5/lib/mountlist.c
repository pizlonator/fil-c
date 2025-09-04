/* mountlist.c -- return a list of mounted file systems

   Copyright (C) 1991-1992, 1997-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include "mountlist.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "xalloc.h"

#include <errno.h>

#include <fcntl.h>

#include <unistd.h>

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#if MAJOR_IN_MKDEV
# include <sys/mkdev.h>
#elif MAJOR_IN_SYSMACROS
# include <sys/sysmacros.h>
#endif

#if defined MOUNTED_GETFSSTAT   /* OSF/1, also (obsolete) Apple Darwin 1.3 */
# if HAVE_SYS_UCRED_H
#  include <grp.h> /* needed on OSF V4.0 for definition of NGROUPS,
                      NGROUPS is used as an array dimension in ucred.h */
#  include <sys/ucred.h> /* needed by powerpc-apple-darwin1.3.7 */
# endif
# if HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
# if HAVE_SYS_FS_TYPES_H
#  include <sys/fs_types.h> /* needed by powerpc-apple-darwin1.3.7 */
# endif
# if HAVE_STRUCT_FSSTAT_F_FSTYPENAME
#  define FS_TYPE(Ent) ((Ent).f_fstypename)
# else
#  define FS_TYPE(Ent) mnt_names[(Ent).f_type]
# endif
#endif /* MOUNTED_GETFSSTAT */

#ifdef MOUNTED_GETMNTENT1       /* glibc, HP-UX, IRIX, Cygwin, Android,
                                   also (obsolete) 4.3BSD, SunOS */
# include <mntent.h>
# include <sys/types.h>
# if defined __ANDROID__        /* Android */
   /* Bionic versions from between 2014-01-09 and 2015-01-08 define MOUNTED to
      an incorrect value; older Bionic versions don't define it at all.  */
#  undef MOUNTED
#  define MOUNTED "/proc/mounts"
# elif !defined MOUNTED
#  if defined _PATH_MOUNTED     /* GNU libc  */
#   define MOUNTED _PATH_MOUNTED
#  endif
#  if defined MNT_MNTTAB        /* HP-UX.  */
#   define MOUNTED MNT_MNTTAB
#  endif
# endif
#endif

#ifdef MOUNTED_GETMNTINFO       /* Mac OS X, FreeBSD, OpenBSD, also (obsolete) 4.4BSD */
# include <sys/mount.h>
#endif

#ifdef MOUNTED_GETMNTINFO2      /* NetBSD, Minix */
# include <sys/statvfs.h>
#endif

#ifdef MOUNTED_FS_STAT_DEV      /* Haiku, also (obsolete) BeOS */
# include <fs_info.h>
# include <dirent.h>
#endif

#ifdef MOUNTED_FREAD_FSTYP      /* (obsolete) SVR3 */
# include <mnttab.h>
# include <sys/fstyp.h>
# include <sys/statfs.h>
#endif

#ifdef MOUNTED_GETEXTMNTENT     /* Solaris >= 8 */
# include <sys/mnttab.h>
#endif

#ifdef MOUNTED_GETMNTENT2       /* Solaris < 8, also (obsolete) SVR4 */
# include <sys/mnttab.h>
#endif

#ifdef MOUNTED_VMOUNT           /* AIX */
# include <fshelp.h>
# include <sys/vfs.h>
#endif

#ifdef MOUNTED_INTERIX_STATVFS  /* Interix */
# include <sys/statvfs.h>
# include <dirent.h>
#endif

#if HAVE_SYS_MNTENT_H
/* This is to get MNTOPT_IGNORE on e.g. SVR4.  */
# include <sys/mntent.h>
#endif

#ifdef MOUNTED_GETMNTENT1
# if !HAVE_SETMNTENT            /* Android <= 4.4 */
#  define setmntent(fp,mode) fopen (fp, mode "e")
# endif
# if !HAVE_ENDMNTENT            /* Android <= 4.4 */
#  define endmntent(fp) fclose (fp)
# endif
#endif

#ifndef HAVE_HASMNTOPT
# define hasmntopt(mnt, opt) ((char *) 0)
#endif

#undef MNT_IGNORE
#ifdef MNTOPT_IGNORE
# if defined __sun && defined __SVR4
/* Solaris defines hasmntopt(struct mnttab *, char *)
   while it is otherwise hasmntopt(struct mnttab *, const char *).  */
#  define MNT_IGNORE(M) hasmntopt (M, (char *) MNTOPT_IGNORE)
# else
#  define MNT_IGNORE(M) hasmntopt (M, MNTOPT_IGNORE)
# endif
#else
# define MNT_IGNORE(M) 0
#endif

/* Each of the FILE streams in this file is only used in a single thread.  */
#include "unlocked-io.h"

/* The results of opendir() in this file are not used with dirfd and fchdir,
   therefore save some unnecessary work in fchdir.c.  */
#ifdef GNULIB_defined_DIR
# undef DIR
# undef opendir
# undef closedir
# undef readdir
#else
# ifdef GNULIB_defined_opendir
#  undef opendir
# endif
# ifdef GNULIB_defined_closedir
#  undef closedir
# endif
#endif

#define ME_DUMMY_0(Fs_name, Fs_type)            \
  (strcmp (Fs_type, "autofs") == 0              \
   || strcmp (Fs_type, "proc") == 0             \
   || strcmp (Fs_type, "subfs") == 0            \
   /* for Linux 2.6/3.x */                      \
   || strcmp (Fs_type, "debugfs") == 0          \
   || strcmp (Fs_type, "devpts") == 0           \
   || strcmp (Fs_type, "fusectl") == 0          \
   || strcmp (Fs_type, "fuse.portal") == 0      \
   || strcmp (Fs_type, "mqueue") == 0           \
   || strcmp (Fs_type, "rpc_pipefs") == 0       \
   || strcmp (Fs_type, "sysfs") == 0            \
   /* FreeBSD, Linux 2.4 */                     \
   || strcmp (Fs_type, "devfs") == 0            \
   /* for NetBSD 3.0 */                         \
   || strcmp (Fs_type, "kernfs") == 0           \
   /* for Irix 6.5 */                           \
   || strcmp (Fs_type, "ignore") == 0)

/* Historically, we have marked as "dummy" any file system of type "none",
   but now that programs like du need to know about bind-mounted directories,
   we grant an exception to any with "bind" in its list of mount options.
   I.e., those are *not* dummy entries.  */
#ifdef MOUNTED_GETMNTENT1
# define ME_DUMMY(Fs_name, Fs_type, Bind) \
  (ME_DUMMY_0 (Fs_name, Fs_type) \
   || (strcmp (Fs_type, "none") == 0 && !Bind))
#else
# define ME_DUMMY(Fs_name, Fs_type) \
  (ME_DUMMY_0 (Fs_name, Fs_type) || strcmp (Fs_type, "none") == 0)
#endif

#ifdef __CYGWIN__
# include <windows.h>
/* Don't assume that UNICODE is not defined.  */
# undef GetDriveType
# define GetDriveType GetDriveTypeA
# define ME_REMOTE me_remote
/* All cygwin mount points include ':' or start with '//'; so it
   requires a native Windows call to determine remote disks.  */
static bool
me_remote (char const *fs_name, _GL_UNUSED char const *fs_type)
{
  if (fs_name[0] && fs_name[1] == ':')
    {
      char drive[4];
      sprintf (drive, "%c:\\", fs_name[0]);
      switch (GetDriveType (drive))
        {
        case DRIVE_REMOVABLE:
        case DRIVE_FIXED:
        case DRIVE_CDROM:
        case DRIVE_RAMDISK:
          return false;
        }
    }
  return true;
}
#endif

#ifndef ME_REMOTE
/* A file system is "remote" if its Fs_name contains a ':'
   or if (it is of type (smbfs or cifs) and its Fs_name starts with '//')
   or if it is of any other of the listed types
   or Fs_name is equal to "-hosts" (used by autofs to mount remote fs).
   "VM" file systems like prl_fs or vboxsf are not considered remote here. */
# define ME_REMOTE(Fs_name, Fs_type)            \
    (strchr (Fs_name, ':') != NULL              \
     || ((Fs_name)[0] == '/'                    \
         && (Fs_name)[1] == '/'                 \
         && (strcmp (Fs_type, "smbfs") == 0     \
             || strcmp (Fs_type, "smb3") == 0   \
             || strcmp (Fs_type, "cifs") == 0)) \
     || strcmp (Fs_type, "acfs") == 0           \
     || strcmp (Fs_type, "afs") == 0            \
     || strcmp (Fs_type, "coda") == 0           \
     || strcmp (Fs_type, "auristorfs") == 0     \
     || strcmp (Fs_type, "fhgfs") == 0          \
     || strcmp (Fs_type, "gpfs") == 0           \
     || strcmp (Fs_type, "ibrix") == 0          \
     || strcmp (Fs_type, "ocfs2") == 0          \
     || strcmp (Fs_type, "vxfs") == 0           \
     || strcmp ("-hosts", Fs_name) == 0)
#endif

#if MOUNTED_GETMNTINFO          /* Mac OS X, FreeBSD, OpenBSD, also (obsolete) 4.4BSD */

# if ! HAVE_STRUCT_STATFS_F_FSTYPENAME
static char *
fstype_to_string (short int t)
{
  switch (t)
    {
#  ifdef MOUNT_PC
    case MOUNT_PC:
      return "pc";
#  endif
#  ifdef MOUNT_MFS
    case MOUNT_MFS:
      return "mfs";
#  endif
#  ifdef MOUNT_LO
    case MOUNT_LO:
      return "lo";
#  endif
#  ifdef MOUNT_TFS
    case MOUNT_TFS:
      return "tfs";
#  endif
#  ifdef MOUNT_TMP
    case MOUNT_TMP:
      return "tmp";
#  endif
#  ifdef MOUNT_UFS
   case MOUNT_UFS:
     return "ufs" ;
#  endif
#  ifdef MOUNT_NFS
   case MOUNT_NFS:
     return "nfs" ;
#  endif
#  ifdef MOUNT_MSDOS
   case MOUNT_MSDOS:
     return "msdos" ;
#  endif
#  ifdef MOUNT_LFS
   case MOUNT_LFS:
     return "lfs" ;
#  endif
#  ifdef MOUNT_LOFS
   case MOUNT_LOFS:
     return "lofs" ;
#  endif
#  ifdef MOUNT_FDESC
   case MOUNT_FDESC:
     return "fdesc" ;
#  endif
#  ifdef MOUNT_PORTAL
   case MOUNT_PORTAL:
     return "portal" ;
#  endif
#  ifdef MOUNT_NULL
   case MOUNT_NULL:
     return "null" ;
#  endif
#  ifdef MOUNT_UMAP
   case MOUNT_UMAP:
     return "umap" ;
#  endif
#  ifdef MOUNT_KERNFS
   case MOUNT_KERNFS:
     return "kernfs" ;
#  endif
#  ifdef MOUNT_PROCFS
   case MOUNT_PROCFS:
     return "procfs" ;
#  endif
#  ifdef MOUNT_AFS
   case MOUNT_AFS:
     return "afs" ;
#  endif
#  ifdef MOUNT_CD9660
   case MOUNT_CD9660:
     return "cd9660" ;
#  endif
#  ifdef MOUNT_UNION
   case MOUNT_UNION:
     return "union" ;
#  endif
#  ifdef MOUNT_DEVFS
   case MOUNT_DEVFS:
     return "devfs" ;
#  endif
#  ifdef MOUNT_EXT2FS
   case MOUNT_EXT2FS:
     return "ext2fs" ;
#  endif
    default:
      return "?";
    }
}
# endif

static char *
fsp_to_string (const struct statfs *fsp)
{
# if HAVE_STRUCT_STATFS_F_FSTYPENAME
  return (char *) (fsp->f_fstypename);
# else
  return fstype_to_string (fsp->f_type);
# endif
}

#endif /* MOUNTED_GETMNTINFO */

#ifdef MOUNTED_VMOUNT           /* AIX */
static char *
fstype_to_string (int t)
{
  struct vfs_ent *e;

  e = getvfsbytype (t);
  if (!e || !e->vfsent_name)
    return "none";
  else
    return e->vfsent_name;
}
#endif /* MOUNTED_VMOUNT */


#if defined MOUNTED_GETMNTENT1 || defined MOUNTED_GETMNTENT2

/* Return the device number from MOUNT_OPTIONS, if possible.
   Otherwise return (dev_t) -1.  */
static dev_t
dev_from_mount_options (char const *mount_options)
{
  /* GNU/Linux allows file system implementations to define their own
     meaning for "dev=" mount options, so don't trust the meaning
     here.  */
# ifndef __linux__

  static char const dev_pattern[] = ",dev=";
  char const *devopt = strstr (mount_options, dev_pattern);

  if (devopt)
    {
      char const *optval = devopt + sizeof dev_pattern - 1;
      char *optvalend;
      unsigned long int dev;
      errno = 0;
      dev = strtoul (optval, &optvalend, 16);
      if (optval != optvalend
          && (*optvalend == '\0' || *optvalend == ',')
          && ! (dev == ULONG_MAX && errno == ERANGE)
          && dev == (dev_t) dev)
        return dev;
    }

# endif
  (void) mount_options;
  return -1;
}

#endif

#if defined MOUNTED_GETMNTENT1 && (defined __linux__ || defined __ANDROID__) /* GNU/Linux, Android */

/* Unescape the paths in mount tables.
   STR is updated in place.  */

static void
unescape_tab (char *str)
{
  size_t i, j = 0;
  size_t len = strlen (str) + 1;
  for (i = 0; i < len; i++)
    {
      if (str[i] == '\\' && (i + 4 < len)
          && str[i + 1] >= '0' && str[i + 1] <= '3'
          && str[i + 2] >= '0' && str[i + 2] <= '7'
          && str[i + 3] >= '0' && str[i + 3] <= '7')
        {
          str[j++] = (str[i + 1] - '0') * 64 +
                     (str[i + 2] - '0') * 8 +
                     (str[i + 3] - '0');
          i += 3;
        }
      else
        str[j++] = str[i];
    }
}

/* Find the next space in STR, terminate the string there in place,
   and return that position.  Otherwise return NULL.  */

static char *
terminate_at_blank (char *str)
{
  char *s = strchr (str, ' ');
  if (s)
    *s = '\0';
  return s;
}
#endif

/* Return a list of the currently mounted file systems, or NULL on error.
   Add each entry to the tail of the list so that they stay in order.
   If NEED_FS_TYPE is true, ensure that the file system type fields in
   the returned list are valid.  Otherwise, they might not be.  */

struct mount_entry *
read_file_system_list (bool need_fs_type)
{
  struct mount_entry *mount_list;
  struct mount_entry *me;
  struct mount_entry **mtail = &mount_list;
  (void) need_fs_type;

#ifdef MOUNTED_GETMNTENT1       /* glibc, HP-UX, IRIX, Cygwin, Android,
                                   also (obsolete) 4.3BSD, SunOS */
  {
    FILE *fp;

# if defined __linux__ || defined __ANDROID__
    /* Try parsing mountinfo first, as that make device IDs available.
       Note we could use libmount routines to simplify this parsing a little
       (and that code is in previous versions of this function), however
       libmount depends on libselinux which pulls in many dependencies.  */
    char const *mountinfo = "/proc/self/mountinfo";
    fp = fopen (mountinfo, "re");
    if (fp != NULL)
      {
        char *line = NULL;
        size_t buf_size = 0;

        while (getline (&line, &buf_size, fp) != -1)
          {
            unsigned int devmaj, devmin;
            int rc, mntroot_s;

            rc = sscanf(line, "%*u "        /* id - discarded  */
                              "%*u "        /* parent - discarded  */
                              "%u:%u "      /* dev major:minor  */
                              "%n",         /* mountroot (start)  */
                              &devmaj, &devmin,
                              &mntroot_s);

            if (rc != 2 && rc != 3)  /* 3 if %n included in count.  */
              continue;

            /* find end of MNTROOT.  */
            char *mntroot = line + mntroot_s;
            char *blank = terminate_at_blank (mntroot);
            if (! blank)
              continue;

            /* find end of TARGET.  */
            char *target = blank + 1;
            blank = terminate_at_blank (target);
            if (! blank)
              continue;

            /* skip optional fields, terminated by " - "  */
            char *dash = strstr (blank + 1, " - ");
            if (! dash)
              continue;

            /* advance past the " - " separator.  */
            char *fstype = dash + 3;
            blank = terminate_at_blank (fstype);
            if (! blank)
              continue;

            /* find end of SOURCE.  */
            char *source = blank + 1;
            if (! terminate_at_blank (source))
              continue;

            /* manipulate the sub-strings in place.  */
            unescape_tab (source);
            unescape_tab (target);
            unescape_tab (mntroot);
            unescape_tab (fstype);

            me = xmalloc (sizeof *me);

            me->me_devname = xstrdup (source);
            me->me_mountdir = xstrdup (target);
            me->me_mntroot = xstrdup (mntroot);
            me->me_type = xstrdup (fstype);
            me->me_type_malloced = 1;
            me->me_dev = makedev (devmaj, devmin);
            /* we pass "false" for the "Bind" option as that's only
               significant when the Fs_type is "none" which will not be
               the case when parsing "/proc/self/mountinfo", and only
               applies for static /etc/mtab files.  */
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type, false);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
          }

        free (line);

        if (ferror (fp))
          {
            int saved_errno = errno;
            fclose (fp);
            errno = saved_errno;
            goto free_then_fail;
          }

        if (fclose (fp) == EOF)
          goto free_then_fail;
      }
    else /* fallback to /proc/self/mounts (/etc/mtab).  */
# endif /* __linux __ || __ANDROID__ */
      {
        struct mntent *mnt;
        char const *table = MOUNTED;

        fp = setmntent (table, "r");
        if (fp == NULL)
          return NULL;

        while ((mnt = getmntent (fp)))
          {
            bool bind = hasmntopt (mnt, "bind");

            me = xmalloc (sizeof *me);
            me->me_devname = xstrdup (mnt->mnt_fsname);
            me->me_mountdir = xstrdup (mnt->mnt_dir);
            me->me_mntroot = NULL;
            me->me_type = xstrdup (mnt->mnt_type);
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type, bind);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = dev_from_mount_options (mnt->mnt_opts);

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
          }

        if (endmntent (fp) == 0)
          goto free_then_fail;
      }
  }
#endif /* MOUNTED_GETMNTENT1. */

#ifdef MOUNTED_GETMNTINFO       /* Mac OS X, FreeBSD, OpenBSD, also (obsolete) 4.4BSD */
  {
    struct statfs *fsp;
    int entries;

    entries = getmntinfo (&fsp, MNT_NOWAIT);
    if (entries < 0)
      return NULL;
    for (; entries-- > 0; fsp++)
      {
        char *fs_type = fsp_to_string (fsp);

        me = xmalloc (sizeof *me);
        me->me_devname = xstrdup (fsp->f_mntfromname);
        me->me_mountdir = xstrdup (fsp->f_mntonname);
        me->me_mntroot = NULL;
        me->me_type = fs_type;
        me->me_type_malloced = 0;
        me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
        me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
        me->me_dev = (dev_t) -1;        /* Magic; means not known yet. */

        /* Add to the linked list. */
        *mtail = me;
        mtail = &me->me_next;
      }
  }
#endif /* MOUNTED_GETMNTINFO */

#ifdef MOUNTED_GETMNTINFO2      /* NetBSD, Minix */
  {
    struct statvfs *fsp;
    int entries;

    entries = getmntinfo (&fsp, MNT_NOWAIT);
    if (entries < 0)
      return NULL;
    for (; entries-- > 0; fsp++)
      {
        me = xmalloc (sizeof *me);
        me->me_devname = xstrdup (fsp->f_mntfromname);
        me->me_mountdir = xstrdup (fsp->f_mntonname);
        me->me_mntroot = NULL;
        me->me_type = xstrdup (fsp->f_fstypename);
        me->me_type_malloced = 1;
        me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
        me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
        me->me_dev = (dev_t) -1;        /* Magic; means not known yet. */

        /* Add to the linked list. */
        *mtail = me;
        mtail = &me->me_next;
      }
  }
#endif /* MOUNTED_GETMNTINFO2 */

#if defined MOUNTED_FS_STAT_DEV /* Haiku, also (obsolete) BeOS */
  {
    /* The next_dev() and fs_stat_dev() system calls give the list of
       all file systems, including the information returned by statvfs()
       (fs type, total blocks, free blocks etc.), but without the mount
       point. But on BeOS all file systems except / are mounted in the
       rootfs, directly under /.
       The directory name of the mount point is often, but not always,
       identical to the volume name of the device.
       We therefore get the list of subdirectories of /, and the list
       of all file systems, and match the two lists.  */

    DIR *dirp;
    struct rootdir_entry
      {
        char *name;
        dev_t dev;
        ino_t ino;
        struct rootdir_entry *next;
      };
    struct rootdir_entry *rootdir_list;
    struct rootdir_entry **rootdir_tail;
    int32 pos;
    dev_t dev;
    fs_info fi;

    /* All volumes are mounted in the rootfs, directly under /. */
    rootdir_list = NULL;
    rootdir_tail = &rootdir_list;
    dirp = opendir ("/");
    if (dirp)
      {
        struct dirent *d;

        while ((d = readdir (dirp)) != NULL)
          {
            char *name;
            struct stat statbuf;

            if (strcmp (d->d_name, "..") == 0)
              continue;

            if (strcmp (d->d_name, ".") == 0)
              name = xstrdup ("/");
            else
              {
                name = xmalloc (1 + strlen (d->d_name) + 1);
                name[0] = '/';
                strcpy (name + 1, d->d_name);
              }

            if (lstat (name, &statbuf) >= 0 && S_ISDIR (statbuf.st_mode))
              {
                struct rootdir_entry *re = xmalloc (sizeof *re);
                re->name = name;
                re->dev = statbuf.st_dev;
                re->ino = statbuf.st_ino;

                /* Add to the linked list.  */
                *rootdir_tail = re;
                rootdir_tail = &re->next;
              }
            else
              free (name);
          }
        closedir (dirp);
      }
    *rootdir_tail = NULL;

    for (pos = 0; (dev = next_dev (&pos)) >= 0; )
      if (fs_stat_dev (dev, &fi) >= 0)
        {
          /* Note: fi.dev == dev. */
          struct rootdir_entry *re;

          for (re = rootdir_list; re; re = re->next)
            if (re->dev == fi.dev && re->ino == fi.root)
              break;

          me = xmalloc (sizeof *me);
          me->me_devname = xstrdup (fi.device_name[0] != '\0'
                                    ? fi.device_name : fi.fsh_name);
          me->me_mountdir = xstrdup (re != NULL ? re->name : fi.fsh_name);
          me->me_mntroot = NULL;
          me->me_type = xstrdup (fi.fsh_name);
          me->me_type_malloced = 1;
          me->me_dev = fi.dev;
          me->me_dummy = 0;
          me->me_remote = (fi.flags & B_FS_IS_SHARED) != 0;

          /* Add to the linked list. */
          *mtail = me;
          mtail = &me->me_next;
        }
    *mtail = NULL;

    while (rootdir_list != NULL)
      {
        struct rootdir_entry *re = rootdir_list;
        rootdir_list = re->next;
        free (re->name);
        free (re);
      }
  }
#endif /* MOUNTED_FS_STAT_DEV */

#if defined MOUNTED_GETFSSTAT   /* OSF/1, also (obsolete) Apple Darwin 1.3 */
  {
    int numsys, counter;
    size_t bufsize;
    struct statfs *stats;

    numsys = getfsstat (NULL, 0L, MNT_NOWAIT);
    if (numsys < 0)
      return NULL;
    if (SIZE_MAX / sizeof *stats <= numsys)
      xalloc_die ();

    bufsize = (1 + numsys) * sizeof *stats;
    stats = xmalloc (bufsize);
    numsys = getfsstat (stats, bufsize, MNT_NOWAIT);

    if (numsys < 0)
      {
        free (stats);
        return NULL;
      }

    for (counter = 0; counter < numsys; counter++)
      {
        me = xmalloc (sizeof *me);
        me->me_devname = xstrdup (stats[counter].f_mntfromname);
        me->me_mountdir = xstrdup (stats[counter].f_mntonname);
        me->me_mntroot = NULL;
        me->me_type = xstrdup (FS_TYPE (stats[counter]));
        me->me_type_malloced = 1;
        me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
        me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
        me->me_dev = (dev_t) -1;        /* Magic; means not known yet. */

        /* Add to the linked list. */
        *mtail = me;
        mtail = &me->me_next;
      }

    free (stats);
  }
#endif /* MOUNTED_GETFSSTAT */

#if defined MOUNTED_FREAD_FSTYP /* (obsolete) SVR3 */
  {
    struct mnttab mnt;
    char *table = "/etc/mnttab";
    FILE *fp;

    fp = fopen (table, "re");
    if (fp == NULL)
      return NULL;

    while (fread (&mnt, sizeof mnt, 1, fp) > 0)
      {
        me = xmalloc (sizeof *me);
        me->me_devname = xstrdup (mnt.mt_dev);
        me->me_mountdir = xstrdup (mnt.mt_filsys);
        me->me_mntroot = NULL;
        me->me_dev = (dev_t) -1;        /* Magic; means not known yet. */
        me->me_type = "";
        me->me_type_malloced = 0;
        if (need_fs_type)
          {
            struct statfs fsd;
            char typebuf[FSTYPSZ];

            if (statfs (me->me_mountdir, &fsd, sizeof fsd, 0) != -1
                && sysfs (GETFSTYP, fsd.f_fstyp, typebuf) != -1)
              {
                me->me_type = xstrdup (typebuf);
                me->me_type_malloced = 1;
              }
          }
        me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
        me->me_remote = ME_REMOTE (me->me_devname, me->me_type);

        /* Add to the linked list. */
        *mtail = me;
        mtail = &me->me_next;
      }

    if (ferror (fp))
      {
        /* The last fread() call must have failed.  */
        int saved_errno = errno;
        fclose (fp);
        errno = saved_errno;
        goto free_then_fail;
      }

    if (fclose (fp) == EOF)
      goto free_then_fail;
  }
#endif /* MOUNTED_FREAD_FSTYP.  */

#ifdef MOUNTED_GETEXTMNTENT     /* Solaris >= 8 */
  {
    struct extmnttab mnt;
    const char *table = MNTTAB;
    FILE *fp;
    int ret;

    /* No locking is needed, because the contents of /etc/mnttab is generated
       by the kernel.  */

    errno = 0;
    fp = fopen (table, "re");
    if (fp == NULL)
      ret = errno;
    else
      {
        while ((ret = getextmntent (fp, &mnt, 1)) == 0)
          {
            me = xmalloc (sizeof *me);
            me->me_devname = xstrdup (mnt.mnt_special);
            me->me_mountdir = xstrdup (mnt.mnt_mountp);
            me->me_mntroot = NULL;
            me->me_type = xstrdup (mnt.mnt_fstype);
            me->me_type_malloced = 1;
            me->me_dummy = MNT_IGNORE (&mnt) != 0;
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = makedev (mnt.mnt_major, mnt.mnt_minor);

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
          }

        ret = fclose (fp) == EOF ? errno : 0 < ret ? 0 : -1;
        /* Here ret = -1 means success, ret >= 0 means failure.  */
      }

    if (0 <= ret)
      {
        errno = ret;
        goto free_then_fail;
      }
  }
#endif /* MOUNTED_GETEXTMNTENT */

#ifdef MOUNTED_GETMNTENT2       /* Solaris < 8, also (obsolete) SVR4 */
  {
    struct mnttab mnt;
    const char *table = MNTTAB;
    FILE *fp;
    int ret;
    int lockfd = -1;

# if defined F_RDLCK && defined F_SETLKW
    /* MNTTAB_LOCK is a macro name of our own invention; it's not present in
       e.g. Solaris 2.6.  If the SVR4 folks ever define a macro
       for this file name, we should use their macro name instead.
       (Why not just lock MNTTAB directly?  We don't know.)  */
#  ifndef MNTTAB_LOCK
#   define MNTTAB_LOCK "/etc/.mnttab.lock"
#  endif
    lockfd = open (MNTTAB_LOCK, O_RDONLY | O_CLOEXEC);
    if (0 <= lockfd)
      {
        struct flock flock;
        flock.l_type = F_RDLCK;
        flock.l_whence = SEEK_SET;
        flock.l_start = 0;
        flock.l_len = 0;
        while (fcntl (lockfd, F_SETLKW, &flock) == -1)
          if (errno != EINTR)
            {
              int saved_errno = errno;
              close (lockfd);
              errno = saved_errno;
              return NULL;
            }
      }
    else if (errno != ENOENT)
      return NULL;
# endif

    errno = 0;
    fp = fopen (table, "re");
    if (fp == NULL)
      ret = errno;
    else
      {
        while ((ret = getmntent (fp, &mnt)) == 0)
          {
            me = xmalloc (sizeof *me);
            me->me_devname = xstrdup (mnt.mnt_special);
            me->me_mountdir = xstrdup (mnt.mnt_mountp);
            me->me_mntroot = NULL;
            me->me_type = xstrdup (mnt.mnt_fstype);
            me->me_type_malloced = 1;
            me->me_dummy = MNT_IGNORE (&mnt) != 0;
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = dev_from_mount_options (mnt.mnt_mntopts);

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
          }

        ret = fclose (fp) == EOF ? errno : 0 < ret ? 0 : -1;
        /* Here ret = -1 means success, ret >= 0 means failure.  */
      }

    if (0 <= lockfd && close (lockfd) != 0)
      ret = errno;

    if (0 <= ret)
      {
        errno = ret;
        goto free_then_fail;
      }
  }
#endif /* MOUNTED_GETMNTENT2.  */

#ifdef MOUNTED_VMOUNT           /* AIX */
  {
    int bufsize;
    void *entries;
    char *thisent;
    struct vmount *vmp;
    int n_entries;
    int i;

    /* Ask how many bytes to allocate for the mounted file system info.  */
    entries = &bufsize;
    if (mntctl (MCTL_QUERY, sizeof bufsize, entries) != 0)
      return NULL;
    entries = xmalloc (bufsize);

    /* Get the list of mounted file systems.  */
    n_entries = mntctl (MCTL_QUERY, bufsize, entries);
    if (n_entries < 0)
      {
        free (entries);
        return NULL;
      }

    for (i = 0, thisent = entries;
         i < n_entries;
         i++, thisent += vmp->vmt_length)
      {
        char *options, *ignore;

        vmp = (struct vmount *) thisent;
        me = xmalloc (sizeof *me);
        if (vmp->vmt_flags & MNT_REMOTE)
          {
            char *host, *dir;

            me->me_remote = 1;
            /* Prepend the remote dirname.  */
            host = thisent + vmp->vmt_data[VMT_HOSTNAME].vmt_off;
            dir = thisent + vmp->vmt_data[VMT_OBJECT].vmt_off;
            me->me_devname = xmalloc (strlen (host) + strlen (dir) + 2);
            strcpy (me->me_devname, host);
            strcat (me->me_devname, ":");
            strcat (me->me_devname, dir);
          }
        else
          {
            me->me_remote = 0;
            me->me_devname = xstrdup (thisent +
                                      vmp->vmt_data[VMT_OBJECT].vmt_off);
          }
        me->me_mountdir = xstrdup (thisent + vmp->vmt_data[VMT_STUB].vmt_off);
        me->me_mntroot = NULL;
        me->me_type = xstrdup (fstype_to_string (vmp->vmt_gfstype));
        me->me_type_malloced = 1;
        options = thisent + vmp->vmt_data[VMT_ARGS].vmt_off;
        ignore = strstr (options, "ignore");
        me->me_dummy = (ignore
                        && (ignore == options || ignore[-1] == ',')
                        && (ignore[sizeof "ignore" - 1] == ','
                            || ignore[sizeof "ignore" - 1] == '\0'));
        me->me_dev = (dev_t) -1; /* vmt_fsid might be the info we want.  */

        /* Add to the linked list. */
        *mtail = me;
        mtail = &me->me_next;
      }
    free (entries);
  }
#endif /* MOUNTED_VMOUNT. */

#ifdef MOUNTED_INTERIX_STATVFS  /* Interix */
  {
    DIR *dirp = opendir ("/dev/fs");
    char node[9 + NAME_MAX];

    if (!dirp)
      goto free_then_fail;

    while (1)
      {
        struct statvfs dev;
        struct dirent entry;
        struct dirent *result;

        /* FIXME: readdir_r is planned to be withdrawn from POSIX and
           marked obsolescent in glibc.  Use readdir instead.  */
        if (readdir_r (dirp, &entry, &result) || result == NULL)
          break;

        strcpy (node, "/dev/fs/");
        strcat (node, entry.d_name);

        if (statvfs (node, &dev) == 0)
          {
            me = xmalloc (sizeof *me);
            me->me_devname = xstrdup (dev.f_mntfromname);
            me->me_mountdir = xstrdup (dev.f_mntonname);
            me->me_mntroot = NULL;
            me->me_type = xstrdup (dev.f_fstypename);
            me->me_type_malloced = 1;
            me->me_dummy = ME_DUMMY (me->me_devname, me->me_type);
            me->me_remote = ME_REMOTE (me->me_devname, me->me_type);
            me->me_dev = (dev_t) -1;        /* Magic; means not known yet. */

            /* Add to the linked list. */
            *mtail = me;
            mtail = &me->me_next;
          }
      }
    closedir (dirp);
  }
#endif /* MOUNTED_INTERIX_STATVFS */

  *mtail = NULL;
  return mount_list;


 free_then_fail: _GL_UNUSED_LABEL;
  {
    int saved_errno = errno;
    *mtail = NULL;

    while (mount_list)
      {
        me = mount_list->me_next;
        free_mount_entry (mount_list);
        mount_list = me;
      }

    errno = saved_errno;
    return NULL;
  }
}

/* Free a mount entry as returned from read_file_system_list ().  */

void
free_mount_entry (struct mount_entry *me)
{
  free (me->me_devname);
  free (me->me_mountdir);
  free (me->me_mntroot);
  if (me->me_type_malloced)
    free (me->me_type);
  free (me);
}
