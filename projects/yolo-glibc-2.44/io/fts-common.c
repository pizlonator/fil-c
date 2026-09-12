/* Traverse a file hierarchy.

   Copyright (C) 2004-2026 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/*-
 * Copyright (c) 1990, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#if defined LIBC_SCCS && !defined GCC_LINT && !defined lint
static char sccsid[] = "@(#)fts.c       8.6 (Berkeley) 8/14/94";
#endif

#if _LIBC
# include <fts.h>
#else
# include "fts_.h"
#endif
#if _LIBC || HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Support for the LFS API version.  */
#ifndef FTS_OPEN
# define FTS_OPEN fts_open
# define FTS_CLOSE fts_close
# define FTS_READ fts_read
# define FTS_SET fts_set
# define FTS_CHILDREN fts_children
# define FTSOBJ FTS
# define FTSENTRY FTSENT
# define INO_T ino_t
# define STRUCT_STAT stat
# define FSTAT __fstat
# define FSTATAT __fstatat
# define STRUCT_STATFS statfs
# define FSTATFS __fstatfs
#endif

#if ! _LIBC
# include "attribute.h"
# include "fcntl--.h"
# include "openat.h"
# include "opendirat.h"
# include "same-inode.h"
# define OPENDIRAT opendirat
# define FTSENT_WRAPPER(__p) __p
# define FTS_COMPAR_CAST(__fn) __fn
#else
# include <stdbool.h>

# define internal_function
# define FALLTHROUGH               ; [[fallthrough]]
# define HAVE_STRUCT_DIRENT_D_TYPE 1
# define GNULIB_FTS_DEBUG          0
# ifdef O_PATH
#  define O_SEARCH                 O_PATH
# else
#  define O_SEARCH                 O_RDONLY
# endif
# define HAVE_SYS_VFS_H            1
# define HAVE_FSTATFS              1
# define HAVE_STRUCT_STATFS_F_TYPE 1
# define HAVE_OPENAT               1
# define HAVE_WORKING_O_NOFOLLOW   1
# define _GL_CMP(a, b)             ((a) < (b) ? -1 : (a) > (b))
# define OPENDIRAT                 __opendirat

static inline bool openat_needs_fchdir (void)
{
  return false;
}

static inline bool streq (const char *s1, const char *s2)
{
  return strcmp (s1, s2) == 0;
}
# define reallocarray              __libc_reallocarray
# define fchdir                    __fchdir
# define close                     __close
# define closedir                  __closedir
# define fcntl                     __fcntl
# define readdir                   __readdir
# ifndef dirfd
#  define dirfd                    __dirfd
# endif
# define open                      __open
# define openat                    __openat

# include "cycle-check.c"
# include "i-ring.c"

struct FTSENT_wrapper
{
  FTSOBJ *fts_fts;                /* the file hierarchy itself */
  DIR *fts_dirp;                  /* Dir pointer for any directory containing
				     more entries than we read at one time.  */
  struct STRUCT_STAT fts_stat;

  FTSENTRY ent;
};

/* glibc historicaly defines the FTS::fts_compar as having 'void *', while the
   fts_open has a function point using 'FTSENT **' as argument.  */
# define FTS_COMPAR_CAST(__fn) ((int (*) (void const *, void const *))__fn)

# define FTSENT_WRAPPER(p) \
    ((struct FTSENT_wrapper *) ((char *)(p) - offsetof(struct FTSENT_wrapper, ent)))
#endif
#define FTSENT_FTS(p)   (FTSENT_WRAPPER(p)->fts_fts)
#define FTSENT_DIRP(p)  (FTSENT_WRAPPER(p)->fts_dirp)

#include "flexmember.h"

#include <dirent.h>
#ifndef _D_EXACT_NAMLEN
# define _D_EXACT_NAMLEN(dirent) strlen ((dirent)->d_name)
#endif

#if HAVE_STRUCT_DIRENT_D_TYPE
/* True if the type of the directory entry D is known.  */
# define DT_IS_KNOWN(d) ((d)->d_type != DT_UNKNOWN)
/* True if the type of the directory entry D must be T.  */
# define DT_MUST_BE(d, t) ((d)->d_type == (t))
# define D_TYPE(d) ((d)->d_type)
#else
# define DT_IS_KNOWN(d) false
# define DT_MUST_BE(d, t) false
# define D_TYPE(d) DT_UNKNOWN

# undef DT_UNKNOWN
# define DT_UNKNOWN 0

/* Any nonzero values will do here, so long as they're distinct.
   Undef any existing macros out of the way.  */
# undef DT_BLK
# undef DT_CHR
# undef DT_DIR
# undef DT_FIFO
# undef DT_LNK
# undef DT_REG
# undef DT_SOCK
# define DT_BLK 1
# define DT_CHR 2
# define DT_DIR 3
# define DT_FIFO 4
# define DT_LNK 5
# define DT_REG 6
# define DT_SOCK 7
#endif

#ifndef S_IFBLK
# define S_IFBLK 0
#endif
#ifndef S_IFLNK
# define S_IFLNK 0
#endif
#ifndef S_IFSOCK
# define S_IFSOCK 0
#endif

enum
{
  NOT_AN_INODE_NUMBER = 0
};

#ifdef D_INO_IN_DIRENT
# define D_INO(dp) (dp)->d_ino
#else
/* Some systems don't have inodes, so fake them to avoid lots of ifdefs.  */
# define D_INO(dp) NOT_AN_INODE_NUMBER
#endif

/* If possible (see max_entries, below), read no more than this many directory
   entries at a time.  Without this limit (i.e., when using non-NULL
   fts_compar), processing a directory with 4,000,000 entries requires ~1GiB
   of memory, and handling 64M entries would require 16GiB of memory.  */
#ifndef FTS_MAX_READDIR_ENTRIES
# define FTS_MAX_READDIR_ENTRIES 100000
#endif

/* If there are more than this many entries in a directory,
   and the conditions mentioned below are satisfied, then sort
   the entries on inode number before any further processing.  */
#ifndef FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD
# define FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD 10000
#endif

enum
{
  _FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD = FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD
};

enum Fts_stat
{
  FTS_NO_STAT_REQUIRED = 1,
  FTS_STAT_REQUIRED = 2
};

#ifndef __set_errno
# define __set_errno(Val) errno = (Val)
#endif

/* If this host provides the openat function, then we can avoid
   attempting to open "." in some initialization code below.  */
#ifdef HAVE_OPENAT
# define HAVE_OPENAT_SUPPORT 1
#else
# define HAVE_OPENAT_SUPPORT 0
#endif

#ifdef NDEBUG
# define fts_assert(expr) ((void) (0 && (expr)))
#else
# define fts_assert(expr)       \
    do                          \
      {                         \
        if (!(expr))            \
          abort ();             \
      }                         \
    while (false)
#endif

static FTSENTRY  *fts_alloc (FTSOBJ *, const char *, size_t);
static FTSENTRY  *fts_build (FTSOBJ *, int);
static void      fts_lfree (FTSENTRY *);
static void      fts_load (FTSOBJ *, FTSENTRY *);
static size_t    fts_maxarglen (char * const *);
static void      fts_padjust (FTSOBJ *, FTSENTRY *);
static bool      fts_palloc (FTSOBJ *, size_t);
static FTSENTRY  *fts_sort (FTSOBJ *, FTSENTRY *, size_t);
static unsigned short int fts_stat (FTSOBJ *, FTSENTRY *, bool);
static int      fts_safe_changedir (FTSOBJ *, FTSENTRY *, int, const char *);

#include "fts-cycle.c"

#ifndef MAX
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

#define ISDOT(a)        (a[0] == '.' && (!a[1] || (a[1] == '.' && !a[2])))

#define CLR(opt)        (sp->fts_options &= ~(opt))
#define ISSET(opt)      ((sp->fts_options & (opt)) != 0)
#define SET(opt)        (sp->fts_options |= (opt))

/* FIXME: FTS_NOCHDIR is now misnamed.
   Call it FTS_USE_FULL_RELATIVE_FILE_NAMES instead. */
#define FCHDIR(sp, fd)                                  \
  (!ISSET(FTS_NOCHDIR) && (ISSET(FTS_CWDFD)             \
                           ? (cwd_advance_fd ((sp), (fd), true), 0) \
                           : fchdir (fd)))


/* fts_build flags */
/* FIXME: make this an enum */
#define BCHILD          1               /* fts_children */
#define BNAMES          2               /* fts_children, names only */
#define BREAD           3               /* fts_read */

#if GNULIB_FTS_DEBUG
# include <inttypes.h>
# include <stdio.h>
bool fts_debug = false;
# define Dprintf(x) do { if (fts_debug) printf x; } while (false)
static void fd_ring_check (FTSOBJ const *);
static void fd_ring_print (FTSOBJ const *, FILE *, char const *);
#else
# define Dprintf(x)
# define fd_ring_check(x)
# define fd_ring_print(a, b, c)
#endif

#define LEAVE_DIR(Fts, Ent, Tag)                                \
  do                                                            \
    {                                                           \
      Dprintf (("  %s-leaving: %s\n", Tag, (Ent)->fts_path));   \
      leave_dir (Fts, Ent);                                     \
      fd_ring_check (Fts);                                      \
    }                                                           \
  while (false)

static void
fd_ring_clear (I_ring *fd_ring)
{
  while ( ! i_ring_empty (fd_ring))
    {
      int fd = i_ring_pop (fd_ring);
      if (0 <= fd)
        close (fd);
    }
}

/* Overload the fts_statp->st_size member (otherwise unused, when
   fts_info is FTS_NSOK) to indicate whether fts_read should stat
   this entry or not.  */
static void
fts_set_stat_required (FTSENTRY *p, bool required)
{
  fts_assert (p->fts_info == FTS_NSOK);
  p->fts_statp->st_size = (required
                           ? FTS_STAT_REQUIRED
                           : FTS_NO_STAT_REQUIRED);
}

/* Virtual fchdir.  Advance SP's working directory file descriptor,
   SP->fts_cwd_fd, to FD, and push the previous value onto the fd_ring.
   CHDIR_DOWN_ONE is true if FD corresponds to an entry in the directory
   open on sp->fts_cwd_fd; i.e., to move the working directory one level
   down.  */
static void
internal_function
cwd_advance_fd (FTSOBJ *sp, int fd, bool chdir_down_one)
{
  int old = sp->fts_cwd_fd;
  fts_assert (old != fd || old == AT_FDCWD);

  if (chdir_down_one)
    {
      /* Push "old" onto the ring.
         If the displaced file descriptor is non-negative, close it.  */
      int prev_fd_in_slot = i_ring_push (&sp->fts_fd_ring, old);
      fd_ring_print (sp, stderr, "post-push");
      if (0 <= prev_fd_in_slot)
        close (prev_fd_in_slot); /* ignore any close failure */
    }
  else if ( ! ISSET (FTS_NOCHDIR))
    {
      if (0 <= old)
        close (old); /* ignore any close failure */
    }

  sp->fts_cwd_fd = fd;
}

/* Restore the initial, pre-traversal, "working directory".
   In FTS_CWDFD mode, we merely call cwd_advance_fd, otherwise,
   we may actually change the working directory.
   Return 0 upon success. Upon failure, set errno and return nonzero.  */
static int
restore_initial_cwd (FTSOBJ *sp)
{
  int fail = FCHDIR (sp, ISSET (FTS_CWDFD) ? AT_FDCWD : sp->fts_rfd);
  fd_ring_clear (&(sp->fts_fd_ring));
  return fail;
}

/* Open the directory DIR if possible, and return a file
   descriptor.  Return -1 and set errno on failure.  It doesn't matter
   whether the file descriptor has read or write access.  */

static int
internal_function
diropen (FTSOBJ const *sp, char const *dir)
{
  int open_flags = (O_SEARCH | O_CLOEXEC | O_DIRECTORY | O_NOCTTY | O_NONBLOCK
                    | (ISSET (FTS_PHYSICAL) ? O_NOFOLLOW : 0));

  int fd = (ISSET (FTS_CWDFD)
            ? openat (sp->fts_cwd_fd, dir, open_flags)
            : open (dir, open_flags));
  return fd;
}

FTSOBJ *
FTS_OPEN (char * const *argv,
          register int options,
          int (*compar) (const FTSENTRY **, const FTSENTRY **))
{
        /* Options check: glibc added other flags after FTS_NAMEONLY and
	   FTS_STOP, and they are assumed to be private.  */
        if (options & ~FTS_OPTIONMASK
#if _LIBC
	    || options & (FTS_NAMEONLY | FTS_STOP)
#endif
	    ) {
                __set_errno (EINVAL);
                return (NULL);
        }
        if ((options & FTS_NOCHDIR) && (options & FTS_CWDFD)) {
                __set_errno (EINVAL);
                return (NULL);
        }
#if !_LIBC
        if ( ! (options & (FTS_LOGICAL | FTS_PHYSICAL))) {
                __set_errno (EINVAL);
                return (NULL);
        }
#else
	/* glibc historically falls to FTS_PHYSICAL if no FTS_PHYSICAL or
	   FTS_LOGICAL is specified.  */
        if (! (options & (FTS_PHYSICAL | FTS_LOGICAL)))
	  options |= FTS_PHYSICAL;
#endif

        /* Allocate/initialize the stream */
        register FTSOBJ *sp = calloc (1, sizeof *sp);
        if (sp == NULL)
                return (NULL);
        sp->fts_compar = FTS_COMPAR_CAST(compar);
        sp->fts_options = options;

        /* Logical walks turn on NOCHDIR; symbolic links are too hard. */
        if (ISSET(FTS_LOGICAL)) {
                SET(FTS_NOCHDIR);
                CLR(FTS_CWDFD);
        }

        /* Initialize fts_cwd_fd.  */
        sp->fts_cwd_fd = AT_FDCWD;
        if ( ISSET(FTS_CWDFD) && ! HAVE_OPENAT_SUPPORT)
          {
            /* While it isn't technically necessary to open "." this
               early, doing it here saves us the trouble of ensuring
               later (where it'd be messier) that "." can in fact
               be opened.  If not, revert to FTS_NOCHDIR mode.  */
            int fd = open (".", O_SEARCH | O_CLOEXEC);
            if (fd < 0)
              {
                /* Even if "." is unreadable, don't revert to FTS_NOCHDIR mode
                   on systems like Linux+PROC_FS, where our openat emulation
                   is good enough.  Note: on a system that emulates
                   openat via /proc, this technique can still fail, but
                   only in extreme conditions, e.g., when the working
                   directory cannot be saved (i.e. save_cwd fails) --
                   and that happens on Linux only when "." is unreadable
                   and the CWD would be longer than PATH_MAX.
                   FIXME: once Linux kernel openat support is well established,
                   replace the above open call and this entire if/else block
                   with the body of the if-block below.  */
                if ( openat_needs_fchdir ())
                  {
                    SET(FTS_NOCHDIR);
                    CLR(FTS_CWDFD);
                  }
              }
            else
              {
                close (fd);
              }
          }

        /*
         * Start out with 1K of file name space, and enough, in any case,
         * to hold the user's file names.
         */
#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif
        {
          size_t maxarglen = fts_maxarglen(argv);
          if (! fts_palloc(sp, MAX(maxarglen, MAXPATHLEN)))
                  goto mem1;
        }

        /* Allocate/initialize root's parent. */
        FTSENTRY *parent = NULL;
        if (*argv != NULL) {
                if ((parent = fts_alloc(sp, "", 0)) == NULL)
                        goto mem2;
                parent->fts_level = FTS_ROOTPARENTLEVEL;
          }

        /* The classic fts implementation would call fts_stat with
           a new entry for each iteration of the loop below.
           If the comparison function is not specified or if the
           FTS_DEFER_STAT option is in effect, don't stat any entry
           in this loop.  This is an attempt to minimize the interval
           between the initial stat/lstat/fstatat and the point at which
           a directory argument is first opened.  This matters for any
           directory command line argument that resides on a file system
           without genuine i-nodes.  If you specify FTS_DEFER_STAT along
           with a comparison function, that function must not access any
           data via the fts_statp pointer.  */
        bool defer_stat = (compar == NULL || ISSET(FTS_DEFER_STAT));

        /* Allocate/initialize root(s). */
        register FTSENTRY *root;
        register size_t nitems;
        FTSENTRY *tmp = NULL;     /* pacify gcc */
        for (root = NULL, nitems = 0; *argv != NULL; ++argv, ++nitems) {
                /* *Do* allow zero-length file names. */
                size_t len = strlen(*argv);

                if ( ! (options & FTS_VERBATIM))
                  {
                    /* If there are two or more trailing slashes, trim all but one,
                       but don't change "//" to "/", and do map "///" to "/".  */
                    char const *v = *argv;
                    if (2 < len && v[len - 1] == '/')
                      while (1 < len && v[len - 2] == '/')
                        --len;
                  }

                register FTSENTRY *p = fts_alloc(sp, *argv, len);
                if (p == NULL)
                        goto mem3;
                p->fts_level = FTS_ROOTLEVEL;
                p->fts_parent = parent;
                p->fts_accpath = p->fts_name;
                /* Even when defer_stat is true, be sure to stat the first
                   command line argument, since fts_read (at least with
                   FTS_XDEV) requires that.  */
                if (defer_stat && root != NULL) {
                        p->fts_info = FTS_NSOK;
                        fts_set_stat_required(p, true);
                } else {
                        p->fts_info = fts_stat(sp, p, false);
                }

                /*
                 * If comparison routine supplied, traverse in sorted
                 * order; otherwise traverse in the order specified.
                 */
                if (compar) {
                        p->fts_link = root;
                        root = p;
                } else {
                        p->fts_link = NULL;
                        if (root == NULL)
                                tmp = root = p;
                        else {
                                tmp->fts_link = p;
                                tmp = p;
                        }
                }
        }
        if (compar && nitems > 1)
                root = fts_sort(sp, root, nitems);

        /*
         * Allocate a dummy pointer and make fts_read think that we've just
         * finished the node before the root(s); set p->fts_info to FTS_INIT
         * so that everything about the "current" node is ignored.
         */
        if ((sp->fts_cur = fts_alloc(sp, "", 0)) == NULL)
                goto mem3;
        sp->fts_cur->fts_link = root;
        sp->fts_cur->fts_info = FTS_INIT;
        sp->fts_cur->fts_level = 1;
        if (! setup_dir (sp))
                goto mem3;

        /*
         * If using chdir(2), grab a file descriptor pointing to dot to ensure
         * that we can get back here; this could be avoided for some file names,
         * but almost certainly not worth the effort.  Slashes, symbolic links,
         * and ".." are all fairly nasty problems.  Note, if we can't get the
         * descriptor we run anyway, just more slowly.
         */
        if (!ISSET(FTS_NOCHDIR) && !ISSET(FTS_CWDFD)
            && (sp->fts_rfd = diropen (sp, ".")) < 0)
                SET(FTS_NOCHDIR);

        i_ring_init (&sp->fts_fd_ring, -1);
        return (sp);

mem3:   fts_lfree(root);
        free(FTSENT_WRAPPER(parent));
mem2:   free(sp->fts_path);
mem1:   free(sp);
        return (NULL);
}

static void
internal_function
fts_load (FTSOBJ *sp, register FTSENTRY *p)
{
        /*
         * Load the stream structure for the next traversal.  Since we don't
         * actually enter the directory until after the preorder visit, set
         * the fts_accpath field specially so the chdir gets done to the right
         * place and the user can access the first node.  From fts_open it's
         * known that the file name will fit.
         */
        register size_t len = p->fts_pathlen = p->fts_namelen;
        memmove(sp->fts_path, p->fts_name, len + 1);
        register char *cp = strrchr(p->fts_name, '/');
        if (cp && (cp != p->fts_name || cp[1])) {
                len = strlen(++cp);
                memmove(p->fts_name, cp, len + 1);
                p->fts_namelen = len;
        }
        p->fts_accpath = p->fts_path = sp->fts_path;
}

int
FTS_CLOSE (FTSOBJ *sp)
{
        /*
         * This still works if we haven't read anything -- the dummy structure
         * points to the root list, so we step through to the end of the root
         * list which has a valid parent pointer.
         */
        if (sp->fts_cur) {
                register FTSENTRY *p;
                for (p = sp->fts_cur; p->fts_level >= FTS_ROOTLEVEL;) {
                        register FTSENTRY *freep = p;
                        p = p->fts_link != NULL ? p->fts_link : p->fts_parent;
                        free(FTSENT_WRAPPER(freep));
                }
                free(FTSENT_WRAPPER(p));
        }

        /* Free up child linked list, sort array, file name buffer. */
        if (sp->fts_child)
                fts_lfree(sp->fts_child);
        free(sp->fts_array);
        free(sp->fts_path);

        int saved_errno = 0;
        if (ISSET(FTS_CWDFD))
          {
            if (0 <= sp->fts_cwd_fd)
              if (close (sp->fts_cwd_fd))
                saved_errno = errno;
          }
        else if (!ISSET(FTS_NOCHDIR))
          {
            /* Return to original directory, save errno if necessary. */
            if (fchdir(sp->fts_rfd))
              saved_errno = errno;

            /* If close fails, record errno only if saved_errno is zero,
               so that we report the probably-more-meaningful fchdir errno.  */
            if (close (sp->fts_rfd))
              if (saved_errno == 0)
                saved_errno = errno;
          }

        fd_ring_clear (&sp->fts_fd_ring);

        if (sp->fts_leaf_optimization_works_ht)
          hash_free (sp->fts_leaf_optimization_works_ht);

        free_dir (sp);

        /* Free up the stream pointer. */
        free(sp);

        /* Set errno and return. */
        if (saved_errno) {
                __set_errno (saved_errno);
                return (-1);
        }

        return (0);
}

/* Minimum link count of a traditional Unix directory.  When leaf
   optimization is OK and a directory's st_nlink == MIN_DIR_NLINK,
   then the directory has no subdirectories.  */
enum { MIN_DIR_NLINK = 2 };

/* Whether leaf optimization is OK for a directory.  */
enum leaf_optimization
  {
    /* st_nlink is not reliable for this directory's subdirectories.  */
    NO_LEAF_OPTIMIZATION,

    /* st_nlink == 2 means the directory lacks subdirectories.  */
    OK_LEAF_OPTIMIZATION
  };

#if (defined __linux__ || defined __ANDROID__) \
  && HAVE_SYS_VFS_H && HAVE_FSTATFS && HAVE_STRUCT_STATFS_F_TYPE

# include <sys/vfs.h>

/* Linux-specific constants from coreutils' src/fs.h */
# define S_MAGIC_AFS 0x5346414F
# define S_MAGIC_CIFS 0xFF534D42
# define S_MAGIC_LUSTRE 0x0BD00BD0
# define S_MAGIC_NFS 0x6969
# define S_MAGIC_PROC 0x9FA0
# define S_MAGIC_TMPFS 0x1021994

# ifdef HAVE___FSWORD_T
typedef __fsword_t fsword;
# else
typedef long int fsword;
# endif

/* Map a stat.st_dev number to a file system type number f_ftype.  */
struct dev_type
{
  dev_t st_dev;
  fsword f_type;
};

/* Use a tiny initial size.  If a traversal encounters more than
   a few devices, the cost of growing/rehashing this table will be
   rendered negligible by the number of inodes processed.  */
enum { DEV_TYPE_HT_INITIAL_SIZE = 13 };

static size_t
dev_type_hash (void const *x, size_t table_size)
{
  struct dev_type const *ax = x;
  uintmax_t dev = ax->st_dev;
  return dev % table_size;
}

static bool
dev_type_compare (void const *x, void const *y)
{
  struct dev_type const *ax = x;
  struct dev_type const *ay = y;
  return ax->st_dev == ay->st_dev;
}

/* Return the file system type of P with file descriptor FD, or 0 if not known.
   If FD is negative, P's file descriptor is unavailable.
   Try to cache known values.  */

static fsword
filesystem_type (FTSENTRY const *p, int fd)
{
  FTSOBJ *sp = FTSENT_FTS(p);

  /* If we're not in CWDFD mode, don't bother with this optimization,
     since the caller is not serious about performance.  */
  if (!ISSET (FTS_CWDFD))
    return 0;

  Hash_table *h = sp->fts_leaf_optimization_works_ht;
  if (! h)
    h = sp->fts_leaf_optimization_works_ht
      = hash_initialize (DEV_TYPE_HT_INITIAL_SIZE, NULL, dev_type_hash,
                         dev_type_compare, free);

  if (h)
    {
      struct dev_type tmp;
      tmp.st_dev = p->fts_statp->st_dev;
      struct dev_type *ent = hash_lookup (h, &tmp);
      if (ent)
        return ent->f_type;
    }

  /* Look-up failed.  Query directly and cache the result.  */
  struct STRUCT_STATFS fs_buf;
  if (fd < 0 || FSTATFS (fd, &fs_buf) != 0)
    return 0;

  if (h)
    {
      struct dev_type *t2 = malloc (sizeof *t2);
      if (t2)
        {
          t2->st_dev = p->fts_statp->st_dev;
          t2->f_type = fs_buf.f_type;

          struct dev_type *ent = hash_insert (h, t2);
          if (ent)
            fts_assert (ent == t2);
          else
            free (t2);
        }
    }

  return fs_buf.f_type;
}

/* Return true if sorting dirents on inode numbers is known to improve
   traversal performance for the directory P with descriptor DIR_FD.
   Return false otherwise.  When in doubt, return true.
   DIR_FD is negative if unavailable.  */
static bool
dirent_inode_sort_may_be_useful (FTSENTRY const *p, int dir_fd)
{
  /* Skip the sort only if we can determine efficiently
     that skipping it is the right thing to do.
     The cost of performing an unnecessary sort is negligible,
     while the cost of *not* performing it can be O(N^2) with
     a very large constant.  */

  switch (filesystem_type (p, dir_fd))
    {
    case S_MAGIC_LUSTRE:
      /* On Lustre, sorting directory entries interferes with its ability to
         prefetch file metadata (via statahead).  This would make a command
         like 'du' around 9 times slower.  See
         <https://bugs.gnu.org/80106>.  */
    case S_MAGIC_CIFS:
    case S_MAGIC_NFS:
    case S_MAGIC_TMPFS:
      /* On a file system of any of these types, sorting
         is unnecessary, and hence wasteful.  */
      return false;

    default:
      return true;
    }
}

/* Given an FTS entry P for a directory with descriptor DIR_FD,
   return whether it is valid to apply leaf optimization.
   The optimization is valid if a directory's st_nlink value equal
   to MIN_DIR_NLINK means the directory has no subdirectories.
   DIR_FD is negative if unavailable.  */
static enum leaf_optimization
leaf_optimization (FTSENTRY const *p, int dir_fd)
{
  switch (filesystem_type (p, dir_fd))
    {
    case 0:
      /* Leaf optimization is unsafe if the file system type is unknown.  */
      FALLTHROUGH;
    case S_MAGIC_AFS:
      /* Although AFS mount points are not counted in st_nlink, they
         act like directories.  See <https://bugs.debian.org/143111>.  */
      FALLTHROUGH;
    case S_MAGIC_CIFS:
      /* Leaf optimization causes 'find' to abort.  See
         <https://lists.gnu.org/r/bug-gnulib/2018-04/msg00015.html>.  */
      FALLTHROUGH;
    case S_MAGIC_NFS:
      /* NFS provides usable dirent.d_type but not necessarily for all entries
         of large directories, so as per <https://bugzilla.redhat.com/1252549>
         NFS should return true.  However st_nlink values are not accurate on
         all implementations as per <https://bugzilla.redhat.com/1299169>.  */
      FALLTHROUGH;
    case S_MAGIC_PROC:
      /* Per <https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=143111> /proc
         may have bogus stat.st_nlink values.  */
      return NO_LEAF_OPTIMIZATION;

    default:
      return OK_LEAF_OPTIMIZATION;
    }
}

#else
static bool
dirent_inode_sort_may_be_useful (_GL_UNUSED FTSENTRY const *p,
                                 _GL_UNUSED int dir_fd)
{
  return true;
}
static enum leaf_optimization
leaf_optimization (_GL_UNUSED FTSENTRY const *p, _GL_UNUSED int dir_fd)
{
  return NO_LEAF_OPTIMIZATION;
}
#endif

/*
 * Special case of "/" at the end of the file name so that slashes aren't
 * appended which would cause file names to be written as "....//foo".
 */
#define NAPPEND(p)                                                      \
        (p->fts_path[p->fts_pathlen - 1] == '/'                         \
            ? p->fts_pathlen - 1 : p->fts_pathlen)

FTSENTRY *
FTS_READ (FTSOBJ *sp)
{
        /* If finished or unrecoverable error, return NULL. */
        if (sp->fts_cur == NULL || ISSET(FTS_STOP))
                return (NULL);

        /* Set current node pointer. */
        register FTSENTRY *p = sp->fts_cur;

        /* Save and zero out user instructions. */
        register unsigned short int instr = p->fts_instr;
        p->fts_instr = FTS_NOINSTR;

        /* Any type of file may be re-visited; re-stat and re-turn. */
        if (instr == FTS_AGAIN) {
                p->fts_info = fts_stat(sp, p, false);
                return (p);
        }
        Dprintf (("fts_read: p=%s\n",
                  p->fts_info == FTS_INIT ? "" : p->fts_path));

        /*
         * Following a symlink -- SLNONE test allows application to see
         * SLNONE and recover.  If indirecting through a symlink, have
         * keep a pointer to current location.  If unable to get that
         * pointer, follow fails.
         */
        if (instr == FTS_FOLLOW &&
            (p->fts_info == FTS_SL || p->fts_info == FTS_SLNONE)) {
                p->fts_info = fts_stat(sp, p, true);
                if (p->fts_info == FTS_D && !ISSET(FTS_NOCHDIR)) {
                        if ((p->fts_symfd = diropen (sp, ".")) < 0) {
                                p->fts_errno = errno;
                                p->fts_info = FTS_ERR;
                        } else
                                p->fts_flags |= FTS_SYMFOLLOW;
                }
                goto check_for_dir;
        }

        /* Directory in pre-order. */
        if (p->fts_info == FTS_D) {
                /* If skipped or crossed mount point, do post-order visit. */
                if (instr == FTS_SKIP ||
                    (ISSET(FTS_XDEV) && p->fts_statp->st_dev != sp->fts_dev)) {
                        if (p->fts_flags & FTS_SYMFOLLOW)
                                (void)close(p->fts_symfd);
                        if (sp->fts_child) {
                                fts_lfree(sp->fts_child);
                                sp->fts_child = NULL;
                        }
                        p->fts_info = FTS_DP;
                        LEAVE_DIR (sp, p, "1");
                        return (p);
                }

                /* Rebuild if only read the names and now traversing. */
                if (sp->fts_child != NULL && ISSET(FTS_NAMEONLY)) {
                        CLR(FTS_NAMEONLY);
                        fts_lfree(sp->fts_child);
                        sp->fts_child = NULL;
                }

                /*
                 * Cd to the subdirectory.
                 *
                 * If have already read and now fail to chdir, whack the list
                 * to make the names come out right, and set the parent errno
                 * so the application will eventually get an error condition.
                 * Set the FTS_DONTCHDIR flag so that when we logically change
                 * directories back to the parent we don't do a chdir.
                 *
                 * If haven't read do so.  If the read fails, fts_build sets
                 * FTS_STOP or the fts_info field of the node.
                 */
                if (sp->fts_child != NULL) {
                        if (fts_safe_changedir(sp, p, -1, p->fts_accpath)) {
                                p->fts_errno = errno;
                                p->fts_flags |= FTS_DONTCHDIR;
                                for (p = sp->fts_child; p != NULL;
                                     p = p->fts_link)
                                        p->fts_accpath =
                                            p->fts_parent->fts_accpath;
                        }
                } else if ((sp->fts_child = fts_build(sp, BREAD)) == NULL) {
                        if (ISSET(FTS_STOP))
                                return (NULL);
                        /* If fts_build's call to fts_safe_changedir failed
                           because it was not able to fchdir into a
                           subdirectory, tell the caller.  */
                        if (p->fts_errno && p->fts_info != FTS_DNR)
                                p->fts_info = FTS_ERR;
                        LEAVE_DIR (sp, p, "2");
                        return (p);
                }
                p = sp->fts_child;
                sp->fts_child = NULL;
                goto name;
        }

        /* Move to the next node on this level. */
next: ;
        register FTSENTRY *tmp = p;

        /* If we have so many directory entries that we're reading them
           in batches, and we've reached the end of the current batch,
           read in a new batch.  */
        if (p->fts_link == NULL && FTSENT_DIRP(p->fts_parent))
          {
            p = tmp->fts_parent;
            sp->fts_cur = p;
            sp->fts_path[p->fts_pathlen] = '\0';

            if ((p = fts_build (sp, BREAD)) == NULL)
              {
                if (ISSET(FTS_STOP))
                  return NULL;
                goto cd_dot_dot;
              }

            free(FTSENT_WRAPPER(tmp));
            goto name;
          }

        if ((p = p->fts_link) != NULL) {
                sp->fts_cur = p;
                free(FTSENT_WRAPPER(tmp));

                /*
                 * If reached the top, return to the original directory (or
                 * the root of the tree), and load the file names for the next
                 * root.
                 */
                if (p->fts_level == FTS_ROOTLEVEL) {
                        if (restore_initial_cwd(sp)) {
                                SET(FTS_STOP);
                                return (NULL);
                        }
                        free_dir(sp);
                        fts_load(sp, p);
                        if (! setup_dir(sp)) {
                                free_dir(sp);
                                return (NULL);
                        }
                        goto check_for_dir;
                }

                /*
                 * User may have called fts_set on the node.  If skipped,
                 * ignore.  If followed, get a file descriptor so we can
                 * get back if necessary.
                 */
                if (p->fts_instr == FTS_SKIP)
                        goto next;
                if (p->fts_instr == FTS_FOLLOW) {
                        p->fts_info = fts_stat(sp, p, true);
                        if (p->fts_info == FTS_D && !ISSET(FTS_NOCHDIR)) {
                                if ((p->fts_symfd = diropen (sp, ".")) < 0) {
                                        p->fts_errno = errno;
                                        p->fts_info = FTS_ERR;
                                } else
                                        p->fts_flags |= FTS_SYMFOLLOW;
                        }
                        p->fts_instr = FTS_NOINSTR;
                }

name:           {
                  register char *t = sp->fts_path + NAPPEND(p->fts_parent);
                  *t++ = '/';
                  memmove(t, p->fts_name, p->fts_namelen + 1);
                }
check_for_dir:
                sp->fts_cur = p;
                if (p->fts_info == FTS_NSOK)
                  {
                    if (p->fts_statp->st_size == FTS_STAT_REQUIRED)
                      p->fts_info = fts_stat(sp, p, false);
                    else
                      fts_assert (p->fts_statp->st_size == FTS_NO_STAT_REQUIRED);
                  }

                /* Skip files with different device numbers when FTS_MOUNT
                   is set.  */
                if (ISSET (FTS_MOUNT) && p->fts_info != FTS_NS &&
                    p->fts_level != FTS_ROOTLEVEL &&
                    p->fts_statp->st_dev != sp->fts_dev)
                      goto next;

                if (p->fts_info == FTS_D)
                  {
                    /* Now that P->fts_statp is guaranteed to be valid, if
                       this is a command-line directory, record its device
                       number, to be used for FTS_MOUNT and FTS_XDEV.  */
                    if (p->fts_level == FTS_ROOTLEVEL)
                      sp->fts_dev = p->fts_statp->st_dev;
                    Dprintf (("  entering: %s\n", p->fts_path));
                    if (! enter_dir (sp, p))
                      return NULL;
                  }
                return p;
        }
cd_dot_dot:

        /* Move up to the parent node. */
        p = tmp->fts_parent;
        sp->fts_cur = p;
        free(FTSENT_WRAPPER(tmp));

        if (p->fts_level == FTS_ROOTPARENTLEVEL) {
                /*
                 * Done; free everything up and set errno to 0 so the user
                 * can distinguish between error and EOF.
                 */
                free(FTSENT_WRAPPER(p));
                __set_errno (0);
                return (sp->fts_cur = NULL);
        }

        fts_assert (p->fts_info != FTS_NSOK);

        /* NUL terminate the file name.  */
        sp->fts_path[p->fts_pathlen] = '\0';

        /*
         * Return to the parent directory.  If at a root node, restore
         * the initial working directory.  If we came through a symlink,
         * go back through the file descriptor.  Otherwise, move up
         * one level, via "..".
         */
        if (p->fts_level == FTS_ROOTLEVEL) {
                if (restore_initial_cwd(sp)) {
                        p->fts_errno = errno;
                        SET(FTS_STOP);
                }
        } else if (p->fts_flags & FTS_SYMFOLLOW) {
                if (FCHDIR(sp, p->fts_symfd)) {
                        p->fts_errno = errno;
                        SET(FTS_STOP);
                }
                (void)close(p->fts_symfd);
        } else if (!(p->fts_flags & FTS_DONTCHDIR) &&
                   fts_safe_changedir(sp, p->fts_parent, -1, "..")) {
                p->fts_errno = errno;
                SET(FTS_STOP);
        }

        /* If the directory causes a cycle, preserve the FTS_DC flag and keep
           the corresponding dev/ino pair in the hash table.  It is going to be
           removed when leaving the original directory.  */
        if (p->fts_info != FTS_DC) {
                p->fts_info = p->fts_errno ? FTS_ERR : FTS_DP;
                if (p->fts_errno == 0)
                        LEAVE_DIR (sp, p, "3");
        }
        return ISSET(FTS_STOP) ? NULL : p;
}

/*
 * Fts_set takes the stream as an argument although it's not used in this
 * implementation; it would be necessary if anyone wanted to add global
 * semantics to fts using fts_set.  An error return is allowed for similar
 * reasons.
 */
/* ARGSUSED */
int
FTS_SET (_GL_UNUSED FTSOBJ *sp, FTSENTRY *p, int instr)
{
        if (instr != 0 && instr != FTS_AGAIN && instr != FTS_FOLLOW &&
            instr != FTS_NOINSTR && instr != FTS_SKIP) {
                __set_errno (EINVAL);
                return (1);
        }
        p->fts_instr = instr;
        return (0);
}

FTSENTRY *
FTS_CHILDREN (FTSOBJ *sp, int instr)
{
        if (instr != 0 && instr != FTS_NAMEONLY) {
                __set_errno (EINVAL);
                return (NULL);
        }

        /* Set current node pointer. */
        register FTSENTRY *p = sp->fts_cur;

        /*
         * Errno set to 0 so user can distinguish empty directory from
         * an error.
         */
        __set_errno (0);

        /* Fatal errors stop here. */
        if (ISSET(FTS_STOP))
                return (NULL);

        /* Return logical hierarchy of user's arguments. */
        if (p->fts_info == FTS_INIT)
                return (p->fts_link);

        /*
         * If not a directory being visited in pre-order, stop here.  Could
         * allow FTS_DNR, assuming the user has fixed the problem, but the
         * same effect is available with FTS_AGAIN.
         */
        if (p->fts_info != FTS_D /* && p->fts_info != FTS_DNR */)
                return (NULL);

        /* Free up any previous child list. */
        if (sp->fts_child != NULL)
                fts_lfree(sp->fts_child);

        if (instr == FTS_NAMEONLY) {
                SET(FTS_NAMEONLY);
                instr = BNAMES;
        } else
                instr = BCHILD;

        /*
         * If using chdir on a relative file name and called BEFORE fts_read
         * does its chdir to the root of a traversal, we can lose -- we need to
         * chdir into the subdirectory, and we don't know where the current
         * directory is, so we can't get back so that the upcoming chdir by
         * fts_read will work.
         */
        if (p->fts_level != FTS_ROOTLEVEL || p->fts_accpath[0] == '/' ||
            ISSET(FTS_NOCHDIR))
                return (sp->fts_child = fts_build(sp, instr));

        int fd = diropen (sp, ".");
        if (fd < 0)
                return (sp->fts_child = NULL);
        sp->fts_child = fts_build(sp, instr);
        if (ISSET(FTS_CWDFD))
          {
            cwd_advance_fd (sp, fd, true);
          }
        else
          {
            if (fchdir(fd))
              {
                int saved_errno = errno;
                close (fd);
                __set_errno (saved_errno);
                return NULL;
              }
            close (fd);
          }
        return (sp->fts_child);
}

/* A comparison function to sort on increasing inode number.
   For some file system types, sorting either way makes a huge
   performance difference for a directory with very many entries,
   but sorting on increasing values is slightly better than sorting
   on decreasing values.  The difference is in the 5% range.  */
static int
fts_compare_ino (FTSENTRY const **a, FTSENTRY const **b)
{
  return _GL_CMP (a[0]->fts_statp->st_ino, b[0]->fts_statp->st_ino);
}

/* Map the dirent.d_type value, DTYPE, to the corresponding stat.st_mode
   S_IF* bit and set ST.st_mode, thus clearing all other bits in that field.  */
static void
set_stat_type (struct STRUCT_STAT *st, unsigned int dtype)
{
  mode_t type;
  switch (dtype)
    {
    case DT_BLK:
      type = S_IFBLK;
      break;
    case DT_CHR:
      type = S_IFCHR;
      break;
    case DT_DIR:
      type = S_IFDIR;
      break;
    case DT_FIFO:
      type = S_IFIFO;
      break;
    case DT_LNK:
      type = S_IFLNK;
      break;
    case DT_REG:
      type = S_IFREG;
      break;
    case DT_SOCK:
      type = S_IFSOCK;
      break;
    default:
      type = 0;
    }
  st->st_mode = type;
}

#define closedir_and_clear(dirp)                \
  do                                            \
    {                                           \
      closedir (dirp);                          \
      dirp = NULL;                              \
    }                                           \
  while (0)

#define fts_opendir(file, Pdir_fd)                              \
        OPENDIRAT((! ISSET(FTS_NOCHDIR) && ISSET(FTS_CWDFD)     \
                   ? sp->fts_cwd_fd : AT_FDCWD),                \
                  file,                                         \
                  (((ISSET(FTS_PHYSICAL)                        \
                     && ! (ISSET(FTS_COMFOLLOW)                 \
                           && cur->fts_level == FTS_ROOTLEVEL)) \
                    ? O_NOFOLLOW : 0)),                         \
                  Pdir_fd)

/*
 * This is the tricky part -- do not casually change *anything* in here.  The
 * idea is to build the linked list of entries that are used by fts_children
 * and fts_read.  There are lots of special cases.
 *
 * The real slowdown in walking the tree is the stat calls.  If FTS_NOSTAT is
 * set and it's a physical walk (so that symbolic links can't be directories),
 * we can do things quickly.  First, if it's a 4.4BSD file system, the type
 * of the file is in the directory entry.  Otherwise, we assume that the number
 * of subdirectories in a node is equal to the number of links to the parent.
 * The former skips all stat calls.  The latter skips stat calls in any leaf
 * directories and for any files after the subdirectories in the directory have
 * been found, cutting the stat calls by about 2/3.
 */
static FTSENTRY *
internal_function
fts_build (register FTSOBJ *sp, int type)
{
        FTSENTRY *cur = sp->fts_cur;
        bool continue_readdir = !!FTSENT_DIRP(cur);

        /* When cur->fts_dirp is non-NULL, that means we should
           continue calling readdir on that existing DIR* pointer
           rather than opening a new one.  */
        int dir_fd;
        if (continue_readdir)
          {
            DIR *dp = FTSENT_DIRP(cur);
            dir_fd = dirfd (dp);
            if (dir_fd < 0)
              {
                int dirfd_errno = errno;
                closedir_and_clear (FTSENT_DIRP(cur));
                if (type == BREAD)
                  {
                    cur->fts_info = FTS_DNR;
                    cur->fts_errno = dirfd_errno;
                  }
                return NULL;
              }
          }
        else
          {
            /* Open the directory for reading.  If this fails, we're done.
               If being called from fts_read, set the fts_info field. */
            if ((FTSENT_DIRP (cur) = fts_opendir(cur->fts_accpath, &dir_fd)) == NULL)
              {
                if (type == BREAD)
                  {
                    cur->fts_info = FTS_DNR;
                    cur->fts_errno = errno;
                  }
                return NULL;
              }
            /* Rather than calling fts_stat for each and every entry encountered
               in the readdir loop (below), stat each directory only right after
               opening it.  */
            bool stat_optimization = cur->fts_info == FTS_NSOK;

            if (stat_optimization
                /* Also read the stat info again after opening a directory to
                   reveal eventual changes caused by a submount triggered by
                   the traversal.  But do it only for utilities which use
                   FTS_TIGHT_CYCLE_CHECK.  Therefore, only find and du
                   benefit/suffer from this feature for now.  */
                || ISSET (FTS_TIGHT_CYCLE_CHECK))
              {
                if (!stat_optimization)
                  LEAVE_DIR (sp, cur, "4");
                if (FSTAT (dir_fd, cur->fts_statp) != 0)
                  {
                    int fstat_errno = errno;
                    closedir_and_clear (FTSENT_DIRP(cur));
                    if (type == BREAD)
                      {
                        cur->fts_errno = fstat_errno;
                        cur->fts_info = FTS_NS;
                      }
                    __set_errno (fstat_errno);
                    return NULL;
                  }
                if (stat_optimization)
                  cur->fts_info = FTS_D;
                else if (! enter_dir (sp, cur))
                  {
                    int saved_errno = errno;
                    closedir_and_clear (FTSENT_DIRP(cur));
                    __set_errno (saved_errno);
                    return NULL;
                  }
              }
          }

        /* Maximum number of readdir entries to read at one time.  This
           limitation is to avoid reading millions of entries into memory
           at once.  When an fts_compar function is specified, we have no
           choice: we must read all entries into memory before calling that
           function.  But when no such function is specified, we can read
           entries in batches that are large enough to help us with inode-
           sorting, yet not so large that we risk exhausting memory.  */
        size_t max_entries = sp->fts_compar ? SIZE_MAX : FTS_MAX_READDIR_ENTRIES;

        /*
         * If we're going to need to stat anything or we want to descend
         * and stay in the directory, chdir.  If this fails we keep going,
         * but set a flag so we don't chdir after the post-order visit.
         * We won't be able to stat anything, but we can still return the
         * names themselves.  Note, that since fts_read won't be able to
         * chdir into the directory, it will have to return different file
         * names than before, i.e. "a/b" instead of "b".  Since the node
         * has already been visited in pre-order, have to wait until the
         * post-order visit to return the error.  There is a special case
         * here, if there was nothing to stat then it's not an error to
         * not be able to stat.  This is all fairly nasty.  If a program
         * needed sorted entries or stat information, they had better be
         * checking FTS_NS on the returned nodes.
         */
        bool descend;
        if (continue_readdir)
          {
            /* When resuming a short readdir run, we already have
               the required dirp and dir_fd.  */
            descend = true;
          }
        else
          {
            /* Try to descend unless it is a names-only fts_children,
               or the directory is known to lack subdirectories.  */
            descend = (type != BNAMES
                       && ! (ISSET (FTS_NOSTAT) && ISSET (FTS_PHYSICAL)
                             && ! ISSET (FTS_SEEDOT)
                             && cur->fts_statp->st_nlink == MIN_DIR_NLINK
                             && (leaf_optimization (cur, dir_fd)
                                 != NO_LEAF_OPTIMIZATION)));
            if (descend || type == BREAD)
              {
                if (ISSET(FTS_CWDFD))
                  dir_fd = fcntl (dir_fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
                if (dir_fd < 0 || fts_safe_changedir(sp, cur, dir_fd, NULL)) {
                        if (descend && type == BREAD)
                                cur->fts_errno = errno;
                        cur->fts_flags |= FTS_DONTCHDIR;
                        descend = false;
                        closedir_and_clear(FTSENT_DIRP(cur));
                        if (ISSET(FTS_CWDFD) && 0 <= dir_fd)
                                close (dir_fd);
                        FTSENT_DIRP(cur) = NULL;
                } else
                        descend = true;
              }
          }

        /*
         * Figure out the max file name length that can be stored in the
         * current buffer -- the inner loop allocates more space as necessary.
         * We really wouldn't have to do the maxlen calculations here, we
         * could do them in fts_read before returning the name, but it's a
         * lot easier here since the length is part of the dirent structure.
         *
         * If not changing directories set a pointer so that can just append
         * each new component into the file name.
         */
        size_t len = NAPPEND(cur);
        char *cp;
        if (ISSET(FTS_NOCHDIR)) {
                cp = sp->fts_path + len;
                *cp++ = '/';
        } else {
                /* GCC, you're too verbose. */
                cp = NULL;
        }
        len++;
        size_t maxlen = sp->fts_pathlen - len;

        ptrdiff_t level = cur->fts_level + 1;

        /* Read the directory, attaching each entry to the "link" pointer. */
        bool doadjust = false;
        register FTSENTRY *head = NULL;
        FTSENTRY *tail = NULL;
        register size_t nitems = 0;
        bool sort_by_inode = false;
        while (FTSENT_DIRP(cur)) {
                __set_errno (0);
                struct dirent *dp = readdir(FTSENT_DIRP(cur));
                if (dp == NULL) {
                        /* Some readdir()s do not absorb ENOENT (dir
                           deleted but open).  This bug was fixed in
                           glibc 2.3 (2002).  */
#if ! (2 < __GLIBC__ + (3 <= __GLIBC_MINOR__))
                        if (errno == ENOENT)
                          errno = 0;
#endif
                        if (errno) {
                                cur->fts_errno = errno;
                                /* If we've not read any items yet, treat
                                   the error as if we can't access the dir.  */
                                cur->fts_info = (continue_readdir || nitems)
                                                ? FTS_ERR : FTS_DNR;
                        }
                        closedir_and_clear(FTSENT_DIRP(cur));
                        break;
                }
                if (!ISSET(FTS_SEEDOT) && ISDOT(dp->d_name))
                        continue;

                size_t d_namelen = _D_EXACT_NAMLEN (dp);
                register FTSENTRY *p = fts_alloc (sp, dp->d_name, d_namelen);
                if (!p)
                        goto mem1;
                if (d_namelen >= maxlen) {
                        /* include space for NUL */
                        uintptr_t oldaddr = (uintptr_t) sp->fts_path;
                        if (! fts_palloc(sp, d_namelen + len + 1)) {
                                /*
                                 * No more memory.  Save
                                 * errno, free up the current structure and the
                                 * structures already allocated.
                                 */
mem1: ;
                                int saved_errno = errno;
                                free(FTSENT_WRAPPER(p));
                                fts_lfree(head);
                                closedir_and_clear(FTSENT_DIRP(cur));
                                cur->fts_info = FTS_ERR;
                                SET(FTS_STOP);
                                __set_errno (saved_errno);
                                return (NULL);
                        }
                        /* Did realloc() change the pointer? */
                        if (oldaddr != (uintptr_t) sp->fts_path) {
                                doadjust = true;
                                if (ISSET(FTS_NOCHDIR))
                                        cp = sp->fts_path + len;
                        }
                        maxlen = sp->fts_pathlen - len;
                }

                size_t new_len = len + d_namelen;
                if (new_len < len) {
                        /*
                         * In the unlikely event that we would end up
                         * with a file name longer than SIZE_MAX, free up
                         * the current structure and the structures already
                         * allocated, then error out with ENAMETOOLONG.
                         */
                        free(FTSENT_WRAPPER(p));
                        fts_lfree(head);
                        closedir_and_clear(FTSENT_DIRP(cur));
                        cur->fts_info = FTS_ERR;
                        SET(FTS_STOP);
                        __set_errno (ENAMETOOLONG);
                        return (NULL);
                }
                p->fts_level = level;
                p->fts_parent = sp->fts_cur;
                p->fts_pathlen = new_len;

                /* Store dirent.d_ino, in case we need to sort
                   entries before processing them.  */
                p->fts_statp->st_ino = D_INO (dp);

                /* Build a file name for fts_stat to stat. */
                if (ISSET(FTS_NOCHDIR)) {
                        p->fts_accpath = p->fts_path;
                        memmove(cp, p->fts_name, p->fts_namelen + 1);
                } else
                        p->fts_accpath = p->fts_name;

                if (sp->fts_compar == NULL || ISSET(FTS_DEFER_STAT)) {
                        /* Record what fts_read will have to do with this
                           entry. In many cases, it will simply fts_stat it,
                           but we can take advantage of any d_type information
                           to optimize away the unnecessary stat calls.  I.e.,
                           if FTS_NOSTAT is in effect, we don't need device
                           numbers unconditionally (FTS_MOUNT) and we're not
                           following symlinks (FTS_PHYSICAL) and d_type
                           indicates this is *not* a directory, then we won't
                           have to stat it  at all.  If it *is* a directory,
                           then (currently) we stat it regardless, in order to
                           get device and inode numbers.  Some day we might
                           optimize that away, too, for directories where
                           d_ino is known to be valid.  */
                        bool skip_stat = (ISSET(FTS_NOSTAT)
                                          && DT_IS_KNOWN(dp)
                                          && ! DT_MUST_BE(dp, DT_DIR)
                                          && (ISSET(FTS_PHYSICAL)
                                              || ! DT_MUST_BE(dp, DT_LNK))
                                          && ! ISSET(FTS_MOUNT));
                        p->fts_info = FTS_NSOK;
                        /* Propagate dirent.d_type information back
                           to caller, when possible.  */
                        set_stat_type (p->fts_statp, D_TYPE (dp));
                        fts_set_stat_required(p, !skip_stat);
                } else {
                        p->fts_info = fts_stat(sp, p, false);
                }

                /* We walk in directory order so "ls -f" doesn't get upset. */
                p->fts_link = NULL;
                if (head == NULL)
                        head = tail = p;
                else {
                        tail->fts_link = p;
                        tail = p;
                }

                /* If there are many entries, no sorting function has been
                   specified, and this file system is of a type that may be
                   slow with a large number of entries, arrange to sort the
                   directory entries on increasing inode numbers.

                   The NITEMS comparison uses ==, not >, because the test
                   needs to be tried at most once once, and NITEMS will exceed
                   the threshold after it is incremented below.  */
                if (nitems == _FTS_INODE_SORT_DIR_ENTRIES_THRESHOLD
                    && !sp->fts_compar)
                  sort_by_inode = dirent_inode_sort_may_be_useful (cur, dir_fd);

                ++nitems;
                if (max_entries <= nitems) {
                        /* When there are too many dir entries, leave
                           fts_dirp open, so that a subsequent fts_read
                           can take up where we leave off.  */
                        break;
                }
        }

        /*
         * If realloc() changed the address of the file name, adjust the
         * addresses for the rest of the tree and the dir list.
         */
        if (doadjust)
                fts_padjust(sp, head);

        /*
         * If not changing directories, reset the file name back to original
         * state.
         */
        if (ISSET(FTS_NOCHDIR)) {
                if (len == sp->fts_pathlen || nitems == 0)
                        --cp;
                *cp = '\0';
        }

        /*
         * If descended after called from fts_children or after called from
         * fts_read and nothing found, get back.  At the root level we use
         * the saved fd; if one of fts_open()'s arguments is a relative name
         * to an empty directory, we wind up here with no other way back.  If
         * can't get back, we're done.
         */
        if (!continue_readdir && descend && (type == BCHILD || !nitems) &&
            (cur->fts_level == FTS_ROOTLEVEL
             ? restore_initial_cwd(sp)
             : fts_safe_changedir(sp, cur->fts_parent, -1, ".."))) {
                cur->fts_info = FTS_ERR;
                SET(FTS_STOP);
                fts_lfree(head);
                return (NULL);
        }

        /* If didn't find anything, return NULL. */
        if (!nitems) {
                if (type == BREAD
                    && cur->fts_info != FTS_DNR && cur->fts_info != FTS_ERR)
                        cur->fts_info = FTS_DP;
                fts_lfree(head);
                return (NULL);
        }

        if (sort_by_inode) {
                sp->fts_compar = FTS_COMPAR_CAST (fts_compare_ino);
                head = fts_sort (sp, head, nitems);
                sp->fts_compar = NULL;
        }

        /* Sort the entries. */
        if (sp->fts_compar && nitems > 1)
                head = fts_sort(sp, head, nitems);
        return (head);
}

#if GNULIB_FTS_DEBUG

struct devino {
  intmax_t dev, ino;
};
#define PRINT_DEVINO "(%jd,%jd)"

static struct devino
getdevino (int fd)
{
  struct STRUCT_STAT st;
  return (fd == AT_FDCWD
          ? (struct devino) { -1, 0 }
          : FSTAT (fd, &st) == 0
          ? (struct devino) { st.st_dev, st.st_ino }
          : (struct devino) { -1, errno });
}

/* Walk ->fts_parent links starting at E_CURR, until the root of the
   current hierarchy.  There should be a directory with dev/inode
   matching those of AD.  If not, print a lot of diagnostics.  */
static void
find_matching_ancestor (FTSENTRY const *e_curr, struct Active_dir const *ad)
{
  for (FTSENTRY const *ent = e_curr;
       ent->fts_level >= FTS_ROOTLEVEL;
       ent = ent->fts_parent)
    {
      if (ad->ino == ent->fts_statp->st_ino
          && ad->dev == ent->fts_statp->st_dev)
        return;
    }
  printf ("ERROR: tree dir, %s, not active\n", ad->fts_ent->fts_accpath);
  printf ("active dirs:\n");
  for (FTSENTRY const *ent = e_curr;
       ent->fts_level >= FTS_ROOTLEVEL;
       ent = ent->fts_parent)
    printf ("  %s(%"PRIuMAX"/%"PRIuMAX") to %s(%"PRIuMAX"/%"PRIuMAX")...\n",
            ad->fts_ent->fts_accpath,
            (uintmax_t) ad->dev,
            (uintmax_t) ad->ino,
            ent->fts_accpath,
            (uintmax_t) ent->fts_statp->st_dev,
            (uintmax_t) ent->fts_statp->st_ino);
}

void
fts_cross_check (FTSOBJ const *sp)
{
  if ( ! ISSET (FTS_TIGHT_CYCLE_CHECK))
    return;

  FTSENTRY const *ent = sp->fts_cur;

  Dprintf (("fts-cross-check cur=%s\n", ent->fts_path));
  /* Make sure every parent dir is in the tree.  */
  for (FTSENTRY const *t = ent->fts_parent;
       t->fts_level >= FTS_ROOTLEVEL;
       t = t->fts_parent)
    {
      struct Active_dir ad;
      ad.ino = t->fts_statp->st_ino;
      ad.dev = t->fts_statp->st_dev;
      if ( ! hash_lookup (sp->fts_cycle.ht, &ad))
        printf ("ERROR: active dir, %s, not in tree\n", t->fts_path);
    }

  /* Make sure every dir in the tree is an active dir.
     But ENT is not necessarily a directory.  If so, just skip this part. */
  if (ent->fts_parent->fts_level >= FTS_ROOTLEVEL
      && (ent->fts_info == FTS_DP
          || ent->fts_info == FTS_D))
    for (struct Active_dir *ad = hash_get_first (sp->fts_cycle.ht);
         ad != NULL;
         ad = hash_get_next (sp->fts_cycle.ht, ad))
      {
        find_matching_ancestor (ent, ad);
      }
}

static bool
same_fd (int fd1, int fd2)
{
  struct STRUCT_STAT sb1, sb2;
  return (FSTAT (fd1, &sb1) == 0
          && FSTAT (fd2, &sb2) == 0
          && psame_inode (&sb1, &sb2));
}

static void
fd_ring_print (FTSOBJ const *sp, FILE *stream, char const *msg)
{
  if (!fts_debug)
    return;
  I_ring const *fd_ring = &sp->fts_fd_ring;
  struct devino cwd = getdevino (sp->fts_cwd_fd);
  fprintf (stream, "=== %s ========== "PRINT_DEVINO"\n", msg, cwd.dev, cwd.ino);
  if (i_ring_empty (fd_ring))
    return;

  unsigned int i = fd_ring->ir_front;
  while (true)
    {
      int fd = fd_ring->ir_data[i];
      if (fd < 0)
        fprintf (stream, "%u: %d:\n", i, fd);
      else
        {
          struct devino wd = getdevino (fd);
          fprintf (stream, "%u: %d: "PRINT_DEVINO"\n", i, fd, wd.dev, wd.ino);
        }
      if (i == fd_ring->ir_back)
        break;
      i = (i + I_RING_SIZE - 1) % I_RING_SIZE;
    }
}

/* Ensure that each file descriptor on the fd_ring matches a
   parent, grandparent, etc. of the current working directory.  */
static void
fd_ring_check (FTSOBJ const *sp)
{
  if (!fts_debug)
    return;

  /* Make a writable copy.  */
  I_ring fd_w = sp->fts_fd_ring;

  int cwd_fd = sp->fts_cwd_fd;
  cwd_fd = fcntl (cwd_fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
  struct devino dot = getdevino (cwd_fd);
  fprintf (stderr, "===== check ===== cwd: "PRINT_DEVINO"\n",
           dot.dev, dot.ino);
  while ( ! i_ring_empty (&fd_w))
    {
      int fd = i_ring_pop (&fd_w);
      if (0 <= fd)
        {
          int open_flags = O_SEARCH | O_CLOEXEC;
          int parent_fd = openat (cwd_fd, "..", open_flags);
          if (parent_fd < 0)
            {
              // Warn?
              break;
            }
          if (!same_fd (fd, parent_fd))
            {
              struct devino cwd = getdevino (fd);
              fprintf (stderr, "ring  : "PRINT_DEVINO"\n", cwd.dev, cwd.ino);
              struct devino c2 = getdevino (parent_fd);
              fprintf (stderr, "parent: "PRINT_DEVINO"\n", c2.dev, c2.ino);
              fts_assert (0);
            }
          close (cwd_fd);
          cwd_fd = parent_fd;
        }
    }
  close (cwd_fd);
}
#endif

static unsigned short int
internal_function
fts_stat(FTSOBJ *sp, register FTSENTRY *p, bool follow)
{
        if (ISSET (FTS_LOGICAL)
            || (ISSET (FTS_COMFOLLOW) && p->fts_level == FTS_ROOTLEVEL))
                follow = true;

        struct STRUCT_STAT *sbp = p->fts_statp;

        /*
         * If doing a logical walk, or application requested FTS_FOLLOW, do
         * a stat(2).  If that fails, check for a nonexistent symlink.  If
         * fail, set the errno from the stat call.
         */
        int flags = follow ? 0 : AT_SYMLINK_NOFOLLOW;
        if (FSTATAT (sp->fts_cwd_fd, p->fts_accpath, sbp, flags) < 0)
          {
            if (follow && errno == ENOENT
                && 0 <= FSTATAT (sp->fts_cwd_fd, p->fts_accpath, sbp,
                                 AT_SYMLINK_NOFOLLOW))
              {
                __set_errno (0);
                return FTS_SLNONE;
              }

            p->fts_errno = errno;
           memset (sbp, 0, sizeof *sbp);
            return FTS_NS;
          }

        if (S_ISDIR(sbp->st_mode)) {
                if (ISDOT(p->fts_name)) {
                        /* Command-line "." and ".." are real directories. */
                        return (p->fts_level == FTS_ROOTLEVEL ? FTS_D : FTS_DOT);
                }

                return (FTS_D);
        }
        if (S_ISLNK(sbp->st_mode))
                return (FTS_SL);
        if (S_ISREG(sbp->st_mode))
                return (FTS_F);
        return (FTS_DEFAULT);
}

static int
fts_compar (void const *a, void const *b)
{
  /* Convert A and B to the correct types, to pacify the compiler, and
     for portability to bizarre hosts where "void const *" and "FTSENT
     const **" differ in runtime representation.  The comparison
     function cannot modify *a and *b, but there is no compile-time
     check for this.  */
  FTSENTRY const **pa = (FTSENTRY const **) a;
  FTSENTRY const **pb = (FTSENTRY const **) b;
  return FTSENT_FTS(pa[0])->fts_compar (pa, pb);
}

static FTSENTRY *
internal_function
fts_sort (FTSOBJ *sp, FTSENTRY *head, register size_t nitems)
{
        register FTSENTRY **ap, *p;

        /* On most modern hosts, void * and FTSENT ** have the same
           run-time representation, and one can convert sp->fts_compar to
           the type qsort expects without problem.  Use the heuristic that
           this is OK if the two pointer types are the same size, and if
           converting FTSENT ** to uintptr_t is the same as converting
           FTSENT ** to void * and then to uintptr_t.  This heuristic isn't
           valid in general but we don't know of any counterexamples.  */
        FTSENTRY *dummy;
        int (*compare) (void const *, void const *) =
          ((sizeof &dummy == sizeof (void *)
            && (uintptr_t) &dummy == (uintptr_t) (void *) &dummy)
           ? (int (*) (void const *, void const *)) sp->fts_compar
           : fts_compar);

        /*
         * Construct an array of pointers to the structures and call qsort(3).
         * Reassemble the array in the order returned by qsort.  If unable to
         * sort for memory reasons, return the directory entries in their
         * current order.  Allocate enough space for the current needs plus
         * 40 so don't realloc one entry at a time.
         */
        if (nitems > sp->fts_nitems) {
                sp->fts_nitems = nitems + 40;
                FTSENTRY **a;
                if (! (a = reallocarray (sp->fts_array,
                                         sp->fts_nitems, sizeof *a))) {
                        free(sp->fts_array);
                        sp->fts_array = NULL;
                        sp->fts_nitems = 0;
                        return (head);
                }
                sp->fts_array = a;
        }
        for (ap = sp->fts_array, p = head; p; p = p->fts_link)
                *ap++ = p;
        qsort((void *)sp->fts_array, nitems, sizeof(FTSENTRY *), compare);
        for (head = *(ap = sp->fts_array); --nitems; ++ap)
                ap[0]->fts_link = ap[1];
        ap[0]->fts_link = NULL;
        return (head);
}

static FTSENTRY *
internal_function
fts_alloc (FTSOBJ *sp, const char *name, register size_t namelen)
{
        /*
         * The file name is a variable length array.  Allocate the FTSENT
         * structure and the file name in one chunk.
         */
        size_t len = FLEXSIZEOF(FTSENT, fts_name, namelen + 1);
	register FTSENTRY *p;
#if !_LIBC
        p = malloc(len);
        if (p == NULL)
                return (NULL);
#else
	/*
	 * For glibc, we use a wrapper struct to provide the extra required
	 * fields without changing the FSENT layout.
	 */
	len += sizeof (struct FTSENT_wrapper);
	struct FTSENT_wrapper *wrapper = malloc(len);
	if (wrapper == NULL)
		return (NULL);
	p = &wrapper->ent;
        p->fts_statp = &wrapper->fts_stat;
#endif

        /* Copy the name and guarantee NUL termination. */
        memcpy(p->fts_name, name, namelen);
        p->fts_name[namelen] = '\0';

        p->fts_namelen = namelen;
        FTSENT_FTS(p)= sp;
        p->fts_path = sp->fts_path;
        p->fts_errno = 0;
        FTSENT_DIRP(p) = NULL;
        p->fts_flags = 0;
        p->fts_instr = FTS_NOINSTR;
        p->fts_number = 0;
        p->fts_pointer = NULL;
        return (p);
}

static void
internal_function
fts_lfree (register FTSENTRY *head)
{
        int saved_errno = errno;

        /* Free a linked list of structures. */
        register FTSENTRY *p;
        while ((p = head)) {
                head = head->fts_link;
                if (FTSENT_DIRP(p))
                        closedir (FTSENT_DIRP(p));
                free(FTSENT_WRAPPER(p));
        }

        __set_errno (saved_errno);
}

/*
 * Allow essentially unlimited file name lengths; find, rm, ls should
 * all work on any tree.  Most systems will allow creation of file
 * names much longer than MAXPATHLEN, even though the kernel won't
 * resolve them.  Add the size (not just what's needed) plus 256 bytes
 * so don't realloc the file name 2 bytes at a time.
 */
static bool
internal_function
fts_palloc (FTSOBJ *sp, size_t more)
{
        size_t new_len = sp->fts_pathlen + more + 256;

        /*
         * See if fts_pathlen would overflow.
         */
        if (new_len < sp->fts_pathlen) {
                free(sp->fts_path);
                sp->fts_path = NULL;
                __set_errno (ENAMETOOLONG);
                return false;
        }
        sp->fts_pathlen = new_len;
        char *p = realloc(sp->fts_path, sp->fts_pathlen);
        if (p == NULL) {
                free(sp->fts_path);
                sp->fts_path = NULL;
                return false;
        }
        sp->fts_path = p;
        return true;
}

/*
 * When the file name is realloc'd, have to fix all of the pointers in
 *  structures already returned.
 */
static void
internal_function
fts_padjust (FTSOBJ *sp, FTSENTRY *head)
{
        char *addr = sp->fts_path;

        /* This code looks at bit-patterns of freed pointers to
           relocate them, so it relies on undefined behavior.  If this
           trick does not work on your platform, please report a bug.  */

#define ADJUST(p) do {                                                  \
        uintptr_t old_accpath = (uintptr_t) (p)->fts_accpath;           \
        if (old_accpath != (uintptr_t) (p)->fts_name) {                 \
                (p)->fts_accpath =                                      \
                  addr + (old_accpath - (uintptr_t) (p)->fts_path);     \
        }                                                               \
        (p)->fts_path = addr;                                           \
} while (0)
        /* Adjust the current set of children. */
        for (FTSENTRY *p = sp->fts_child; p; p = p->fts_link)
                ADJUST(p);

        /* Adjust the rest of the tree, including the current level. */
        for (FTSENTRY *p = head; p->fts_level >= FTS_ROOTLEVEL;) {
                ADJUST(p);
                p = p->fts_link ? p->fts_link : p->fts_parent;
        }
}

static size_t
internal_function _GL_ATTRIBUTE_PURE
fts_maxarglen (char * const *argv)
{
        size_t max;

        for (max = 0; *argv; ++argv) {
                size_t len = strlen(*argv);
                if (len > max)
                        max = len;
        }
        return (max + 1);
}

/*
 * Change to dir specified by fd or file name without getting
 * tricked by someone changing the world out from underneath us.
 * Assumes p->fts_statp->st_dev and p->fts_statp->st_ino are filled in.
 * If FD is non-negative, expect it to be used after this function returns,
 * and to be closed eventually.  So don't pass e.g., 'dirfd(dirp)' and then
 * do closedir(dirp), because that would invalidate the saved FD.
 * Upon failure, close FD immediately and return nonzero.
 */
static int
internal_function
fts_safe_changedir (FTSOBJ *sp, FTSENTRY *p, int fd, char const *dir)
{
        fts_assert (0 <= fd || dir != NULL);
        bool is_dotdot = dir && streq (dir, "..");

        /* This clause handles the unusual case in which FTS_NOCHDIR
           is specified, along with FTS_CWDFD.  In that case, there is
           no need to change even the virtual cwd file descriptor.
           However, if FD is non-negative, we do close it here.  */
        if (ISSET (FTS_NOCHDIR))
          {
            if (ISSET (FTS_CWDFD) && 0 <= fd)
              close (fd);
            return 0;
          }

        if (fd < 0 && is_dotdot && ISSET (FTS_CWDFD))
          {
            /* When possible, skip the diropen and subsequent fstat+dev/ino
               comparison.  I.e., when changing to parent directory
               (chdir ("..")), use a file descriptor from the ring and
               save the overhead of diropen+fstat, as well as avoiding
               failure when we lack "x" access to the virtual cwd.  */
            if ( ! i_ring_empty (&sp->fts_fd_ring))
              {
                int parent_fd;
                fd_ring_print (sp, stderr, "pre-pop");
                parent_fd = i_ring_pop (&sp->fts_fd_ring);
                if (0 <= parent_fd)
                  {
                    fd = parent_fd;
                    dir = NULL;
                  }
              }
          }

        int newfd = fd;
        if (fd < 0 && (newfd = diropen (sp, dir)) < 0)
          return -1;

        /* The following dev/inode check is necessary if we're doing a
           "logical" traversal (through symlinks, a la chown -L), if the
           system lacks O_NOFOLLOW support, or if we're changing to ".."
           (but not via a popped file descriptor).  When changing to the
           name "..", O_NOFOLLOW can't help.  In general, when the target is
           not "..", diropen's use of O_NOFOLLOW ensures we don't mistakenly
           follow a symlink, so we can avoid the expense of this fstat.  */
        int ret;
        if (ISSET(FTS_LOGICAL) || ! HAVE_WORKING_O_NOFOLLOW
            || (dir && streq (dir, "..")))
          {
            struct STRUCT_STAT sb;
            if (FSTAT (newfd, &sb))
              {
                ret = -1;
                goto bail;
              }
            if (p->fts_statp->st_dev != sb.st_dev
                || p->fts_statp->st_ino != sb.st_ino)
              {
                __set_errno (ENOENT);           /* disinformation */
                ret = -1;
                goto bail;
              }
          }

        if (ISSET(FTS_CWDFD))
          {
            cwd_advance_fd (sp, newfd, ! is_dotdot);
            return 0;
          }

        ret = fchdir(newfd);
bail:
        if (fd < 0)
          {
            int oerrno = errno;
            (void)close(newfd);
            __set_errno (oerrno);
          }
        return ret;
}
