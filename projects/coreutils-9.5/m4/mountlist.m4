# serial 17
dnl Copyright (C) 2002-2006, 2009-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Jim Meyering.

AC_PREREQ([2.60])

AC_DEFUN([gl_MOUNTLIST],
[
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_CHECK_FUNCS([listmntent])
  AC_CHECK_HEADERS_ONCE([sys/param.h sys/statvfs.h])

  # We must include grp.h before ucred.h on OSF V4.0, since ucred.h uses
  # NGROUPS (as the array dimension for a struct member) without a definition.
  AC_CHECK_HEADERS([sys/ucred.h], [], [], [#include <grp.h>])

  AC_CHECK_HEADERS([sys/mount.h], [], [],
    [AC_INCLUDES_DEFAULT
     [#if HAVE_SYS_PARAM_H
       #include <sys/param.h>
      #endif
    ]])

  AC_CHECK_HEADERS([mntent.h sys/fs_types.h])
  getfsstat_includes="\
$ac_includes_default
#if HAVE_SYS_PARAM_H
# include <sys/param.h> /* needed by powerpc-apple-darwin1.3.7 */
#endif
#if HAVE_SYS_UCRED_H
# include <grp.h> /* needed for definition of NGROUPS */
# include <sys/ucred.h> /* needed by powerpc-apple-darwin1.3.7 */
#endif
#if HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#if HAVE_SYS_FS_TYPES_H
# include <sys/fs_types.h> /* needed by powerpc-apple-darwin1.3.7 */
#endif
"
  AC_CHECK_MEMBERS([struct fsstat.f_fstypename],,,[$getfsstat_includes])

  # Determine how to get the list of mounted file systems.
  ac_list_mounted_fs=

  # If the getmntent function is available but not in the standard library,
  # make sure LIBS contains the appropriate -l option.
  AC_FUNC_GETMNTENT

  if test -z "$ac_list_mounted_fs"; then
    # AIX.
    AC_CACHE_CHECK([for mntctl function and struct vmount],
      [fu_cv_sys_mounted_vmount],
      [AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <fshelp.h>]])],
         [fu_cv_sys_mounted_vmount=yes],
         [fu_cv_sys_mounted_vmount=no])])
    if test $fu_cv_sys_mounted_vmount = yes; then
      ac_list_mounted_fs=found
      AC_DEFINE([MOUNTED_VMOUNT], [1],
        [Define if there is a function named mntctl that can be used to read
         the list of mounted file systems, and there is a system header file
         that declares 'struct vmount'.  (AIX)])
    fi
  fi

  if test $ac_cv_func_getmntent = yes; then

    # This system has the getmntent function.
    # Determine whether it's the one-argument variant or the two-argument one.

    if test -z "$ac_list_mounted_fs"; then
      # glibc, HP-UX, IRIX, Cygwin, Android, also (obsolete) 4.3BSD, SunOS.
      AC_CACHE_CHECK([for one-argument getmntent function],
        [fu_cv_sys_mounted_getmntent1],
        [AC_COMPILE_IFELSE(
           [AC_LANG_PROGRAM([[
/* SunOS 4.1.x /usr/include/mntent.h needs this for FILE */
#include <stdio.h>

#include <mntent.h>
#if defined __ANDROID__        /* Android */
# undef MOUNTED
# define MOUNTED "/proc/mounts"
#elif !defined MOUNTED
# if defined _PATH_MOUNTED     /* GNU libc  */
#  define MOUNTED _PATH_MOUNTED
# endif
# if defined MNT_MNTTAB        /* HP-UX.  */
#  define MOUNTED MNT_MNTTAB
# endif
#endif
]],
              [[struct mntent *mnt = 0; char *table = MOUNTED;
                if (sizeof mnt && sizeof table) return 0;
              ]])],
           [fu_cv_sys_mounted_getmntent1=yes],
           [fu_cv_sys_mounted_getmntent1=no])
        ])
      if test $fu_cv_sys_mounted_getmntent1 = yes; then
        ac_list_mounted_fs=found
        AC_DEFINE([MOUNTED_GETMNTENT1], [1],
          [Define if there is a function named getmntent for reading the list
           of mounted file systems, and that function takes a single argument.
           (4.3BSD, SunOS, HP-UX, Irix)])
        gl_CHECK_FUNCS_ANDROID([setmntent],
          [[#include <stdio.h>
            #include <mntent.h>
          ]])
        gl_CHECK_FUNCS_ANDROID([endmntent],
          [[#include <stdio.h>
            #include <mntent.h>
          ]])
        gl_CHECK_FUNCS_ANDROID([hasmntopt],
          [[#include <stdio.h>
            #include <mntent.h>
          ]])
      fi
    fi

    if test -z "$ac_list_mounted_fs"; then
      # Solaris >= 8.
      AC_CACHE_CHECK([for getextmntent function],
        [fu_cv_sys_mounted_getextmntent],
        [AC_EGREP_HEADER([getextmntent], [sys/mnttab.h],
           [fu_cv_sys_mounted_getextmntent=yes],
           [fu_cv_sys_mounted_getextmntent=no])])
      if test $fu_cv_sys_mounted_getextmntent = yes; then
        ac_list_mounted_fs=found
        AC_DEFINE([MOUNTED_GETEXTMNTENT], [1],
          [Define if there is a function named getextmntent for reading the list
           of mounted file systems.  (Solaris)])
      fi
    fi

    if test -z "$ac_list_mounted_fs"; then
      # Solaris < 8, also (obsolete) SVR4.
      # Solaris >= 8 has the two-argument getmntent but is already handled above.
      AC_CACHE_CHECK([for two-argument getmntent function],
        [fu_cv_sys_mounted_getmntent2],
        [AC_EGREP_HEADER([getmntent], [sys/mnttab.h],
           [fu_cv_sys_mounted_getmntent2=yes],
           [fu_cv_sys_mounted_getmntent2=no])
        ])
      if test $fu_cv_sys_mounted_getmntent2 = yes; then
        ac_list_mounted_fs=found
        AC_DEFINE([MOUNTED_GETMNTENT2], [1],
          [Define if there is a function named getmntent for reading the list of
           mounted file systems, and that function takes two arguments.  (SVR4)])
        AC_CHECK_FUNCS([hasmntopt])
      fi
    fi

  fi

  if test -z "$ac_list_mounted_fs"; then
    # OSF/1, also (obsolete) Apple Darwin 1.3.
    # powerpc-apple-darwin1.3.7 needs sys/param.h sys/ucred.h sys/fs_types.h

    AC_CACHE_CHECK([for getfsstat function],
      [fu_cv_sys_mounted_getfsstat],
      [AC_LINK_IFELSE(
         [AC_LANG_PROGRAM([[
#include <sys/types.h>
#if HAVE_STRUCT_FSSTAT_F_FSTYPENAME
# define FS_TYPE(Ent) ((Ent).f_fstypename)
#else
# define FS_TYPE(Ent) mnt_names[(Ent).f_type]
#endif
$getfsstat_includes
            ]],
            [[struct statfs *stats;
              int numsys = getfsstat ((struct statfs *)0, 0L, MNT_WAIT);
              char *t = FS_TYPE (*stats);
            ]])],
         [fu_cv_sys_mounted_getfsstat=yes],
         [fu_cv_sys_mounted_getfsstat=no])
      ])
    if test $fu_cv_sys_mounted_getfsstat = yes; then
      ac_list_mounted_fs=found
      AC_DEFINE([MOUNTED_GETFSSTAT], [1],
        [Define if there is a function named getfsstat for reading the
         list of mounted file systems.  (DEC Alpha running OSF/1)])
    fi
  fi

  if test -z "$ac_list_mounted_fs"; then
    # (obsolete) SVR3
    AC_CACHE_CHECK([for FIXME existence of three headers],
      [fu_cv_sys_mounted_fread_fstyp],
      [AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#include <sys/statfs.h>
#include <sys/fstyp.h>
#include <mnttab.h>
]])],
         [fu_cv_sys_mounted_fread_fstyp=yes],
         [fu_cv_sys_mounted_fread_fstyp=no])
      ])
    if test $fu_cv_sys_mounted_fread_fstyp = yes; then
      ac_list_mounted_fs=found
      AC_DEFINE([MOUNTED_FREAD_FSTYP], [1],
        [Define if (like SVR2) there is no specific function for reading the
         list of mounted file systems, and your system has these header files:
         <sys/fstyp.h> and <sys/statfs.h>.  (SVR3)])
    fi
  fi

  if test -z "$ac_list_mounted_fs"; then
    # Mac OS X, FreeBSD, NetBSD, OpenBSD, Minix, also (obsolete) 4.4BSD.
    # OSF/1 also has getmntinfo but is already handled above.
    # We cannot use AC_CHECK_FUNCS([getmntinfo]) here, because at the linker
    # level the function is sometimes called getmntinfo64 or getmntinfo$INODE64
    # on Mac OS X, __getmntinfo13 on NetBSD and Minix, _F64_getmntinfo on OSF/1.
    AC_CACHE_CHECK([for getmntinfo function],
      [fu_cv_sys_mounted_getmntinfo],
      [AC_LINK_IFELSE(
         [AC_LANG_PROGRAM([[
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#if HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#include <stdlib.h>
            ]],
            [[int count = getmntinfo (NULL, MNT_WAIT);
            ]])],
         [fu_cv_sys_mounted_getmntinfo=yes],
         [fu_cv_sys_mounted_getmntinfo=no])
      ])
    if test $fu_cv_sys_mounted_getmntinfo = yes; then
      AC_CACHE_CHECK([whether getmntinfo returns statvfs structures],
        [fu_cv_sys_mounted_getmntinfo2],
        [AC_COMPILE_IFELSE(
           [AC_LANG_PROGRAM([[
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#if HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
extern
#ifdef __cplusplus
"C"
#endif
int getmntinfo (struct statfs **, int);
              ]], [[]])],
           [fu_cv_sys_mounted_getmntinfo2=no],
           [fu_cv_sys_mounted_getmntinfo2=yes])
        ])
      if test $fu_cv_sys_mounted_getmntinfo2 = no; then
        # Mac OS X, FreeBSD, OpenBSD, also (obsolete) 4.4BSD.
        ac_list_mounted_fs=found
        AC_DEFINE([MOUNTED_GETMNTINFO], [1],
          [Define if there is a function named getmntinfo for reading the
           list of mounted file systems and it returns an array of
           'struct statfs'.  (4.4BSD, Darwin)])
      else
        # NetBSD, Minix.
        ac_list_mounted_fs=found
        AC_DEFINE([MOUNTED_GETMNTINFO2], [1],
          [Define if there is a function named getmntinfo for reading the
           list of mounted file systems and it returns an array of
           'struct statvfs'.  (NetBSD 3.0)])
      fi
    fi
  fi

  if test -z "$ac_list_mounted_fs"; then
    # Haiku, also (obsolete) BeOS.
    AC_CHECK_FUNCS([next_dev fs_stat_dev])
    AC_CHECK_HEADERS([fs_info.h])
    AC_CACHE_CHECK([for BEOS mounted file system support functions],
      [fu_cv_sys_mounted_fs_stat_dev],
      [if test $ac_cv_header_fs_info_h = yes \
          && test $ac_cv_func_next_dev = yes \
          && test $ac_cv_func_fs_stat_dev = yes; then
         fu_cv_sys_mounted_fs_stat_dev=yes
       else
         fu_cv_sys_mounted_fs_stat_dev=no
       fi
      ])
    if test $fu_cv_sys_mounted_fs_stat_dev = yes; then
      ac_list_mounted_fs=found
      AC_DEFINE([MOUNTED_FS_STAT_DEV], [1],
        [Define if there are functions named next_dev and fs_stat_dev for
         reading the list of mounted file systems.  (BeOS)])
    fi
  fi

  if test -z "$ac_list_mounted_fs"; then
    # Interix / BSD alike statvfs
    # the code is really interix specific, so make sure, we're on it.
    case "$host" in
      *-interix*)
        AC_CHECK_FUNCS([statvfs])
        if test $ac_cv_func_statvfs = yes; then
          ac_list_mounted_fs=found
          AC_DEFINE([MOUNTED_INTERIX_STATVFS], [1],
            [Define if we are on interix, and ought to use statvfs plus
             some special knowledge on where mounted file systems can be
             found. (Interix)])
        fi
        ;;
    esac
  fi

  if test -z "$ac_list_mounted_fs"; then
    AC_MSG_ERROR([could not determine how to read list of mounted file systems])
    # FIXME -- no need to abort building the whole package
    # Can't build mountlist.c or anything that needs its functions
  fi

  if test $ac_list_mounted_fs = found; then
    gl_cv_list_mounted_fs=yes
  else
    gl_cv_list_mounted_fs=no
  fi
])

# Prerequisites of lib/mountlist.c not done by gl_MOUNTLIST.
AC_DEFUN([gl_PREREQ_MOUNTLIST_EXTRA],
[
  dnl Note gl_MOUNTLIST checks for mntent.h, not sys/mntent.h.
  AC_CHECK_HEADERS([sys/mntent.h])
  AC_HEADER_MAJOR()dnl for use of makedev ()
  gl_FSTYPENAME
])

# Replace Autoconf's AC_FUNC_GETMNTENT to omit checks that are unnecessary
# nowadays.
AC_DEFUN([AC_FUNC_GETMNTENT],
[
  # getmntent is in the standard C library on most systems, but in -lgen on
  # Unixware.
  AC_SEARCH_LIBS([getmntent], [gen])
  AC_CHECK_FUNCS([getmntent])
])
