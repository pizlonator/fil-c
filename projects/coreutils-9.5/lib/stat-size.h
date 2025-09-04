/* Yield size-related values in struct stat.
   Copyright (C) 1989, 1991-2024 Free Software Foundation, Inc.

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

#ifndef STAT_SIZE_H
#define STAT_SIZE_H

/* This file uses HAVE_SYS_PARAM_H, HAVE_STRUCT_STAT_ST_BLOCKS.  */
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

/* sys/param.h may define DEV_BSIZE */
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

/* The device blocksize, or a faked version thereof.
   But use ST_NBLOCKSIZE instead.
   Usually defined by sys/param.h (if at all).  */
#if !defined DEV_BSIZE && defined BSIZE
# define DEV_BSIZE BSIZE
#endif
#if !defined DEV_BSIZE && defined BBSIZE /* SGI sys/param.h */
# define DEV_BSIZE BBSIZE
#endif
#ifndef DEV_BSIZE
# define DEV_BSIZE 4096
#endif

/* Extract or fake data from a struct stat *st.
   STP_BLKSIZE(st): Preferred performance I/O blocksize, in bytes.
   STP_NBLOCKS(st): Number of blocks, including indirect blocks.
   ST_NBLOCKSIZE: Size of blocks used when calculating ST_NBLOCKS.  */
#ifndef HAVE_STRUCT_STAT_ST_BLOCKS
# define STP_BLKSIZE(st) DEV_BSIZE
  /* coreutils' fileblocks.c also uses BSIZE.  */
# if defined _POSIX_SOURCE || !defined BSIZE
#  define STP_NBLOCKS(st) \
    ((st)->st_size / ST_NBLOCKSIZE + ((st)->st_size % ST_NBLOCKSIZE != 0))
# else
   /* This definition calls st_blocks, which is in the fileblocks module. */
#  define STP_NBLOCKS(st) \
    (S_ISREG ((st)->st_mode) || S_ISDIR ((st)->st_mode) \
     ? st_blocks ((st)->st_size) : 0)
# endif
#else
/* When running 'rsh hpux11-system cat any-file', cat would
   determine that the output stream had an st_blksize of 2147421096.
   Conversely st_blksize can be 2 GiB (or maybe even larger) with XFS
   on 64-bit hosts.  Somewhat arbitrarily, limit the "optimal" block
   size to SIZE_MAX / 8 + 1.  (Dividing SIZE_MAX by only 4 wouldn't
   suffice, since "cat" sometimes multiplies the result by 4.)  If
   anyone knows of a system for which this limit is too small, please
   report it as a bug in this code.  */
# define STP_BLKSIZE(st) ((0 < (st)->st_blksize \
                           && (st)->st_blksize <= (size_t) -1 / 8 + 1)  \
                          ? (st)->st_blksize : DEV_BSIZE)
# if defined hpux || defined __hpux__ || defined __hpux
  /* HP-UX counts st_blocks in 1024-byte units.
     This loses when mixing HP-UX and BSD file systems with NFS.  */
#  define ST_NBLOCKSIZE 1024
# endif
#endif

#ifndef STP_NBLOCKS
# define STP_NBLOCKS(st) ((st)->st_blocks)
#endif

#ifndef ST_NBLOCKSIZE
# ifdef S_BLKSIZE
#  define ST_NBLOCKSIZE S_BLKSIZE
# else
#  define ST_NBLOCKSIZE 512
# endif
#endif

/* Preferred blocksize and number of blocks for a struct stat object.  */
#define ST_BLKSIZE(statbuf) STP_BLKSIZE (&(statbuf))
#define ST_NBLOCKS(statbuf) STP_NBLOCKS (&(statbuf))

#endif /* STAT_SIZE_H */
