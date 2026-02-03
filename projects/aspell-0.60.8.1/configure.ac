AC_INIT(GNU Aspell, 0.60.8.1)
AC_CONFIG_SRCDIR(prog/aspell.cpp)
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(gen/settings.h)

if test -e "$srcdir"/autogen -a "$enable_maintainer_mode" != "no"
then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE

AC_ARG_ENABLE(docdir, 
  AS_HELP_STRING([--enable-docdir=DIR],[documentation files in DIR @<:@PREFIX/share/doc/aspell@:>@]),
  pkgdocdir=$enable_docdir, pkgdocdir=\${prefix}/share/doc/aspell)
AC_SUBST(pkgdocdir)

dnl pkgdatadir

pkgdatadir=undef
AC_ARG_ENABLE(pkgdatadir, 
  AS_HELP_STRING([--enable-pkgdatadir=DIR],[device dependent data files @<:@LIBDIR/aspell-0.60@:>@]),
  pkgdatadir=$enable_pkgdatadir)

AC_ARG_ENABLE(pkgdata-dir, 
  AS_HELP_STRING([--enable-data-dir=DIR],[alias for pkgdatadir]),
  pkgdatadir=$enable_dict_dir)

if test "$pkgdatadir" = "undef"
then
  pkgdatadir=\${libdir}/aspell-0.60
fi

AC_SUBST(pkgdatadir)

dnl pkglibdir

pkglibdir=undef
AC_ARG_ENABLE(pkglibdir, 
  AS_HELP_STRING([--enable-pkglibdir=DIR],[device dependent data files @<:@LIBDIR/aspell-0.60@:>@]),
  pkglibdir=$enable_pkglibdir)

AC_ARG_ENABLE(dict-dir, 
  AS_HELP_STRING([--enable-dict-dir=DIR],[alias for pkglibdir]),
  pkglibdir=$enable_dict_dir)

if test "$pkglibdir" = "undef"
then
  pkglibdir=\${libdir}/aspell-0.60
fi

AC_SUBST(pkglibdir)

dnl optional features

AC_ARG_ENABLE(win32-relocatable,
  [  --enable-win32-relocatable])

AC_ARG_ENABLE(curses,           
  AS_HELP_STRING([--enable-curses=LIBFILE],[cursor control library]))

AC_ARG_ENABLE(curses-include,           
  [  --enable-curses-include=DIR])

AC_ARG_ENABLE(wide-curses,
  AS_HELP_STRING([--disable-wide-curses],[disable wide char utf8 cursor control]))

AC_ARG_ENABLE(regex,
  [  --disable-regex])

AC_ARG_ENABLE(compile-in-filters,
  [  --enable-compile-in-filters])

AC_ARG_ENABLE(filter-version-control,
  [  --disable-filter-version-control])

AC_ARG_ENABLE(32-bit-hash-fun,
  AS_HELP_STRING([--enable-32-bit-hash-fun],[use 32-bit hash function for compiled dictionaries]))

AC_ARG_ENABLE(sloppy-null-term-strings,
  AS_HELP_STRING([--enable-sloppy-null-term-strings],[allows allow null terminated UCS-2 and UCS-4 strings]))

AC_ARG_ENABLE(pspell-compatibility,
  AS_HELP_STRING([--disable-pspell-compatibility],[don't install pspell compatibility libraries]))

AC_ARG_ENABLE(incremented-soname,
  AS_HELP_STRING([--enable-incremented-soname],[break aspell 0.50 binary compatibility]))

dnl AC_ARG_ENABLE(aspell5-compatibility,
dnl [  --enable-aspell5-compatibility  install aspell 0.50 compatibility libraries])

AC_ARG_ENABLE(w-all-error,
  AS_HELP_STRING([--enable-w-all-error],[selectively enable -Wall and -Werror]))

dnl

AC_PROG_CXX
if test "$GXX" = "yes" && expr x"$CXXFLAGS" : '.*-O' > /dev/null
then
  CXXFLAGS="$CXXFLAGS -fno-exceptions"
fi

AC_LANG([C++])

AM_PROG_CC_C_O

AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

dnl DL stuff

AC_CHECK_HEADERS(dlfcn.h,,[enable_compile_in_filters=yes])
AC_CHECK_FUNC(dlopen,,
              AC_CHECK_LIB(dl, dlopen,,[enable_compile_in_filters=yes]))

dnl

AC_CHECK_PROG(SED,sed,sed)
AC_PATH_PROG(PERLPROG,perl)

dnl

if test "$enable_static" = "yes"
then
  enable_compile_in_filters=yes
fi

if test "$enable_compile_in_filters" = "yes"
then
  AC_DEFINE(COMPILE_IN_FILTER, 1, [Defined if filters should be compiled in])
fi

find_git=`expr "$PACKAGE_VERSION" : '.*git'`
if test "$find_git" -gt 0
then
  enable_filter_version_control=no
fi

if test "$enable_filter_version_control" != "no"
then
  AC_DEFINE(FILTER_VERSION_CONTROL, 1, 
            [Defined if filter version control should be used])
fi

AM_CONDITIONAL(COMPILE_IN_FILTERS, 
  [test "$enable_compile_in_filters" = "yes"])

if test "$enable_32_bit_hash_fun" = "yes"
then
  AC_DEFINE(USE_32_BIT_HASH_FUN, 1, [Defined if 32-bit hash function should be used for compiled dictionaries.])
fi

if test "$enable_sloppy_null_term_strings" = "yes"
then
  AC_DEFINE(SLOPPY_NULL_TERM_STRINGS, 1, [Defined if null-terminated UCS-2 and UCS-4 strings should always be allowed.])
fi

AM_CONDITIONAL(PSPELL_COMPATIBILITY,  
  [test "$enable_pspell_compatibility" != "no"])
AM_CONDITIONAL(INCREMENTED_SONAME,    
  [test "$enable_incremented_soname" = "yes"])
dnl AM_CONDITIONAL(ASPELL5_COMPATIBILITY,
dnl   [test "$enable_incremented_soname" != "no" -a \
dnl         "$enable_aspell5_compatibility" = "yes"])
AM_CONDITIONAL(W_ALL_ERROR,
  [test "$enable_w_all_error" = "yes"])

dnl GETTEXT

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.3])

AH_TOP([#ifndef ASPELL_SETTINGS__H
#define ASPELL_SETTINGS__H])
AH_BOTTOM([#define C_EXPORT extern "C"])
AH_BOTTOM([#endif /* ASPELL_SETTINGS__H */])

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                     Platform Specific Tests                     #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

if test "$enable_win32_relocatable" = "yes"
then
  AC_DEFINE(ENABLE_WIN32_RELOCATABLE, 1, [Defined if win32 relocation should be used])
fi

# DL stuff
AC_CHECK_HEADERS(dlfcn.h)
AC_CHECK_LIB(dl, dlopen)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                           Posix tests                           #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_MSG_CHECKING(if file locking and truncating is supported)
AC_TRY_LINK( 
  [#include <fcntl.h>
   #include <unistd.h>],
  [int fd;	
   struct flock fl;
   fcntl(fd, F_SETLKW, &fl);
   ftruncate(fd,0);],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(USE_FILE_LOCKS, 1,
             [Defined if file locking and truncating is supported])
  ],
  [AC_MSG_RESULT(no)] )

AC_MSG_CHECKING(if mmap and friends is supported)
AC_TRY_LINK(
  [#include <fcntl.h>
   #include <unistd.h>
   #include <sys/mman.h>],
  [char * p = (char *)mmap(NULL, 10, PROT_READ, MAP_SHARED, -1, 2);
   munmap(p,10);],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(HAVE_MMAP, 1, [Defined if mmap and friends is supported])],
  [AC_MSG_RESULT(no)] )

AC_MSG_CHECKING(if file ino is supported)
touch conftest-f1
touch conftest-f2
AC_TRY_RUN(
  [#include <stdlib.h>
   #include <sys/stat.h>
   int main() {
     struct stat s1,s2;
     if (stat("conftest-f1",&s1) != 0) exit(2);
     if (stat("conftest-f2",&s2) != 0) exit(2);
     exit (s1.st_ino != s2.st_ino ? 0 : 1);
   }
  ],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(USE_FILE_INO, 1, [Defined if file ino is supported])
  ],
  [AC_MSG_RESULT(no)],
  [if test "$MINGW32" = "yes"
   then
     AC_MSG_RESULT(cant run test!, assuming no)
   else
     AC_MSG_RESULT(cant run test!, assuming yes)
     AC_DEFINE(USE_FILE_INO, 1, [Defined if file ino is supported])
   fi
  ] )

AC_MSG_CHECKING(if posix locals are supported)
AC_TRY_COMPILE(
  [#include <locale.h>],
  [setlocale (LC_ALL, NULL);
   setlocale (LC_MESSAGES, NULL);],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(USE_LOCALE, 1, [Defined if Posix locales are supported])],
  [AC_MSG_RESULT(no)] )

if test "$enable_regex" != "no"
then
  AC_MSG_CHECKING(if posix regex are supported)
  AC_TRY_LINK(
    [#include <sys/types.h>
     #include <regex.h>],
    [regex_t r;
     regcomp(&r, "", REG_EXTENDED);
     regexec(&r, "", 0, 0, 0);],
    [AC_MSG_RESULT(yes)
     AC_DEFINE(USE_POSIX_REGEX, 1, [Defined if Posix regex are supported])],
    [AC_MSG_RESULT(no)])
fi

AM_LANGINFO_CODESET

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                   Posix lock function tests                     #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_SUBST(PTHREAD_LIB)

AC_MSG_CHECKING(if posix mutexes are supported)

ORIG_LIBS="$LIBS"

for l in '' '-lpthread' 
do 
  if test -z "$use_posix_mutex"
  then
    LIBS="$l $ORIG_LIBS"
    AC_TRY_LINK(
      [#include <pthread.h>],
      [pthread_mutex_t lck;
       pthread_mutex_init(&lck, 0);
       pthread_mutex_lock(&lck);
       pthread_mutex_unlock(&lck);
       pthread_mutex_destroy(&lck);],
      [PTHREAD_LIB=$l
       use_posix_mutex=1])
  fi
done

LIBS="$ORIG_LIBS"

if test "$use_posix_mutex"
then
  if test -z "$PTHREAD_LIB"
  then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT([yes (in $PTHREAD_LIB)])
  fi
  AC_DEFINE(USE_POSIX_MUTEX, 1, [Defined if Posix mutexes are supported])
else
  AC_MSG_RESULT(no)
  AC_MSG_WARN([Unable to find locking mechanism, Aspell will not be thread safe.])
fi


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                     Terminal function tests                     #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_MSG_CHECKING(if mblen is supported)
AC_TRY_LINK(
  [#include <wchar.h>
   #include <stddef.h>
   #include <stdlib.h>],
  [size_t s = mblen("bla", MB_CUR_MAX);],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(HAVE_MBLEN, 1, [Defined if mblen is supported])
   have_mblen=1],
  [AC_MSG_RESULT(no)]
)

AC_SUBST(CURSES_LIB)
AC_SUBST(CURSES_INCLUDE)

if test "$enable_curses" != "no"
then
  use_curses=t
  case "$enable_curses" in
    yes | ""                     )                             ;;
    /* | *lib* | *.a | -l* | -L* ) CURSES_LIB="$enable_curses" ;;
    *                            ) CURSES_LIB=-l$enable_curses ;;
  esac
  case "$enable_curses_include" in
    yes | no | "")                                         ;;
    -I*          ) CURSES_INCLUDE="$enable_curses_include" ;;
    *            ) CURSES_INCLUDE=-I$enable_curses_include ;;
  esac
fi

if test "$use_curses"
then

  ORIG_LIBS="$LIBS"
  ORIG_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CURSES_INCLUDE $ORIG_CPPFLAGS"

  if test -z "$CURSES_LIB"
  then

    AC_MSG_CHECKING(for working curses library)

    if test "$enable_wide_curses" != "no" -a -n "$have_mblen"
    then
      LIBS="-lncursesw $ORIG_LIBS"
      AC_TRY_LINK(
        [#include <ncursesw/curses.h>], [initscr()],
        [CURSES_LIB=-lncursesw
         AC_DEFINE(CURSES_HEADER, <ncursesw/curses.h>, [Defined to curses header file])
         AC_DEFINE(TERM_HEADER, <ncursesw/term.h>, [Defined to term header file])])
    fi

    if test -z "$CURSES_LIB"
    then
      LIBS="-lncurses $ORIG_LIBS"
      AC_TRY_LINK(
        [#include <ncurses/curses.h>], [initscr()],
        [CURSES_LIB=-lncurses
         AC_DEFINE(CURSES_HEADER, <ncurses/curses.h>, [Defined to curses header file])
         AC_DEFINE(TERM_HEADER, <ncurses/term.h>, [Defined to term header file])],
        [
      LIBS="-lncurses $ORIG_LIBS"
      AC_TRY_LINK(
        [#include <ncurses.h>], [initscr()],
        [CURSES_LIB=-lncurses
         AC_DEFINE(CURSES_HEADER, <ncurses.h>, [Defined to curses header file])
         AC_DEFINE(TERM_HEADER, <term.h>, [Defined to term header file])],
        [
      LIBS="-lcurses $ORIG_LIBS"
      AC_TRY_LINK(
        [#include <curses.h>], [initscr()],
        [CURSES_LIB=-lcurses
         AC_DEFINE(CURSES_HEADER, <curses.h>, [Defined to curses header file])
         AC_DEFINE(TERM_HEADER, <term.h>, [Defined to term header file])],
        [
      LIBS="-lncurses $ORIG_LIBS"
      AC_TRY_LINK(
        [#include <curses.h>], [initscr()],
        [CURSES_LIB=-lncurses
         AC_DEFINE(CURSES_HEADER, <curses.h>, [Defined to curses header file])
         AC_DEFINE(TERM_HEADER, <term.h>, [Defined to term header file])],
      ) ]) ]) ])
    fi

    if test -n "$CURSES_LIB"
    then
      AC_MSG_RESULT([found in $CURSES_LIB])
    else
      AC_MSG_RESULT([not found])
    fi

  else

    AC_DEFINE(CURSES_HEADER, <curses.h>, [Defined to curses header file])
    AC_DEFINE(TERM_HEADER, <term.h>, [Defined to term header file])

  fi

  if test -n "$CURSES_LIB"
  then
     LIBS="$CURSES_LIB $ORIG_LIBS"

     if test "$enable_wide_curses" != "no"
     then

       AC_MSG_CHECKING(for wide character support in curses libraray)
       if test -n "$have_mblen"
       then
         AC_TRY_LINK(
           [#include <wchar.h>
            #include CURSES_HEADER
           ],
           [wchar_t wch = 0;
            addnwstr(&wch, 1);],
           [AC_MSG_RESULT(yes)
            AC_DEFINE(HAVE_WIDE_CURSES, 1, [Defined if curses libraray includes wide character support])],
           [

         AC_TRY_LINK(
           [#define _XOPEN_SOURCE_EXTENDED 1
            #include <wchar.h>
            #include CURSES_HEADER
           ],
           [wchar_t wch = 0;
            addnwstr(&wch, 1);],
           [AC_MSG_RESULT(yes)
            AC_DEFINE(HAVE_WIDE_CURSES, 1)
            AC_DEFINE(DEFINE_XOPEN_SOURCE_EXTENDED, 1, 
                      [Defined if _XOPEN_SOURCE_EXTENDED needs to be defined. 
                       (Can't define globally as that will cause problems with some systems)])
           ],
           [AC_MSG_RESULT(no)
            AC_MSG_WARN([Aspell will not be able to Display UTF-8 characters correctly.])])])
       else
         AC_MSG_RESULT([no, because "mblen" is not supported])
         AC_MSG_WARN([Aspell will not be able to Display UTF-8 characters correctly.])
       fi

     fi
 
     AC_MSG_CHECKING(if standard curses include sequence will work)
     AC_TRY_LINK(
       [#ifdef DEFINE_XOPEN_SOURCE_EXTENDED
        #  define _XOPEN_SOURCE_EXTENDED 1
        #endif
        #include <termios.h>
	#include <unistd.h>
        #include CURSES_HEADER
	#include TERM_HEADER
       ],
       [tigetstr(const_cast<char *>("cup"));],
       [AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_LIBCURSES, 1, 
                  [Defined if the curses library is available])
	posix_termios=t
	AC_DEFINE(CURSES_INCLUDE_STANDARD, 1, 
                  [Defined if no special Workarounds are needed for Curses headers])],
       [AC_MSG_RESULT(no)

     dnl else if
     AC_MSG_CHECKING(if curses workaround I will work)
     AC_TRY_LINK(
       [#ifdef DEFINE_XOPEN_SOURCE_EXTENDED
        #  define _XOPEN_SOURCE_EXTENDED 1
        #endif
        #include <termios.h>
	#include <unistd.h>
        #include CURSES_HEADER
	extern "C" {char * tigetstr(char * capname);}],
       [tigetstr(const_cast<char *>("cup"));],
       [AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_LIBCURSES, 1, [])
        posix_termios=t
	AC_DEFINE(CURSES_INCLUDE_WORKAROUND_1, 1, 
                  [Defined if special Wordaround I is need for Curses headers])],
       [AC_MSG_RESULT(no)

     dnl else if
     AC_MSG_CHECKING(if curses without Unix stuff will work)
     AC_TRY_LINK(
       [#include CURSES_HEADER
       ],
       [initscr();],
       [AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_LIBCURSES, 1, [])
	AC_DEFINE(CURSES_ONLY, 1, 
                  [Defined if curses like POSIX Functions should be used])
        curses_only=t],
       [AC_MSG_RESULT(no)

     dnl else
     use_curses=false
     CURSES_LIBS=""
     CURSES_INCLUDE=""

     ]) ]) ])

  fi

  CPPFLAGS="$ORIG_CPPFLAGS"
  LIBS="$ORIG_LIBS"

fi

if test -z "$posix_termios" -a -z "$curses_only"
then
  AC_MSG_CHECKING(if posix termios is supported)
  AC_TRY_LINK(
    [#include <termios.h>
     #include <unistd.h>
     #include <stdlib.h>],
    [isatty (STDIN_FILENO);
     atexit(0);
     termios attrib;
     tcgetattr (STDIN_FILENO, &attrib);
     tcsetattr (STDIN_FILENO, TCSAFLUSH, &attrib);],
    [AC_MSG_RESULT(yes)
     posix_termios=t],
    [AC_MSG_RESULT(no)]
  )
fi

if test -z "$posix_termios" -a -z "$use_curses"
then
  dnl else if
  AC_MSG_CHECKING(if getch is supported)
  AC_TRY_LINK(
    [extern "C" {int getch();}],
    [char c = getch();],
    [AC_MSG_RESULT(yes)
     AC_DEFINE(HAVE_GETCH, 1, [Defined if msdos getch is supported])
    ],
    [AC_MSG_RESULT(no)]
  )
fi

if test "$posix_termios"
then
  AC_DEFINE(POSIX_TERMIOS, 1, [Defined if Posix Termios is Supported])
fi

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                    Compiler Quirks Tests                        #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_MSG_CHECKING(for STL rel_ops pollution)
AC_TRY_COMPILE(
  [#include <utility>

   template <typename T>
   class C {};

   template <typename T>
   bool operator== (C<T>, C<T>) {return true;}

   template <typename T>
   bool operator!= (C<T>, C<T>) {return false;}],
  [C<int> c1, c2;
   bool v = c1 != c2;],
  [AC_MSG_RESULT(no)],
  [AC_MSG_RESULT(yes)
   AC_DEFINE(REL_OPS_POLLUTION, 1, 
             [Defined if STL rel_ops pollute the global namespace])
  ]
)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                                                                 #
#                           Output                                #
#                                                                 #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

AC_CONFIG_FILES([Makefile gen/Makefile common/Makefile lib/Makefile data/Makefile
                 auto/Makefile modules/Makefile modules/tokenizer/Makefile
                 modules/speller/Makefile modules/speller/default/Makefile
		 interfaces/Makefile interfaces/cc/Makefile
		 scripts/Makefile examples/Makefile
		 prog/Makefile manual/Makefile po/Makefile.in m4/Makefile
                 modules/filter/Makefile myspell/Makefile lib5/Makefile
                 ])
AC_OUTPUT
