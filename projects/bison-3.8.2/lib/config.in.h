/* lib/config.in.h.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
#undef AC_APPLE_UNIVERSAL_BUILD

/* Define to the number of bits in type 'ptrdiff_t'. */
#undef BITSIZEOF_PTRDIFF_T

/* Define to the number of bits in type 'sig_atomic_t'. */
#undef BITSIZEOF_SIG_ATOMIC_T

/* Define to the number of bits in type 'size_t'. */
#undef BITSIZEOF_SIZE_T

/* Define to the number of bits in type 'wchar_t'. */
#undef BITSIZEOF_WCHAR_T

/* Define to the number of bits in type 'wint_t'. */
#undef BITSIZEOF_WINT_T

/* Define if you wish *printf() functions that have a safe handling of
   non-IEEE-754 'long double' values. */
#undef CHECK_PRINTF_SAFE

/* Define to 1 if using 'alloca.c'. */
#undef C_ALLOCA

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'double'. */
#undef DBL_EXPBIT0_BIT

/* Define as the word index where to find the exponent of 'double'. */
#undef DBL_EXPBIT0_WORD

/* Define as the bit index in the word where to find the sign of 'double'. */
#undef DBL_SIGNBIT_BIT

/* Define as the word index where to find the sign of 'double'. */
#undef DBL_SIGNBIT_WORD

/* the name of the file descriptor member of DIR */
#undef DIR_FD_MEMBER_NAME

#ifdef DIR_FD_MEMBER_NAME
# define DIR_TO_FD(Dir_p) ((Dir_p)->DIR_FD_MEMBER_NAME)
#else
# define DIR_TO_FD(Dir_p) -1
#endif


/* Define to 1 if // is a file system root distinct from /. */
#undef DOUBLE_SLASH_IS_DISTINCT_ROOT

/* Define if struct dirent has a member d_ino that actually works. */
#undef D_INO_IN_DIRENT

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#undef ENABLE_NLS

/* Define to 1 if the package shall run at any location in the file system. */
#undef ENABLE_RELOCATABLE

/* Define this to 1 if F_DUPFD behavior does not match POSIX */
#undef FCNTL_DUPFD_BUGGY

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'float'. */
#undef FLT_EXPBIT0_BIT

/* Define as the word index where to find the exponent of 'float'. */
#undef FLT_EXPBIT0_WORD

/* Define as the bit index in the word where to find the sign of 'float'. */
#undef FLT_SIGNBIT_BIT

/* Define as the word index where to find the sign of 'float'. */
#undef FLT_SIGNBIT_WORD

/* Define to 1 if fopen() fails to recognize a trailing slash. */
#undef FOPEN_TRAILING_SLASH_BUG

/* Define to 1 if realpath() can malloc memory, always gives an absolute path,
   and handles a trailing slash correctly. */
#undef FUNC_REALPATH_NEARLY_WORKS

/* Define to 1 if realpath() can malloc memory, always gives an absolute path,
   and handles leading slashes and a trailing slash correctly. */
#undef FUNC_REALPATH_WORKS

/* Define this to 'void' or 'struct timezone' to match the system's
   declaration of the second argument to gettimeofday. */
#undef GETTIMEOFDAY_TIMEZONE

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module canonicalize shall be considered present. */
#undef GNULIB_CANONICALIZE

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module canonicalize-lgpl shall be considered present. */
#undef GNULIB_CANONICALIZE_LGPL

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module close-stream shall be considered present. */
#undef GNULIB_CLOSE_STREAM

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module dirname shall be considered present. */
#undef GNULIB_DIRNAME

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fdopendir shall be considered present. */
#undef GNULIB_FDOPENDIR

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fd-safer-flag shall be considered present. */
#undef GNULIB_FD_SAFER_FLAG

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fopen-gnu shall be considered present. */
#undef GNULIB_FOPEN_GNU

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fopen-safer shall be considered present. */
#undef GNULIB_FOPEN_SAFER

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fscanf shall be considered present. */
#undef GNULIB_FSCANF

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module getcwd shall be considered present. */
#undef GNULIB_GETCWD

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module lock shall be considered present. */
#undef GNULIB_LOCK

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module msvc-nothrow shall be considered present. */
#undef GNULIB_MSVC_NOTHROW

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module openat shall be considered present. */
#undef GNULIB_OPENAT

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module pipe2-safer shall be considered present. */
#undef GNULIB_PIPE2_SAFER

/* Define to 1 if printf and friends should be labeled with attribute
   "__gnu_printf__" instead of "__printf__" */
#undef GNULIB_PRINTF_ATTRIBUTE_FLAVOR_GNU

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module reallocarray shall be considered present. */
#undef GNULIB_REALLOCARRAY

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module scanf shall be considered present. */
#undef GNULIB_SCANF

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module snprintf shall be considered present. */
#undef GNULIB_SNPRINTF

/* Define to 1 if you want the FILE stream functions getc, putc, etc. to use
   unlocked I/O if available, throughout the package. Unlocked I/O can improve
   performance, sometimes dramatically. But unlocked I/O is safe only in
   single-threaded programs, as well as in multithreaded programs for which
   you can guarantee that every FILE stream, including stdin, stdout, stderr,
   is used only in a single thread. */
#undef GNULIB_STDIO_SINGLE_THREAD

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module strerror shall be considered present. */
#undef GNULIB_STRERROR

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module strerror_r-posix shall be considered present. */
#undef GNULIB_STRERROR_R_POSIX

/* Define to 1 when the gnulib module access should be tested. */
#undef GNULIB_TEST_ACCESS

/* Define to 1 when the gnulib module calloc-posix should be tested. */
#undef GNULIB_TEST_CALLOC_POSIX

/* Define to 1 when the gnulib module canonicalize should be tested. */
#undef GNULIB_TEST_CANONICALIZE

/* Define to 1 when the gnulib module canonicalize_file_name should be tested.
   */
#undef GNULIB_TEST_CANONICALIZE_FILE_NAME

/* Define to 1 when the gnulib module chdir should be tested. */
#undef GNULIB_TEST_CHDIR

/* Define to 1 when the gnulib module cloexec should be tested. */
#undef GNULIB_TEST_CLOEXEC

/* Define to 1 when the gnulib module close should be tested. */
#undef GNULIB_TEST_CLOSE

/* Define to 1 when the gnulib module closedir should be tested. */
#undef GNULIB_TEST_CLOSEDIR

/* Define to 1 when the gnulib module dirfd should be tested. */
#undef GNULIB_TEST_DIRFD

/* Define to 1 when the gnulib module dup should be tested. */
#undef GNULIB_TEST_DUP

/* Define to 1 when the gnulib module dup2 should be tested. */
#undef GNULIB_TEST_DUP2

/* Define to 1 when the gnulib module environ should be tested. */
#undef GNULIB_TEST_ENVIRON

/* Define to 1 when the gnulib module fchdir should be tested. */
#undef GNULIB_TEST_FCHDIR

/* Define to 1 when the gnulib module fcntl should be tested. */
#undef GNULIB_TEST_FCNTL

/* Define to 1 when the gnulib module fdopendir should be tested. */
#undef GNULIB_TEST_FDOPENDIR

/* Define to 1 when the gnulib module ffsl should be tested. */
#undef GNULIB_TEST_FFSL

/* Define to 1 when the gnulib module fgetc should be tested. */
#undef GNULIB_TEST_FGETC

/* Define to 1 when the gnulib module fgets should be tested. */
#undef GNULIB_TEST_FGETS

/* Define to 1 when the gnulib module fopen should be tested. */
#undef GNULIB_TEST_FOPEN

/* Define to 1 when the gnulib module fprintf should be tested. */
#undef GNULIB_TEST_FPRINTF

/* Define to 1 when the gnulib module fprintf-posix should be tested. */
#undef GNULIB_TEST_FPRINTF_POSIX

/* Define to 1 when the gnulib module fputc should be tested. */
#undef GNULIB_TEST_FPUTC

/* Define to 1 when the gnulib module fputs should be tested. */
#undef GNULIB_TEST_FPUTS

/* Define to 1 when the gnulib module fread should be tested. */
#undef GNULIB_TEST_FREAD

/* Define to 1 when the gnulib module free-posix should be tested. */
#undef GNULIB_TEST_FREE_POSIX

/* Define to 1 when the gnulib module frexp should be tested. */
#undef GNULIB_TEST_FREXP

/* Define to 1 when the gnulib module frexpl should be tested. */
#undef GNULIB_TEST_FREXPL

/* Define to 1 when the gnulib module fscanf should be tested. */
#undef GNULIB_TEST_FSCANF

/* Define to 1 when the gnulib module fstat should be tested. */
#undef GNULIB_TEST_FSTAT

/* Define to 1 when the gnulib module fstatat should be tested. */
#undef GNULIB_TEST_FSTATAT

/* Define to 1 when the gnulib module fsync should be tested. */
#undef GNULIB_TEST_FSYNC

/* Define to 1 when the gnulib module fwrite should be tested. */
#undef GNULIB_TEST_FWRITE

/* Define to 1 when the gnulib module getc should be tested. */
#undef GNULIB_TEST_GETC

/* Define to 1 when the gnulib module getchar should be tested. */
#undef GNULIB_TEST_GETCHAR

/* Define to 1 when the gnulib module getcwd should be tested. */
#undef GNULIB_TEST_GETCWD

/* Define to 1 when the gnulib module getdelim should be tested. */
#undef GNULIB_TEST_GETDELIM

/* Define to 1 when the gnulib module getdtablesize should be tested. */
#undef GNULIB_TEST_GETDTABLESIZE

/* Define to 1 when the gnulib module getline should be tested. */
#undef GNULIB_TEST_GETLINE

/* Define to 1 when the gnulib module getopt-posix should be tested. */
#undef GNULIB_TEST_GETOPT_POSIX

/* Define to 1 when the gnulib module getrusage should be tested. */
#undef GNULIB_TEST_GETRUSAGE

/* Define to 1 when the gnulib module gettimeofday should be tested. */
#undef GNULIB_TEST_GETTIMEOFDAY

/* Define to 1 when the gnulib module isnan should be tested. */
#undef GNULIB_TEST_ISNAN

/* Define to 1 when the gnulib module isnand should be tested. */
#undef GNULIB_TEST_ISNAND

/* Define to 1 when the gnulib module isnanf should be tested. */
#undef GNULIB_TEST_ISNANF

/* Define to 1 when the gnulib module isnanl should be tested. */
#undef GNULIB_TEST_ISNANL

/* Define to 1 when the gnulib module iswblank should be tested. */
#undef GNULIB_TEST_ISWBLANK

/* Define to 1 when the gnulib module iswdigit should be tested. */
#undef GNULIB_TEST_ISWDIGIT

/* Define to 1 when the gnulib module iswxdigit should be tested. */
#undef GNULIB_TEST_ISWXDIGIT

/* Define to 1 when the gnulib module ldexpl should be tested. */
#undef GNULIB_TEST_LDEXPL

/* Define to 1 when the gnulib module lstat should be tested. */
#undef GNULIB_TEST_LSTAT

/* Define to 1 when the gnulib module malloc-posix should be tested. */
#undef GNULIB_TEST_MALLOC_POSIX

/* Define to 1 when the gnulib module mbrtowc should be tested. */
#undef GNULIB_TEST_MBRTOWC

/* Define to 1 when the gnulib module mbsinit should be tested. */
#undef GNULIB_TEST_MBSINIT

/* Define to 1 when the gnulib module memchr should be tested. */
#undef GNULIB_TEST_MEMCHR

/* Define to 1 when the gnulib module mempcpy should be tested. */
#undef GNULIB_TEST_MEMPCPY

/* Define to 1 when the gnulib module memrchr should be tested. */
#undef GNULIB_TEST_MEMRCHR

/* Define to 1 when the gnulib module obstack-printf should be tested. */
#undef GNULIB_TEST_OBSTACK_PRINTF

/* Define to 1 when the gnulib module open should be tested. */
#undef GNULIB_TEST_OPEN

/* Define to 1 when the gnulib module openat should be tested. */
#undef GNULIB_TEST_OPENAT

/* Define to 1 when the gnulib module opendir should be tested. */
#undef GNULIB_TEST_OPENDIR

/* Define to 1 when the gnulib module perror should be tested. */
#undef GNULIB_TEST_PERROR

/* Define to 1 when the gnulib module pipe should be tested. */
#undef GNULIB_TEST_PIPE

/* Define to 1 when the gnulib module pipe2 should be tested. */
#undef GNULIB_TEST_PIPE2

/* Define to 1 when the gnulib module posix_spawn should be tested. */
#undef GNULIB_TEST_POSIX_SPAWN

/* Define to 1 when the gnulib module posix_spawnattr_destroy should be
   tested. */
#undef GNULIB_TEST_POSIX_SPAWNATTR_DESTROY

/* Define to 1 when the gnulib module posix_spawnattr_init should be tested.
   */
#undef GNULIB_TEST_POSIX_SPAWNATTR_INIT

/* Define to 1 when the gnulib module posix_spawnattr_setflags should be
   tested. */
#undef GNULIB_TEST_POSIX_SPAWNATTR_SETFLAGS

/* Define to 1 when the gnulib module posix_spawnattr_setpgroup should be
   tested. */
#undef GNULIB_TEST_POSIX_SPAWNATTR_SETPGROUP

/* Define to 1 when the gnulib module posix_spawnattr_setsigmask should be
   tested. */
#undef GNULIB_TEST_POSIX_SPAWNATTR_SETSIGMASK

/* Define to 1 when the gnulib module posix_spawnp should be tested. */
#undef GNULIB_TEST_POSIX_SPAWNP

/* Define to 1 when the gnulib module posix_spawn_file_actions_addchdir should
   be tested. */
#undef GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR

/* Define to 1 when the gnulib module posix_spawn_file_actions_addclose should
   be tested. */
#undef GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_ADDCLOSE

/* Define to 1 when the gnulib module posix_spawn_file_actions_adddup2 should
   be tested. */
#undef GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_ADDDUP2

/* Define to 1 when the gnulib module posix_spawn_file_actions_addopen should
   be tested. */
#undef GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_ADDOPEN

/* Define to 1 when the gnulib module posix_spawn_file_actions_destroy should
   be tested. */
#undef GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_DESTROY

/* Define to 1 when the gnulib module posix_spawn_file_actions_init should be
   tested. */
#undef GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_INIT

/* Define to 1 when the gnulib module printf should be tested. */
#undef GNULIB_TEST_PRINTF

/* Define to 1 when the gnulib module printf-posix should be tested. */
#undef GNULIB_TEST_PRINTF_POSIX

/* Define to 1 when the gnulib module putc should be tested. */
#undef GNULIB_TEST_PUTC

/* Define to 1 when the gnulib module putchar should be tested. */
#undef GNULIB_TEST_PUTCHAR

/* Define to 1 when the gnulib module puts should be tested. */
#undef GNULIB_TEST_PUTS

/* Define to 1 when the gnulib module raise should be tested. */
#undef GNULIB_TEST_RAISE

/* Define to 1 when the gnulib module rawmemchr should be tested. */
#undef GNULIB_TEST_RAWMEMCHR

/* Define to 1 when the gnulib module readdir should be tested. */
#undef GNULIB_TEST_READDIR

/* Define to 1 when the gnulib module readlink should be tested. */
#undef GNULIB_TEST_READLINK

/* Define to 1 when the gnulib module reallocarray should be tested. */
#undef GNULIB_TEST_REALLOCARRAY

/* Define to 1 when the gnulib module realloc-posix should be tested. */
#undef GNULIB_TEST_REALLOC_POSIX

/* Define to 1 when the gnulib module realpath should be tested. */
#undef GNULIB_TEST_REALPATH

/* Define to 1 when the gnulib module rename should be tested. */
#undef GNULIB_TEST_RENAME

/* Define to 1 when the gnulib module rewinddir should be tested. */
#undef GNULIB_TEST_REWINDDIR

/* Define to 1 when the gnulib module rmdir should be tested. */
#undef GNULIB_TEST_RMDIR

/* Define to 1 when the gnulib module scanf should be tested. */
#undef GNULIB_TEST_SCANF

/* Define to 1 when the gnulib module setlocale_null should be tested. */
#undef GNULIB_TEST_SETLOCALE_NULL

/* Define to 1 when the gnulib module sigaction should be tested. */
#undef GNULIB_TEST_SIGACTION

/* Define to 1 when the gnulib module signbit should be tested. */
#undef GNULIB_TEST_SIGNBIT

/* Define to 1 when the gnulib module sigprocmask should be tested. */
#undef GNULIB_TEST_SIGPROCMASK

/* Define to 1 when the gnulib module snprintf should be tested. */
#undef GNULIB_TEST_SNPRINTF

/* Define to 1 when the gnulib module sprintf-posix should be tested. */
#undef GNULIB_TEST_SPRINTF_POSIX

/* Define to 1 when the gnulib module stat should be tested. */
#undef GNULIB_TEST_STAT

/* Define to 1 when the gnulib module stpcpy should be tested. */
#undef GNULIB_TEST_STPCPY

/* Define to 1 when the gnulib module stpncpy should be tested. */
#undef GNULIB_TEST_STPNCPY

/* Define to 1 when the gnulib module strchrnul should be tested. */
#undef GNULIB_TEST_STRCHRNUL

/* Define to 1 when the gnulib module strdup should be tested. */
#undef GNULIB_TEST_STRDUP

/* Define to 1 when the gnulib module strerror should be tested. */
#undef GNULIB_TEST_STRERROR

/* Define to 1 when the gnulib module strerror_r should be tested. */
#undef GNULIB_TEST_STRERROR_R

/* Define to 1 when the gnulib module strndup should be tested. */
#undef GNULIB_TEST_STRNDUP

/* Define to 1 when the gnulib module strnlen should be tested. */
#undef GNULIB_TEST_STRNLEN

/* Define to 1 when the gnulib module strtod should be tested. */
#undef GNULIB_TEST_STRTOD

/* Define to 1 when the gnulib module strverscmp should be tested. */
#undef GNULIB_TEST_STRVERSCMP

/* Define to 1 when the gnulib module unlink should be tested. */
#undef GNULIB_TEST_UNLINK

/* Define to 1 when the gnulib module unsetenv should be tested. */
#undef GNULIB_TEST_UNSETENV

/* Define to 1 when the gnulib module vasprintf should be tested. */
#undef GNULIB_TEST_VASPRINTF

/* Define to 1 when the gnulib module vfprintf should be tested. */
#undef GNULIB_TEST_VFPRINTF

/* Define to 1 when the gnulib module vfprintf-posix should be tested. */
#undef GNULIB_TEST_VFPRINTF_POSIX

/* Define to 1 when the gnulib module vprintf should be tested. */
#undef GNULIB_TEST_VPRINTF

/* Define to 1 when the gnulib module vsnprintf should be tested. */
#undef GNULIB_TEST_VSNPRINTF

/* Define to 1 when the gnulib module vsprintf-posix should be tested. */
#undef GNULIB_TEST_VSPRINTF_POSIX

/* Define to 1 when the gnulib module waitpid should be tested. */
#undef GNULIB_TEST_WAITPID

/* Define to 1 when the gnulib module wcwidth should be tested. */
#undef GNULIB_TEST_WCWIDTH

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module unistr/u8-mbtoucr shall be considered present. */
#undef GNULIB_UNISTR_U8_MBTOUCR

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module unistr/u8-uctomb shall be considered present. */
#undef GNULIB_UNISTR_U8_UCTOMB

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module vfprintf-posix shall be considered present. */
#undef GNULIB_VFPRINTF_POSIX

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module xalloc shall be considered present. */
#undef GNULIB_XALLOC

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module xalloc-die shall be considered present. */
#undef GNULIB_XALLOC_DIE

/* Define if your system defines TIOCGWINSZ in sys/ioctl.h. */
#undef GWINSZ_IN_SYS_IOCTL

/* Define to 1 if you have 'alloca' after including <alloca.h>, a header that
   may be supplied by this distribution. */
#undef HAVE_ALLOCA

/* Define to 1 if <alloca.h> works. */
#undef HAVE_ALLOCA_H

/* Define if you have an arithmetic hrtime_t type. */
#undef HAVE_ARITHMETIC_HRTIME_T

/* Define to 1 if you have the <bp-sym.h> header file. */
#undef HAVE_BP_SYM_H

/* Define to 1 if you have the `canonicalize_file_name' function. */
#undef HAVE_CANONICALIZE_FILE_NAME

/* Define to 1 if you have the `catgets' function. */
#undef HAVE_CATGETS

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
#undef HAVE_CFLOCALECOPYCURRENT

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
#undef HAVE_CFPREFERENCESCOPYAPPVALUE

/* Define to 1 if you have the `clock_gettime' function. */
#undef HAVE_CLOCK_GETTIME

/* Define to 1 if you have the `clock_settime' function. */
#undef HAVE_CLOCK_SETTIME

/* Define to 1 if you have the `closedir' function. */
#undef HAVE_CLOSEDIR

/* Define to 1 if you have the `confstr' function. */
#undef HAVE_CONFSTR

/* Define if the copysignf function is declared in <math.h> and available in
   libc. */
#undef HAVE_COPYSIGNF_IN_LIBC

/* Define if the copysignl function is declared in <math.h> and available in
   libc. */
#undef HAVE_COPYSIGNL_IN_LIBC

/* Define if the copysign function is declared in <math.h> and available in
   libc. */
#undef HAVE_COPYSIGN_IN_LIBC

/* Define to 1 if you have the `copy_file_range' function. */
#undef HAVE_COPY_FILE_RANGE

/* Define to 1 if you have the <crtdefs.h> header file. */
#undef HAVE_CRTDEFS_H

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
#undef HAVE_DCGETTEXT

/* Define to 1 if you have the declaration of `alarm', and to 0 if you don't.
   */
#undef HAVE_DECL_ALARM

/* Define to 1 if you have the declaration of `clearerr_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_CLEARERR_UNLOCKED

/* Define to 1 if you have the declaration of `copysign', and to 0 if you
   don't. */
#undef HAVE_DECL_COPYSIGN

/* Define to 1 if you have the declaration of `copysignf', and to 0 if you
   don't. */
#undef HAVE_DECL_COPYSIGNF

/* Define to 1 if you have the declaration of `copysignl', and to 0 if you
   don't. */
#undef HAVE_DECL_COPYSIGNL

/* Define to 1 if you have the declaration of `dirfd', and to 0 if you don't.
   */
#undef HAVE_DECL_DIRFD

/* Define to 1 if you have the declaration of `ecvt', and to 0 if you don't.
   */
#undef HAVE_DECL_ECVT

/* Define to 1 if you have the declaration of `execvpe', and to 0 if you
   don't. */
#undef HAVE_DECL_EXECVPE

/* Define to 1 if you have the declaration of `fchdir', and to 0 if you don't.
   */
#undef HAVE_DECL_FCHDIR

/* Define to 1 if you have the declaration of `fcloseall', and to 0 if you
   don't. */
#undef HAVE_DECL_FCLOSEALL

/* Define to 1 if you have the declaration of `fcvt', and to 0 if you don't.
   */
#undef HAVE_DECL_FCVT

/* Define to 1 if you have the declaration of `fdopendir', and to 0 if you
   don't. */
#undef HAVE_DECL_FDOPENDIR

/* Define to 1 if you have the declaration of `feof_unlocked', and to 0 if you
   don't. */
#undef HAVE_DECL_FEOF_UNLOCKED

/* Define to 1 if you have the declaration of `ferror_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_FERROR_UNLOCKED

/* Define to 1 if you have the declaration of `fflush_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_FFLUSH_UNLOCKED

/* Define to 1 if you have the declaration of `fgets_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_FGETS_UNLOCKED

/* Define to 1 if you have the declaration of `fputc_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_FPUTC_UNLOCKED

/* Define to 1 if you have the declaration of `fputs_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_FPUTS_UNLOCKED

/* Define to 1 if you have the declaration of `fread_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_FREAD_UNLOCKED

/* Define to 1 if you have the declaration of `fwrite_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_FWRITE_UNLOCKED

/* Define to 1 if you have the declaration of `gcvt', and to 0 if you don't.
   */
#undef HAVE_DECL_GCVT

/* Define to 1 if you have the declaration of `getchar_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_GETCHAR_UNLOCKED

/* Define to 1 if you have the declaration of `getcwd', and to 0 if you don't.
   */
#undef HAVE_DECL_GETCWD

/* Define to 1 if you have the declaration of `getc_unlocked', and to 0 if you
   don't. */
#undef HAVE_DECL_GETC_UNLOCKED

/* Define to 1 if you have the declaration of `getdelim', and to 0 if you
   don't. */
#undef HAVE_DECL_GETDELIM

/* Define to 1 if you have the declaration of `getdtablesize', and to 0 if you
   don't. */
#undef HAVE_DECL_GETDTABLESIZE

/* Define to 1 if you have the declaration of `gethrtime', and to 0 if you
   don't. */
#undef HAVE_DECL_GETHRTIME

/* Define to 1 if you have the declaration of `getline', and to 0 if you
   don't. */
#undef HAVE_DECL_GETLINE

/* Define to 1 if you have the declaration of `iswblank', and to 0 if you
   don't. */
#undef HAVE_DECL_ISWBLANK

/* Define to 1 if you have the declaration of `mbrtowc', and to 0 if you
   don't. */
#undef HAVE_DECL_MBRTOWC

/* Define to 1 if you have the declaration of `mbsinit', and to 0 if you
   don't. */
#undef HAVE_DECL_MBSINIT

/* Define to 1 if you have a declaration of mbswidth() in <wchar.h>, and to 0
   otherwise. */
#undef HAVE_DECL_MBSWIDTH_IN_WCHAR_H

/* Define to 1 if you have the declaration of `memrchr', and to 0 if you
   don't. */
#undef HAVE_DECL_MEMRCHR

/* Define to 1 if you have the declaration of `obstack_printf', and to 0 if
   you don't. */
#undef HAVE_DECL_OBSTACK_PRINTF

/* Define to 1 if you have the declaration of `posix_spawn', and to 0 if you
   don't. */
#undef HAVE_DECL_POSIX_SPAWN

/* Define to 1 if you have the declaration of `program_invocation_name', and
   to 0 if you don't. */
#undef HAVE_DECL_PROGRAM_INVOCATION_NAME

/* Define to 1 if you have the declaration of `program_invocation_short_name',
   and to 0 if you don't. */
#undef HAVE_DECL_PROGRAM_INVOCATION_SHORT_NAME

/* Define to 1 if you have the declaration of `putchar_unlocked', and to 0 if
   you don't. */
#undef HAVE_DECL_PUTCHAR_UNLOCKED

/* Define to 1 if you have the declaration of `putc_unlocked', and to 0 if you
   don't. */
#undef HAVE_DECL_PUTC_UNLOCKED

/* Define to 1 if you have the declaration of `setenv', and to 0 if you don't.
   */
#undef HAVE_DECL_SETENV

/* Define to 1 if you have the declaration of `snprintf', and to 0 if you
   don't. */
#undef HAVE_DECL_SNPRINTF

/* Define to 1 if you have the declaration of `stpncpy', and to 0 if you
   don't. */
#undef HAVE_DECL_STPNCPY

/* Define to 1 if you have the declaration of `strdup', and to 0 if you don't.
   */
#undef HAVE_DECL_STRDUP

/* Define to 1 if you have the declaration of `strerror_r', and to 0 if you
   don't. */
#undef HAVE_DECL_STRERROR_R

/* Define to 1 if you have the declaration of `strndup', and to 0 if you
   don't. */
#undef HAVE_DECL_STRNDUP

/* Define to 1 if you have the declaration of `strnlen', and to 0 if you
   don't. */
#undef HAVE_DECL_STRNLEN

/* Define to 1 if you have the declaration of `towlower', and to 0 if you
   don't. */
#undef HAVE_DECL_TOWLOWER

/* Define to 1 if you have the declaration of `unsetenv', and to 0 if you
   don't. */
#undef HAVE_DECL_UNSETENV

/* Define to 1 if you have the declaration of `vsnprintf', and to 0 if you
   don't. */
#undef HAVE_DECL_VSNPRINTF

/* Define to 1 if you have the declaration of `wcsdup', and to 0 if you don't.
   */
#undef HAVE_DECL_WCSDUP

/* Define to 1 if you have the declaration of `wcwidth', and to 0 if you
   don't. */
#undef HAVE_DECL_WCWIDTH

/* Define to 1 if you have the declaration of `_snprintf', and to 0 if you
   don't. */
#undef HAVE_DECL__SNPRINTF

/* Define to 1 if you have the declaration of `__argv', and to 0 if you don't.
   */
#undef HAVE_DECL___ARGV

/* Define to 1 if you have the declaration of `__fpending', and to 0 if you
   don't. */
#undef HAVE_DECL___FPENDING

/* Define to 1 if you have the <dirent.h> header file. */
#undef HAVE_DIRENT_H

/* Define to 1 if you have the `dirfd' function. */
#undef HAVE_DIRFD

/* Define if you have the declaration of environ. */
#undef HAVE_ENVIRON_DECL

/* Define to 1 if you have the `faccessat' function. */
#undef HAVE_FACCESSAT

/* Define to 1 if you have the `fchdir' function. */
#undef HAVE_FCHDIR

/* Define to 1 if you have the `fcntl' function. */
#undef HAVE_FCNTL

/* Define to 1 if you have the `fdopendir' function. */
#undef HAVE_FDOPENDIR

/* Define to 1 if you have the <features.h> header file. */
#undef HAVE_FEATURES_H

/* Define to 1 if you have the `ffsl' function. */
#undef HAVE_FFSL

/* Define to 1 if you have the `flockfile' function. */
#undef HAVE_FLOCKFILE

/* Define if the 'free' function is guaranteed to preserve errno. */
#undef HAVE_FREE_POSIX

/* Define if the frexpl function is available in libc. */
#undef HAVE_FREXPL_IN_LIBC

/* Define if the frexp function is available in libc. */
#undef HAVE_FREXP_IN_LIBC

/* Define to 1 if you have the `fstatat' function. */
#undef HAVE_FSTATAT

/* Define to 1 if you have the `fsync' function. */
#undef HAVE_FSYNC

/* Define to 1 if you have the `funlockfile' function. */
#undef HAVE_FUNLOCKFILE

/* Define to 1 if you have the `getcwd' function. */
#undef HAVE_GETCWD

/* Define to 1 if getcwd works, but with shorter paths than is generally
   tested with the replacement. */
#undef HAVE_GETCWD_SHORTER

/* Define to 1 if you have the `getdelim' function. */
#undef HAVE_GETDELIM

/* Define to 1 if you have the `getdtablesize' function. */
#undef HAVE_GETDTABLESIZE

/* Define to 1 if you have the `getexecname' function. */
#undef HAVE_GETEXECNAME

/* Define to 1 if you have the <getopt.h> header file. */
#undef HAVE_GETOPT_H

/* Define to 1 if you have the `getopt_long_only' function. */
#undef HAVE_GETOPT_LONG_ONLY

/* Define to 1 if the system has the 'getpagesize' function. */
#undef HAVE_GETPAGESIZE

/* Define to 1 if you have the `getprogname' function. */
#undef HAVE_GETPROGNAME

/* Define to 1 if you have the `getrusage' function. */
#undef HAVE_GETRUSAGE

/* Define if the GNU gettext() function is already present or preinstalled. */
#undef HAVE_GETTEXT

/* Define to 1 if you have the `gettimeofday' function. */
#undef HAVE_GETTIMEOFDAY

/* Define if you have the iconv() function and it works. */
#undef HAVE_ICONV

/* Define to 1 if you have the <iconv.h> header file. */
#undef HAVE_ICONV_H

/* Define to 1 if the compiler supports one of the keywords 'inline',
   '__inline__', '__inline' and effectively inlines functions marked as such.
   */
#undef HAVE_INLINE

/* Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>. */
#undef HAVE_INTMAX_T

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define if <inttypes.h> exists, doesn't clash with <sys/types.h>, and
   declares uintmax_t. */
#undef HAVE_INTTYPES_H_WITH_UINTMAX

/* Define to 1 if you have the `isascii' function. */
#undef HAVE_ISASCII

/* Define if the isnan(double) function is available in libc. */
#undef HAVE_ISNAND_IN_LIBC

/* Define if the isnan(float) function is available in libc. */
#undef HAVE_ISNANF_IN_LIBC

/* Define if the isnan(long double) function is available in libc. */
#undef HAVE_ISNANL_IN_LIBC

/* Define to 1 if you have the `iswblank' function. */
#undef HAVE_ISWBLANK

/* Define to 1 if you have the `iswcntrl' function. */
#undef HAVE_ISWCNTRL

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#undef HAVE_LANGINFO_CODESET

/* Define if the ldexpl() function is available. */
#undef HAVE_LDEXPL

/* Define if the ldexpl function is available in libc. */
#undef HAVE_LDEXPL_IN_LIBC

/* Define if the ldexp function is available in libc. */
#undef HAVE_LDEXP_IN_LIBC

/* Define if you have the libtextstyle library. */
#undef HAVE_LIBTEXTSTYLE

/* Define to 1 if you have the <limits.h> header file. */
#undef HAVE_LIMITS_H

/* Define to 1 if you have the `link' function. */
#undef HAVE_LINK

/* Define to 1 if you have the <locale.h> header file. */
#undef HAVE_LOCALE_H

/* Define to 1 if the system has the type 'long long int'. */
#undef HAVE_LONG_LONG_INT

/* Define to 1 if you have the `lstat' function. */
#undef HAVE_LSTAT

/* Define to 1 if you have the <mach-o/dyld.h> header file. */
#undef HAVE_MACH_O_DYLD_H

/* Define if malloc, realloc, and calloc set errno on allocation failure. */
#undef HAVE_MALLOC_POSIX

/* Define to 1 if mmap()'s MAP_ANONYMOUS flag is available after including
   config.h and <sys/mman.h>. */
#undef HAVE_MAP_ANONYMOUS

/* Define to 1 if you have the <math.h> header file. */
#undef HAVE_MATH_H

/* Define to 1 if you have the `mbrtowc' function. */
#undef HAVE_MBRTOWC

/* Define to 1 if you have the `mbsinit' function. */
#undef HAVE_MBSINIT

/* Define to 1 if <wchar.h> declares mbstate_t. */
#undef HAVE_MBSTATE_T

/* Define to 1 if you have the `mempcpy' function. */
#undef HAVE_MEMPCPY

/* Define to 1 if you have the `memrchr' function. */
#undef HAVE_MEMRCHR

/* Define to 1 if you have the `microuptime' function. */
#undef HAVE_MICROUPTIME

/* Define to 1 if getcwd minimally works, that is, its result can be trusted
   when it succeeds. */
#undef HAVE_MINIMALLY_WORKING_GETCWD

/* Define to 1 if you have the <minix/config.h> header file. */
#undef HAVE_MINIX_CONFIG_H

/* Define to 1 if <limits.h> defines the MIN and MAX macros. */
#undef HAVE_MINMAX_IN_LIMITS_H

/* Define to 1 if <sys/param.h> defines the MIN and MAX macros. */
#undef HAVE_MINMAX_IN_SYS_PARAM_H

/* Define to 1 if you have the `mprotect' function. */
#undef HAVE_MPROTECT

/* Define to 1 on MSVC platforms that have the "invalid parameter handler"
   concept. */
#undef HAVE_MSVC_INVALID_PARAMETER_HANDLER

/* Define to 1 if you have the `nanouptime' function. */
#undef HAVE_NANOUPTIME

/* Define to 1 if you have the `nl_langinfo' function. */
#undef HAVE_NL_LANGINFO

/* Define to 1 if the system has obstacks that work with any size object. */
#undef HAVE_OBSTACK

/* Define to 1 if you have the `obstack_printf' function. */
#undef HAVE_OBSTACK_PRINTF

/* Define to 1 if you have the `openat' function. */
#undef HAVE_OPENAT

/* Define to 1 if you have the `opendir' function. */
#undef HAVE_OPENDIR

/* Define to 1 if getcwd works, except it sometimes fails when it shouldn't,
   setting errno to ERANGE, ENAMETOOLONG, or ENOENT. */
#undef HAVE_PARTLY_WORKING_GETCWD

/* Define to 1 if you have the <paths.h> header file. */
#undef HAVE_PATHS_H

/* Define to 1 if you have the `pipe' function. */
#undef HAVE_PIPE

/* Define to 1 if you have the `pipe2' function. */
#undef HAVE_PIPE2

/* Define to 1 if you have the `posix_spawn' function. */
#undef HAVE_POSIX_SPAWN

/* Define to 1 if the system has the type `posix_spawnattr_t'. */
#undef HAVE_POSIX_SPAWNATTR_T

/* Define to 1 if you have the `posix_spawn_file_actions_addchdir' function.
   */
#undef HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR

/* Define to 1 if you have the `posix_spawn_file_actions_addchdir_np'
   function. */
#undef HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR_NP

/* Define to 1 if the system has the type `posix_spawn_file_actions_t'. */
#undef HAVE_POSIX_SPAWN_FILE_ACTIONS_T

/* Define if you have the <pthread.h> header and the POSIX threads API. */
#undef HAVE_PTHREAD_API

/* Define if the <pthread.h> defines PTHREAD_MUTEX_RECURSIVE. */
#undef HAVE_PTHREAD_MUTEX_RECURSIVE

/* Define if the POSIX multithreading library has read/write locks. */
#undef HAVE_PTHREAD_RWLOCK

/* Define if the 'pthread_rwlock_rdlock' function prefers a writer to a
   reader. */
#undef HAVE_PTHREAD_RWLOCK_RDLOCK_PREFER_WRITER

/* Define to 1 if you have the `raise' function. */
#undef HAVE_RAISE

/* Define to 1 if you have the `rawmemchr' function. */
#undef HAVE_RAWMEMCHR

/* Define to 1 if you have the `readdir' function. */
#undef HAVE_READDIR

/* Define if you have the readline library. */
#undef HAVE_READLINE

/* Define to 1 if you have the <readline/history.h> header file. */
#undef HAVE_READLINE_HISTORY_H

/* Define to 1 if you have the <readline/readline.h> header file. */
#undef HAVE_READLINE_READLINE_H

/* Define to 1 if you have the `readlink' function. */
#undef HAVE_READLINK

/* Define to 1 if you have the `readlinkat' function. */
#undef HAVE_READLINKAT

/* Define to 1 if you have the `reallocarray' function. */
#undef HAVE_REALLOCARRAY

/* Define to 1 if you have the `realpath' function. */
#undef HAVE_REALPATH

/* Define to 1 if you have the `rewinddir' function. */
#undef HAVE_REWINDDIR

/* Define to 1 if 'long double' and 'double' have the same representation. */
#undef HAVE_SAME_LONG_DOUBLE_AS_DOUBLE

/* Define to 1 if you have the <sched.h> header file. */
#undef HAVE_SCHED_H

/* Define to 1 if you have the `sched_setparam' function. */
#undef HAVE_SCHED_SETPARAM

/* Define to 1 if you have the `sched_setscheduler' function. */
#undef HAVE_SCHED_SETSCHEDULER

/* Define to 1 if you have the <sdkddkver.h> header file. */
#undef HAVE_SDKDDKVER_H

/* Define to 1 if you have the <search.h> header file. */
#undef HAVE_SEARCH_H

/* Define to 1 if you have the `setdtablesize' function. */
#undef HAVE_SETDTABLESIZE

/* Define to 1 if you have the `setegid' function. */
#undef HAVE_SETEGID

/* Define to 1 if you have the `setenv' function. */
#undef HAVE_SETENV

/* Define to 1 if you have the `seteuid' function. */
#undef HAVE_SETEUID

/* Define to 1 if you have the `setlocale' function. */
#undef HAVE_SETLOCALE

/* Define to 1 if you have the `sigaction' function. */
#undef HAVE_SIGACTION

/* Define to 1 if you have the `sigaltstack' function. */
#undef HAVE_SIGALTSTACK

/* Define to 1 if the system has the type `siginfo_t'. */
#undef HAVE_SIGINFO_T

/* Define to 1 if you have the `siginterrupt' function. */
#undef HAVE_SIGINTERRUPT

/* Define to 1 if 'sig_atomic_t' is a signed integer type. */
#undef HAVE_SIGNED_SIG_ATOMIC_T

/* Define to 1 if 'wchar_t' is a signed integer type. */
#undef HAVE_SIGNED_WCHAR_T

/* Define to 1 if 'wint_t' is a signed integer type. */
#undef HAVE_SIGNED_WINT_T

/* Define to 1 if the system has the type `sigset_t'. */
#undef HAVE_SIGSET_T

/* Define to 1 if the system has the type `sig_atomic_t'. */
#undef HAVE_SIG_ATOMIC_T

/* Define to 1 if you have the `snprintf' function. */
#undef HAVE_SNPRINTF

/* Define if the return value of the snprintf function is the number of of
   bytes (excluding the terminating NUL) that would have been produced if the
   buffer had been large enough. */
#undef HAVE_SNPRINTF_RETVAL_C99

/* Define if the string produced by the snprintf function is always NUL
   terminated. */
#undef HAVE_SNPRINTF_TRUNCATION_C99

/* Define to 1 if you have the <spawn.h> header file. */
#undef HAVE_SPAWN_H

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define if <stdint.h> exists, doesn't clash with <sys/types.h>, and declares
   uintmax_t. */
#undef HAVE_STDINT_H_WITH_UINTMAX

/* Define to 1 if you have the <stdio_ext.h> header file. */
#undef HAVE_STDIO_EXT_H

/* Define to 1 if you have the <stdio.h> header file. */
#undef HAVE_STDIO_H

/* Define to 1 if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define to 1 if you have the `stpcpy' function. */
#undef HAVE_STPCPY

/* Define if you have the stpncpy() function and it works. */
#undef HAVE_STPNCPY

/* Define to 1 if you have the `strchrnul' function. */
#undef HAVE_STRCHRNUL

/* Define to 1 if you have the `strerror_r' function. */
#undef HAVE_STRERROR_R

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define to 1 if you have the `strndup' function. */
#undef HAVE_STRNDUP

/* Define to 1 if you have the `strnlen' function. */
#undef HAVE_STRNLEN

/* Define to 1 if `sa_sigaction' is a member of `struct sigaction'. */
#undef HAVE_STRUCT_SIGACTION_SA_SIGACTION

/* Define to 1 if `st_atimensec' is a member of `struct stat'. */
#undef HAVE_STRUCT_STAT_ST_ATIMENSEC

/* Define to 1 if `st_atimespec.tv_nsec' is a member of `struct stat'. */
#undef HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC

/* Define to 1 if `st_atim.st__tim.tv_nsec' is a member of `struct stat'. */
#undef HAVE_STRUCT_STAT_ST_ATIM_ST__TIM_TV_NSEC

/* Define to 1 if `st_atim.tv_nsec' is a member of `struct stat'. */
#undef HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC

/* Define to 1 if `st_birthtimensec' is a member of `struct stat'. */
#undef HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC

/* Define to 1 if `st_birthtimespec.tv_nsec' is a member of `struct stat'. */
#undef HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC

/* Define to 1 if `st_birthtim.tv_nsec' is a member of `struct stat'. */
#undef HAVE_STRUCT_STAT_ST_BIRTHTIM_TV_NSEC

/* Define to 1 if the system has the type `struct tms'. */
#undef HAVE_STRUCT_TMS

/* Define to 1 if you have the `strverscmp' function. */
#undef HAVE_STRVERSCMP

/* Define to 1 if you have the `symlink' function. */
#undef HAVE_SYMLINK

/* Define to 1 if you have the <sys/bitypes.h> header file. */
#undef HAVE_SYS_BITYPES_H

/* Define to 1 if you have the <sys/cdefs.h> header file. */
#undef HAVE_SYS_CDEFS_H

/* Define to 1 if you have the <sys/inttypes.h> header file. */
#undef HAVE_SYS_INTTYPES_H

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#undef HAVE_SYS_IOCTL_H

/* Define to 1 if you have the <sys/mman.h> header file. */
#undef HAVE_SYS_MMAN_H

/* Define to 1 if you have the <sys/param.h> header file. */
#undef HAVE_SYS_PARAM_H

/* Define to 1 if you have the <sys/resource.h> header file. */
#undef HAVE_SYS_RESOURCE_H

/* Define to 1 if you have the <sys/single_threaded.h> header file. */
#undef HAVE_SYS_SINGLE_THREADED_H

/* Define to 1 if you have the <sys/socket.h> header file. */
#undef HAVE_SYS_SOCKET_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/times.h> header file. */
#undef HAVE_SYS_TIMES_H

/* Define to 1 if you have the <sys/time.h> header file. */
#undef HAVE_SYS_TIME_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <sys/wait.h> header file. */
#undef HAVE_SYS_WAIT_H

/* Define to 1 if you have the `tcdrain' function. */
#undef HAVE_TCDRAIN

/* Define to 1 if you have the <termios.h> header file. */
#undef HAVE_TERMIOS_H

/* Define to 1 if you have the `thrd_create' function. */
#undef HAVE_THRD_CREATE

/* Define to 1 if you have the <threads.h> header file. */
#undef HAVE_THREADS_H

/* Define to 1 if you have the `towlower' function. */
#undef HAVE_TOWLOWER

/* Define to 1 if you have the `tsearch' function. */
#undef HAVE_TSEARCH

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to 1 if you have the `unsetenv' function. */
#undef HAVE_UNSETENV

/* Define to 1 if the system has the type 'unsigned long long int'. */
#undef HAVE_UNSIGNED_LONG_LONG_INT

/* Define if you have a global __progname variable */
#undef HAVE_VAR___PROGNAME

/* Define to 1 if you have the `vasnprintf' function. */
#undef HAVE_VASNPRINTF

/* Define to 1 if you have the `vasprintf' function. */
#undef HAVE_VASPRINTF

/* Define to 1 if you have the `vfork' function. */
#undef HAVE_VFORK

/* Define to 1 or 0, depending whether the compiler supports simple visibility
   declarations. */
#undef HAVE_VISIBILITY

/* Define to 1 if you have the `vsnprintf' function. */
#undef HAVE_VSNPRINTF

/* Define to 1 if you have the `waitid' function. */
#undef HAVE_WAITID

/* Define to 1 if you have the <wchar.h> header file. */
#undef HAVE_WCHAR_H

/* Define if you have the 'wchar_t' type. */
#undef HAVE_WCHAR_T

/* Define to 1 if you have the `wcrtomb' function. */
#undef HAVE_WCRTOMB

/* Define to 1 if you have the `wcslen' function. */
#undef HAVE_WCSLEN

/* Define to 1 if you have the `wcsnlen' function. */
#undef HAVE_WCSNLEN

/* Define to 1 if you have the <wctype.h> header file. */
#undef HAVE_WCTYPE_H

/* Define to 1 if you have the `wcwidth' function. */
#undef HAVE_WCWIDTH

/* Define to 1 if the compiler and linker support weak declarations of
   symbols. */
#undef HAVE_WEAK_SYMBOLS

/* Define to 1 if you have the <winsock2.h> header file. */
#undef HAVE_WINSOCK2_H

/* Define if you have the 'wint_t' type. */
#undef HAVE_WINT_T

/* Define to 1 if fstatat (..., 0) works. For example, it does not work in AIX
   7.1. */
#undef HAVE_WORKING_FSTATAT_ZERO_FLAG

/* Define to 1 if O_NOATIME works. */
#undef HAVE_WORKING_O_NOATIME

/* Define to 1 if O_NOFOLLOW works. */
#undef HAVE_WORKING_O_NOFOLLOW

/* Define to 1 if you have the <xlocale.h> header file. */
#undef HAVE_XLOCALE_H

/* Define to 1 if the system has the type `_Bool'. */
#undef HAVE__BOOL

/* Define to 1 if you have the `_NSGetExecutablePath' function. */
#undef HAVE__NSGETEXECUTABLEPATH

/* Define to 1 if you have the `_set_invalid_parameter_handler' function. */
#undef HAVE__SET_INVALID_PARAMETER_HANDLER

/* Define to 1 if the compiler supports __builtin_expect,
   and to 2 if <builtins.h> does.  */
#undef HAVE___BUILTIN_EXPECT
#ifndef HAVE___BUILTIN_EXPECT
# define __builtin_expect(e, c) (e)
#elif HAVE___BUILTIN_EXPECT == 2
# include <builtins.h>
#endif
    

/* Define to 1 if you have the `__fseterr' function. */
#undef HAVE___FSETERR

/* Define to 1 if the compiler supports the keyword '__inline'. */
#undef HAVE___INLINE

/* Define to 1 if you have the `__xpg_strerror_r' function. */
#undef HAVE___XPG_STRERROR_R

/* Define as const if the declaration of iconv() needs const. */
#undef ICONV_CONST

/* Define to a symbolic name denoting the flavor of iconv_open()
   implementation. */
#undef ICONV_FLAVOR

/* Define to the value of ${prefix}, as a string. */
#undef INSTALLPREFIX

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'long double'. */
#undef LDBL_EXPBIT0_BIT

/* Define as the word index where to find the exponent of 'long double'. */
#undef LDBL_EXPBIT0_WORD

/* Define as the bit index in the word where to find the sign of 'long
   double'. */
#undef LDBL_SIGNBIT_BIT

/* Define as the word index where to find the sign of 'long double'. */
#undef LDBL_SIGNBIT_WORD

/* Define to 1 if 'lstat' dereferences a symlink specified with a trailing
   slash. */
#undef LSTAT_FOLLOWS_SLASHED_SYMLINK

/* Define to the GNU M4 executable name. */
#undef M4

/* Define to "-g" if GNU M4 supports -g, otherwise to "". */
#undef M4_GNU_OPTION

/* If malloc(0) is != NULL, define this to 1. Otherwise define this to 0. */
#undef MALLOC_0_IS_NONNULL

/* Define to a substitute value for mmap()'s MAP_ANONYMOUS flag. */
#undef MAP_ANONYMOUS

/* Define if the mbrtowc function does not return (size_t) -2 for empty input.
   */
#undef MBRTOWC_EMPTY_INPUT_BUG

/* Define if the mbrtowc function may signal encoding errors in the C locale.
   */
#undef MBRTOWC_IN_C_LOCALE_MAYBE_EILSEQ

/* Define if the mbrtowc function has the NULL pwc argument bug. */
#undef MBRTOWC_NULL_ARG1_BUG

/* Define if the mbrtowc function has the NULL string argument bug. */
#undef MBRTOWC_NULL_ARG2_BUG

/* Define if the mbrtowc function does not return 0 for a NUL character. */
#undef MBRTOWC_NUL_RETVAL_BUG

/* Define if the mbrtowc function returns a wrong return value. */
#undef MBRTOWC_RETVAL_BUG

/* Define if the mbrtowc function stores a wide character when reporting
   incomplete input. */
#undef MBRTOWC_STORES_INCOMPLETE_BUG

/* Use GNU style printf and scanf.  */
#ifndef __USE_MINGW_ANSI_STDIO
# undef __USE_MINGW_ANSI_STDIO
#endif


/* Define to 1 on musl libc. */
#undef MUSL_LIBC

/* Define to 1 if assertions should be disabled. */
#undef NDEBUG

/* Define if the vasnprintf implementation needs special code for the 'a' and
   'A' directives. */
#undef NEED_PRINTF_DIRECTIVE_A

/* Define if the vasnprintf implementation needs special code for the 'F'
   directive. */
#undef NEED_PRINTF_DIRECTIVE_F

/* Define if the vasnprintf implementation needs special code for the 'ls'
   directive. */
#undef NEED_PRINTF_DIRECTIVE_LS

/* Define if the vasnprintf implementation needs special code for 'double'
   arguments. */
#undef NEED_PRINTF_DOUBLE

/* Define if the vasnprintf implementation needs special code for surviving
   out-of-memory conditions. */
#undef NEED_PRINTF_ENOMEM

/* Define if the vasnprintf implementation needs special code for the ' flag.
   */
#undef NEED_PRINTF_FLAG_GROUPING

/* Define if the vasnprintf implementation needs special code for the '-'
   flag. */
#undef NEED_PRINTF_FLAG_LEFTADJUST

/* Define if the vasnprintf implementation needs special code for the 0 flag.
   */
#undef NEED_PRINTF_FLAG_ZERO

/* Define if the vasnprintf implementation needs special code for infinite
   'double' arguments. */
#undef NEED_PRINTF_INFINITE_DOUBLE

/* Define if the vasnprintf implementation needs special code for infinite
   'long double' arguments. */
#undef NEED_PRINTF_INFINITE_LONG_DOUBLE

/* Define if the vasnprintf implementation needs special code for 'long
   double' arguments. */
#undef NEED_PRINTF_LONG_DOUBLE

/* Define if the vasnprintf implementation needs special code for supporting
   large precisions without arbitrary bounds. */
#undef NEED_PRINTF_UNBOUNDED_PRECISION

/* Define to 1 if open() fails to recognize a trailing slash. */
#undef OPEN_TRAILING_SLASH_BUG

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* The copyright year for this package */
#undef PACKAGE_COPYRIGHT_YEAR

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* Define to the type that is the result of default argument promotions of
   type mode_t. */
#undef PROMOTED_MODE_T

/* Define if the pthread_in_use() detection is hard. */
#undef PTHREAD_IN_USE_DETECTION_HARD

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'ptrdiff_t'. */
#undef PTRDIFF_T_SUFFIX

/* Define to 1 if readlink fails to recognize a trailing slash. */
#undef READLINK_TRAILING_SLASH_BUG

/* Define to 1 if readlink sets errno instead of truncating a too-long link.
   */
#undef READLINK_TRUNCATE_BUG

/* Define if rename does not work when the destination file exists, as on
   Cygwin 1.5 or Windows. */
#undef RENAME_DEST_EXISTS_BUG

/* Define if rename fails to leave hard links alone, as on NetBSD 1.6 or
   Cygwin 1.5. */
#undef RENAME_HARD_LINK_BUG

/* Define if rename does not correctly handle slashes on the destination
   argument, such as on Solaris 11 or NetBSD 1.6. */
#undef RENAME_TRAILING_SLASH_DEST_BUG

/* Define if rename does not correctly handle slashes on the source argument,
   such as on Solaris 9 or cygwin 1.5. */
#undef RENAME_TRAILING_SLASH_SOURCE_BUG

/* Define to 1 if gnulib's dirfd() replacement is used. */
#undef REPLACE_DIRFD

/* Define to 1 if gnulib's fchdir() replacement is used. */
#undef REPLACE_FCHDIR

/* Define if fprintf is overridden by a POSIX compliant gnulib implementation.
   */
#undef REPLACE_FPRINTF_POSIX

/* Define to 1 if stat needs help when passed a file name with a trailing
   slash */
#undef REPLACE_FUNC_STAT_FILE

/* Define to 1 if open() should work around the inability to open a directory.
   */
#undef REPLACE_OPEN_DIRECTORY

/* Define if gnulib uses its own posix_spawn and posix_spawnp functions. */
#undef REPLACE_POSIX_SPAWN

/* Define if printf is overridden by a POSIX compliant gnulib implementation.
   */
#undef REPLACE_PRINTF_POSIX

/* Define to 1 if strerror(0) does not return a message implying success. */
#undef REPLACE_STRERROR_0

/* Define if vasnprintf exists but is overridden by gnulib. */
#undef REPLACE_VASNPRINTF

/* Define if vfprintf is overridden by a POSIX compliant gnulib
   implementation. */
#undef REPLACE_VFPRINTF_POSIX

/* Define to 1 if setlocale (LC_ALL, NULL) is multithread-safe. */
#undef SETLOCALE_NULL_ALL_MTSAFE

/* Define to 1 if setlocale (category, NULL) is multithread-safe. */
#undef SETLOCALE_NULL_ONE_MTSAFE

/* File name of the Bourne shell.  */
#if (defined _WIN32 && !defined __CYGWIN__) || defined __CYGWIN__ || defined __ANDROID__
/* Omit the directory part because
   - For native Windows programs in a Cygwin environment, the Cygwin mounts
     are not visible.
   - For 32-bit Cygwin programs in a 64-bit Cygwin environment, the Cygwin
     mounts are not visible.
   - On Android, /bin/sh does not exist. It's /system/bin/sh instead.  */
# define BOURNE_SHELL "sh"
#else
# define BOURNE_SHELL "/bin/sh"
#endif

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'sig_atomic_t'. */
#undef SIG_ATOMIC_T_SUFFIX

/* Define as the maximum value of type 'size_t', if the system doesn't define
   it. */
#ifndef SIZE_MAX
# undef SIZE_MAX
#endif

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'size_t'. */
#undef SIZE_T_SUFFIX

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
#undef STACK_DIRECTION

/* Define to 1 if the `S_IS*' macros in <sys/stat.h> do not work properly. */
#undef STAT_MACROS_BROKEN

/* Define to 1 if all of the C90 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#undef STDC_HEADERS

/* Define to 1 if strerror_r returns char *. */
#undef STRERROR_R_CHAR_P

/* Define to 1 if the type of the st_atim member of a struct stat is struct
   timespec. */
#undef TYPEOF_STRUCT_STAT_ST_ATIM_IS_STRUCT_TIMESPEC

/* Define to 1 if unlink() on a parent directory may succeed */
#undef UNLINK_PARENT_BUG

/* Define to the prefix of C symbols at the assembler and linker level, either
   an underscore or empty. */
#undef USER_LABEL_PREFIX

/* Define if the combination of the ISO C and POSIX multithreading APIs can be
   used. */
#undef USE_ISOC_AND_POSIX_THREADS

/* Define if the ISO C multithreading library can be used. */
#undef USE_ISOC_THREADS

/* Define if the POSIX multithreading library can be used. */
#undef USE_POSIX_THREADS

/* Define if references to the POSIX multithreading library are satisfied by
   libc. */
#undef USE_POSIX_THREADS_FROM_LIBC

/* Define if references to the POSIX multithreading library should be made
   weak. */
#undef USE_POSIX_THREADS_WEAK

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# undef _ALL_SOURCE
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# undef _DARWIN_C_SOURCE
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# undef __EXTENSIONS__
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# undef _GNU_SOURCE
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
# undef _HPUX_ALT_XOPEN_SOCKET_API
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
# undef _MINIX
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
# undef _NETBSD_SOURCE
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
# undef _OPENBSD_SOURCE
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
# undef _POSIX_SOURCE
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
# undef _POSIX_1_SOURCE
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# undef _POSIX_PTHREAD_SEMANTICS
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
# undef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
# undef __STDC_WANT_IEC_60559_BFP_EXT__
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
# undef __STDC_WANT_IEC_60559_DFP_EXT__
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
# undef __STDC_WANT_IEC_60559_FUNCS_EXT__
#endif
/* Enable extensions specified by ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
# undef __STDC_WANT_IEC_60559_TYPES_EXT__
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
# undef __STDC_WANT_LIB_EXT2__
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
# undef __STDC_WANT_MATH_SPEC_FUNCS__
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# undef _TANDEM_SOURCE
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
# undef _XOPEN_SOURCE
#endif


/* An alias of GNULIB_STDIO_SINGLE_THREAD. */
#undef USE_UNLOCKED_IO

/* Define if the native Windows multithreading API can be used. */
#undef USE_WINDOWS_THREADS

/* Version number of package */
#undef VERSION

/* Define to 1 if unsetenv returns void instead of int. */
#undef VOID_UNSETENV

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'wchar_t'. */
#undef WCHAR_T_SUFFIX

/* Define if sys/ptem.h is required for struct winsize. */
#undef WINSIZE_IN_PTEM

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'wint_t'. */
#undef WINT_T_SUFFIX

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#endif

/* Define to 1 to internationalize bison runtime messages. */
#undef YYENABLE_NLS

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#undef YYTEXT_POINTER

/* Number of bits in a file offset, on hosts where this is settable. */
#undef _FILE_OFFSET_BITS

/* True if the compiler says it groks GNU C version MAJOR.MINOR.  */
#if defined __GNUC__ && defined __GNUC_MINOR__
# define _GL_GNUC_PREREQ(major, minor) \
    ((major) < __GNUC__ + ((minor) <= __GNUC_MINOR__))
#else
# define _GL_GNUC_PREREQ(major, minor) 0
#endif


/* Define to enable the declarations of ISO C 11 types and functions. */
#undef _ISOC11_SOURCE

/* Define for large files, on AIX-style hosts. */
#undef _LARGE_FILES

/* Define to 1 on Solaris. */
#undef _LCONV_C99

/* The _Noreturn keyword of C11.  */
#ifndef _Noreturn
# if (defined __cplusplus \
      && ((201103 <= __cplusplus && !(__GNUC__ == 4 && __GNUC_MINOR__ == 7)) \
          || (defined _MSC_VER && 1900 <= _MSC_VER)) \
      && 0)
    /* [[noreturn]] is not practically usable, because with it the syntax
         extern _Noreturn void func (...);
       would not be valid; such a declaration would only be valid with 'extern'
       and '_Noreturn' swapped, or without the 'extern' keyword.  However, some
       AIX system header files and several gnulib header files use precisely
       this syntax with 'extern'.  */
#  define _Noreturn [[noreturn]]
# elif ((!defined __cplusplus || defined __clang__) \
        && (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) \
            || (!defined __STRICT_ANSI__ \
                && (_GL_GNUC_PREREQ (4, 7) \
                    || (defined __apple_build_version__ \
                        ? 6000000 <= __apple_build_version__ \
                        : 3 < __clang_major__ + (5 <= __clang_minor__))))))
   /* _Noreturn works as-is.  */
# elif _GL_GNUC_PREREQ (2, 8) || defined __clang__ || 0x5110 <= __SUNPRO_C
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
# endif
#endif


/* Number of bits in a timestamp, on hosts where this is settable. */
#undef _TIME_BITS

/* For standard stat data types on VMS. */
#undef _USE_STD_STAT

/* Define to rpl_ if the getopt replacement functions and variables should be
   used. */
#undef __GETOPT_PREFIX

/* For 64-bit time_t on 32-bit mingw. */
#undef __MINGW_USE_VC2005_COMPAT

/* Define to 1 if the system <stdint.h> predates C++11. */
#undef __STDC_CONSTANT_MACROS

/* Define to 1 if the system <stdint.h> predates C++11. */
#undef __STDC_LIMIT_MACROS

/* The _GL_ASYNC_SAFE marker should be attached to functions that are
   signal handlers (for signals other than SIGABRT, SIGPIPE) or can be
   invoked from such signal handlers.  Such functions have some restrictions:
     * All functions that it calls should be marked _GL_ASYNC_SAFE as well,
       or should be listed as async-signal-safe in POSIX
       <https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_04>
       section 2.4.3.  Note that malloc(), sprintf(), and fwrite(), in
       particular, are NOT async-signal-safe.
     * All memory locations (variables and struct fields) that these functions
       access must be marked 'volatile'.  This holds for both read and write
       accesses.  Otherwise the compiler might optimize away stores to and
       reads from such locations that occur in the program, depending on its
       data flow analysis.  For example, when the program contains a loop
       that is intended to inspect a variable set from within a signal handler
           while (!signal_occurred)
             ;
       the compiler is allowed to transform this into an endless loop if the
       variable 'signal_occurred' is not declared 'volatile'.
   Additionally, recall that:
     * A signal handler should not modify errno (except if it is a handler
       for a fatal signal and ends by raising the same signal again, thus
       provoking the termination of the process).  If it invokes a function
       that may clobber errno, it needs to save and restore the value of
       errno.  */
#define _GL_ASYNC_SAFE


/* Attributes.  */
#if (defined __has_attribute \
     && (!defined __clang_minor__ \
         || 3 < __clang_major__ + (5 <= __clang_minor__)))
# define _GL_HAS_ATTRIBUTE(attr) __has_attribute (__##attr##__)
#else
# define _GL_HAS_ATTRIBUTE(attr) _GL_ATTR_##attr
# define _GL_ATTR_alloc_size _GL_GNUC_PREREQ (4, 3)
# define _GL_ATTR_always_inline _GL_GNUC_PREREQ (3, 2)
# define _GL_ATTR_artificial _GL_GNUC_PREREQ (4, 3)
# define _GL_ATTR_cold _GL_GNUC_PREREQ (4, 3)
# define _GL_ATTR_const _GL_GNUC_PREREQ (2, 95)
# define _GL_ATTR_deprecated _GL_GNUC_PREREQ (3, 1)
# define _GL_ATTR_diagnose_if 0
# define _GL_ATTR_error _GL_GNUC_PREREQ (4, 3)
# define _GL_ATTR_externally_visible _GL_GNUC_PREREQ (4, 1)
# define _GL_ATTR_fallthrough _GL_GNUC_PREREQ (7, 0)
# define _GL_ATTR_format _GL_GNUC_PREREQ (2, 7)
# define _GL_ATTR_leaf _GL_GNUC_PREREQ (4, 6)
# define _GL_ATTR_malloc _GL_GNUC_PREREQ (3, 0)
# ifdef _ICC
#  define _GL_ATTR_may_alias 0
# else
#  define _GL_ATTR_may_alias _GL_GNUC_PREREQ (3, 3)
# endif
# define _GL_ATTR_noinline _GL_GNUC_PREREQ (3, 1)
# define _GL_ATTR_nonnull _GL_GNUC_PREREQ (3, 3)
# define _GL_ATTR_nonstring _GL_GNUC_PREREQ (8, 0)
# define _GL_ATTR_nothrow _GL_GNUC_PREREQ (3, 3)
# define _GL_ATTR_packed _GL_GNUC_PREREQ (2, 7)
# define _GL_ATTR_pure _GL_GNUC_PREREQ (2, 96)
# define _GL_ATTR_returns_nonnull _GL_GNUC_PREREQ (4, 9)
# define _GL_ATTR_sentinel _GL_GNUC_PREREQ (4, 0)
# define _GL_ATTR_unused _GL_GNUC_PREREQ (2, 7)
# define _GL_ATTR_warn_unused_result _GL_GNUC_PREREQ (3, 4)
#endif

#ifdef __has_c_attribute
# define _GL_HAS_C_ATTRIBUTE(attr) __has_c_attribute (__##attr##__)
#else
# define _GL_HAS_C_ATTRIBUTE(attr) 0
#endif


/* _GL_ATTRIBUTE_ALLOC_SIZE ((N)) declares that the Nth argument of the function
   is the size of the returned memory block.
   _GL_ATTRIBUTE_ALLOC_SIZE ((M, N)) declares that the Mth argument multiplied
   by the Nth argument of the function is the size of the returned memory block.
 */
/* Applies to: function, pointer to function, function types.  */
#if _GL_HAS_ATTRIBUTE (alloc_size)
# define _GL_ATTRIBUTE_ALLOC_SIZE(args) __attribute__ ((__alloc_size__ args))
#else
# define _GL_ATTRIBUTE_ALLOC_SIZE(args)
#endif

/* _GL_ATTRIBUTE_ALWAYS_INLINE tells that the compiler should always inline the
   function and report an error if it cannot do so.  */
/* Applies to: function.  */
#if _GL_HAS_ATTRIBUTE (always_inline)
# define _GL_ATTRIBUTE_ALWAYS_INLINE __attribute__ ((__always_inline__))
#else
# define _GL_ATTRIBUTE_ALWAYS_INLINE
#endif

/* _GL_ATTRIBUTE_ARTIFICIAL declares that the function is not important to show
    in stack traces when debugging.  The compiler should omit the function from
    stack traces.  */
/* Applies to: function.  */
#if _GL_HAS_ATTRIBUTE (artificial)
# define _GL_ATTRIBUTE_ARTIFICIAL __attribute__ ((__artificial__))
#else
# define _GL_ATTRIBUTE_ARTIFICIAL
#endif

/* _GL_ATTRIBUTE_COLD declares that the function is rarely executed.  */
/* Applies to: functions.  */
/* Avoid __attribute__ ((cold)) on MinGW; see thread starting at
   <https://lists.gnu.org/r/emacs-devel/2019-04/msg01152.html>.
   Also, Oracle Studio 12.6 requires 'cold' not '__cold__'.  */
#if _GL_HAS_ATTRIBUTE (cold) && !defined __MINGW32__
# ifndef __SUNPRO_C
#  define _GL_ATTRIBUTE_COLD __attribute__ ((__cold__))
# else
#  define _GL_ATTRIBUTE_COLD __attribute__ ((cold))
# endif
#else
# define _GL_ATTRIBUTE_COLD
#endif

/* _GL_ATTRIBUTE_CONST declares that it is OK for a compiler to omit duplicate
   calls to the function with the same arguments.
   This attribute is safe for a function that neither depends on nor affects
   observable state, and always returns exactly once - e.g., does not loop
   forever, and does not call longjmp.
   (This attribute is stricter than _GL_ATTRIBUTE_PURE.)  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (const)
# define _GL_ATTRIBUTE_CONST __attribute__ ((__const__))
#else
# define _GL_ATTRIBUTE_CONST
#endif

/* _GL_ATTRIBUTE_DEALLOC (F, I) declares that the function returns pointers
   that can be freed by passing them as the Ith argument to the
   function F.
   _GL_ATTRIBUTE_DEALLOC_FREE declares that the function returns pointers that
   can be freed via 'free'; it can be used only after declaring 'free'.  */
/* Applies to: functions.  Cannot be used on inline functions.  */
#if _GL_GNUC_PREREQ (11, 0)
# define _GL_ATTRIBUTE_DEALLOC(f, i) __attribute__ ((__malloc__ (f, i)))
#else
# define _GL_ATTRIBUTE_DEALLOC(f, i)
#endif
#define _GL_ATTRIBUTE_DEALLOC_FREE _GL_ATTRIBUTE_DEALLOC (free, 1)

/* _GL_ATTRIBUTE_DEPRECATED: Declares that an entity is deprecated.
   The compiler may warn if the entity is used.  */
/* Applies to:
     - function, variable,
     - struct, union, struct/union member,
     - enumeration, enumeration item,
     - typedef,
   in C++ also: namespace, class, template specialization.  */
#if _GL_HAS_C_ATTRIBUTE (deprecated)
# define _GL_ATTRIBUTE_DEPRECATED [[__deprecated__]]
#elif _GL_HAS_ATTRIBUTE (deprecated)
# define _GL_ATTRIBUTE_DEPRECATED __attribute__ ((__deprecated__))
#else
# define _GL_ATTRIBUTE_DEPRECATED
#endif

/* _GL_ATTRIBUTE_ERROR(msg) requests an error if a function is called and
   the function call is not optimized away.
   _GL_ATTRIBUTE_WARNING(msg) requests a warning if a function is called and
   the function call is not optimized away.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (error)
# define _GL_ATTRIBUTE_ERROR(msg) __attribute__ ((__error__ (msg)))
# define _GL_ATTRIBUTE_WARNING(msg) __attribute__ ((__warning__ (msg)))
#elif _GL_HAS_ATTRIBUTE (diagnose_if)
# define _GL_ATTRIBUTE_ERROR(msg) __attribute__ ((__diagnose_if__ (1, msg, "error")))
# define _GL_ATTRIBUTE_WARNING(msg) __attribute__ ((__diagnose_if__ (1, msg, "warning")))
#else
# define _GL_ATTRIBUTE_ERROR(msg)
# define _GL_ATTRIBUTE_WARNING(msg)
#endif

/* _GL_ATTRIBUTE_EXTERNALLY_VISIBLE declares that the entity should remain
   visible to debuggers etc., even with '-fwhole-program'.  */
/* Applies to: functions, variables.  */
#if _GL_HAS_ATTRIBUTE (externally_visible)
# define _GL_ATTRIBUTE_EXTERNALLY_VISIBLE __attribute__ ((externally_visible))
#else
# define _GL_ATTRIBUTE_EXTERNALLY_VISIBLE
#endif

/* _GL_ATTRIBUTE_FALLTHROUGH declares that it is not a programming mistake if
   the control flow falls through to the immediately following 'case' or
   'default' label.  The compiler should not warn in this case.  */
/* Applies to: Empty statement (;), inside a 'switch' statement.  */
/* Always expands to something.  */
#if _GL_HAS_C_ATTRIBUTE (fallthrough)
# define _GL_ATTRIBUTE_FALLTHROUGH [[__fallthrough__]]
#elif _GL_HAS_ATTRIBUTE (fallthrough)
# define _GL_ATTRIBUTE_FALLTHROUGH __attribute__ ((__fallthrough__))
#else
# define _GL_ATTRIBUTE_FALLTHROUGH ((void) 0)
#endif

/* _GL_ATTRIBUTE_FORMAT ((ARCHETYPE, STRING-INDEX, FIRST-TO-CHECK))
   declares that the STRING-INDEXth function argument is a format string of
   style ARCHETYPE, which is one of:
     printf, gnu_printf
     scanf, gnu_scanf,
     strftime, gnu_strftime,
     strfmon,
   or the same thing prefixed and suffixed with '__'.
   If FIRST-TO-CHECK is not 0, arguments starting at FIRST-TO_CHECK
   are suitable for the format string.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (format)
# define _GL_ATTRIBUTE_FORMAT(spec) __attribute__ ((__format__ spec))
#else
# define _GL_ATTRIBUTE_FORMAT(spec)
#endif

/* _GL_ATTRIBUTE_LEAF declares that if the function is called from some other
   compilation unit, it executes code from that unit only by return or by
   exception handling.  This declaration lets the compiler optimize that unit
   more aggressively.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (leaf)
# define _GL_ATTRIBUTE_LEAF __attribute__ ((__leaf__))
#else
# define _GL_ATTRIBUTE_LEAF
#endif

/* _GL_ATTRIBUTE_MALLOC declares that the function returns a pointer to freshly
   allocated memory.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (malloc)
# define _GL_ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
#else
# define _GL_ATTRIBUTE_MALLOC
#endif

/* _GL_ATTRIBUTE_MAY_ALIAS declares that pointers to the type may point to the
   same storage as pointers to other types.  Thus this declaration disables
   strict aliasing optimization.  */
/* Applies to: types.  */
/* Oracle Studio 12.6 mishandles may_alias despite __has_attribute OK.  */
#if _GL_HAS_ATTRIBUTE (may_alias) && !defined __SUNPRO_C
# define _GL_ATTRIBUTE_MAY_ALIAS __attribute__ ((__may_alias__))
#else
# define _GL_ATTRIBUTE_MAY_ALIAS
#endif

/* _GL_ATTRIBUTE_MAYBE_UNUSED declares that it is not a programming mistake if
   the entity is not used.  The compiler should not warn if the entity is not
   used.  */
/* Applies to:
     - function, variable,
     - struct, union, struct/union member,
     - enumeration, enumeration item,
     - typedef,
   in C++ also: class.  */
/* In C++ and C2x, this is spelled [[__maybe_unused__]].
   GCC's syntax is __attribute__ ((__unused__)).
   clang supports both syntaxes.  */
#if _GL_HAS_C_ATTRIBUTE (maybe_unused)
# define _GL_ATTRIBUTE_MAYBE_UNUSED [[__maybe_unused__]]
#else
# define _GL_ATTRIBUTE_MAYBE_UNUSED _GL_ATTRIBUTE_UNUSED
#endif
/* Alternative spelling of this macro, for convenience.  */
#define _GL_UNUSED _GL_ATTRIBUTE_MAYBE_UNUSED
/* Earlier spellings of this macro.  */
#define _UNUSED_PARAMETER_ _GL_ATTRIBUTE_MAYBE_UNUSED

/* _GL_ATTRIBUTE_NODISCARD declares that the caller of the function should not
   discard the return value.  The compiler may warn if the caller does not use
   the return value, unless the caller uses something like ignore_value.  */
/* Applies to: function, enumeration, class.  */
#if _GL_HAS_C_ATTRIBUTE (nodiscard)
# define _GL_ATTRIBUTE_NODISCARD [[__nodiscard__]]
#elif _GL_HAS_ATTRIBUTE (warn_unused_result)
# define _GL_ATTRIBUTE_NODISCARD __attribute__ ((__warn_unused_result__))
#else
# define _GL_ATTRIBUTE_NODISCARD
#endif

/* _GL_ATTRIBUTE_NOINLINE tells that the compiler should not inline the
   function.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (noinline)
# define _GL_ATTRIBUTE_NOINLINE __attribute__ ((__noinline__))
#else
# define _GL_ATTRIBUTE_NOINLINE
#endif

/* _GL_ATTRIBUTE_NONNULL ((N1, N2,...)) declares that the arguments N1, N2,...
   must not be NULL.
   _GL_ATTRIBUTE_NONNULL () declares that all pointer arguments must not be
   null.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (nonnull)
# define _GL_ATTRIBUTE_NONNULL(args) __attribute__ ((__nonnull__ args))
#else
# define _GL_ATTRIBUTE_NONNULL(args)
#endif

/* _GL_ATTRIBUTE_NONSTRING declares that the contents of a character array is
   not meant to be NUL-terminated.  */
/* Applies to: struct/union members and variables that are arrays of element
   type '[[un]signed] char'.  */
#if _GL_HAS_ATTRIBUTE (nonstring)
# define _GL_ATTRIBUTE_NONSTRING __attribute__ ((__nonstring__))
#else
# define _GL_ATTRIBUTE_NONSTRING
#endif

/* There is no _GL_ATTRIBUTE_NORETURN; use _Noreturn instead.  */

/* _GL_ATTRIBUTE_NOTHROW declares that the function does not throw exceptions.
 */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (nothrow) && !defined __cplusplus
# define _GL_ATTRIBUTE_NOTHROW __attribute__ ((__nothrow__))
#else
# define _GL_ATTRIBUTE_NOTHROW
#endif

/* _GL_ATTRIBUTE_PACKED declares:
   For struct members: The member has the smallest possible alignment.
   For struct, union, class: All members have the smallest possible alignment,
   minimizing the memory required.  */
/* Applies to: struct members, struct, union,
   in C++ also: class.  */
#if _GL_HAS_ATTRIBUTE (packed)
# define _GL_ATTRIBUTE_PACKED __attribute__ ((__packed__))
#else
# define _GL_ATTRIBUTE_PACKED
#endif

/* _GL_ATTRIBUTE_PURE declares that It is OK for a compiler to omit duplicate
   calls to the function with the same arguments if observable state is not
   changed between calls.
   This attribute is safe for a function that does not affect
   observable state, and always returns exactly once.
   (This attribute is looser than _GL_ATTRIBUTE_CONST.)  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (pure)
# define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
#else
# define _GL_ATTRIBUTE_PURE
#endif

/* _GL_ATTRIBUTE_RETURNS_NONNULL declares that the function's return value is
   a non-NULL pointer.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (returns_nonnull)
# define _GL_ATTRIBUTE_RETURNS_NONNULL __attribute__ ((__returns_nonnull__))
#else
# define _GL_ATTRIBUTE_RETURNS_NONNULL
#endif

/* _GL_ATTRIBUTE_SENTINEL(pos) declares that the variadic function expects a
   trailing NULL argument.
   _GL_ATTRIBUTE_SENTINEL () - The last argument is NULL (requires C99).
   _GL_ATTRIBUTE_SENTINEL ((N)) - The (N+1)st argument from the end is NULL.  */
/* Applies to: functions.  */
#if _GL_HAS_ATTRIBUTE (sentinel)
# define _GL_ATTRIBUTE_SENTINEL(pos) __attribute__ ((__sentinel__ pos))
#else
# define _GL_ATTRIBUTE_SENTINEL(pos)
#endif

/* A helper macro.  Don't use it directly.  */
#if _GL_HAS_ATTRIBUTE (unused)
# define _GL_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#else
# define _GL_ATTRIBUTE_UNUSED
#endif


/* _GL_UNUSED_LABEL; declares that it is not a programming mistake if the
   immediately preceding label is not used.  The compiler should not warn
   if the label is not used.  */
/* Applies to: label (both in C and C++).  */
/* Note that g++ < 4.5 does not support the '__attribute__ ((__unused__)) ;'
   syntax.  But clang does.  */
#if !(defined __cplusplus && !_GL_GNUC_PREREQ (4, 5)) || defined __clang__
# define _GL_UNUSED_LABEL _GL_ATTRIBUTE_UNUSED
#else
# define _GL_UNUSED_LABEL
#endif


/* Define as 'access' if you don't have the eaccess() function. */
#undef eaccess

/* Please see the Gnulib manual for how to use these macros.

   Suppress extern inline with HP-UX cc, as it appears to be broken; see
   <https://lists.gnu.org/r/bug-texinfo/2013-02/msg00030.html>.

   Suppress extern inline with Sun C in standards-conformance mode, as it
   mishandles inline functions that call each other.  E.g., for 'inline void f
   (void) { } inline void g (void) { f (); }', c99 incorrectly complains
   'reference to static identifier "f" in extern inline function'.
   This bug was observed with Oracle Developer Studio 12.6
   (Sun C 5.15 SunOS_sparc 2017/05/30).

   Suppress extern inline (with or without __attribute__ ((__gnu_inline__)))
   on configurations that mistakenly use 'static inline' to implement
   functions or macros in standard C headers like <ctype.h>.  For example,
   if isdigit is mistakenly implemented via a static inline function,
   a program containing an extern inline function that calls isdigit
   may not work since the C standard prohibits extern inline functions
   from calling static functions (ISO C 99 section 6.7.4.(3).
   This bug is known to occur on:

     OS X 10.8 and earlier; see:
     https://lists.gnu.org/r/bug-gnulib/2012-12/msg00023.html

     DragonFly; see
     http://muscles.dragonflybsd.org/bulk/clang-master-potential/20141111_102002/logs/ah-tty-0.3.12.log

     FreeBSD; see:
     https://lists.gnu.org/r/bug-gnulib/2014-07/msg00104.html

   OS X 10.9 has a macro __header_inline indicating the bug is fixed for C and
   for clang but remains for g++; see <https://trac.macports.org/ticket/41033>.
   Assume DragonFly and FreeBSD will be similar.

   GCC 4.3 and above with -std=c99 or -std=gnu99 implements ISO C99
   inline semantics, unless -fgnu89-inline is used.  It defines a macro
   __GNUC_STDC_INLINE__ to indicate this situation or a macro
   __GNUC_GNU_INLINE__ to indicate the opposite situation.
   GCC 4.2 with -std=c99 or -std=gnu99 implements the GNU C inline
   semantics but warns, unless -fgnu89-inline is used:
     warning: C99 inline functions are not supported; using GNU89
     warning: to disable this warning use -fgnu89-inline or the gnu_inline function attribute
   It defines a macro __GNUC_GNU_INLINE__ to indicate this situation.
 */
#if (((defined __APPLE__ && defined __MACH__) \
      || defined __DragonFly__ || defined __FreeBSD__) \
     && (defined __header_inline \
         ? (defined __cplusplus && defined __GNUC_STDC_INLINE__ \
            && ! defined __clang__) \
         : ((! defined _DONT_USE_CTYPE_INLINE_ \
             && (defined __GNUC__ || defined __cplusplus)) \
            || (defined _FORTIFY_SOURCE && 0 < _FORTIFY_SOURCE \
                && defined __GNUC__ && ! defined __cplusplus))))
# define _GL_EXTERN_INLINE_STDHEADER_BUG
#endif
#if ((__GNUC__ \
      ? defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__ \
      : (199901L <= __STDC_VERSION__ \
         && !defined __HP_cc \
         && !defined __PGI \
         && !(defined __SUNPRO_C && __STDC__))) \
     && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# define _GL_INLINE inline
# define _GL_EXTERN_INLINE extern inline
# define _GL_EXTERN_INLINE_IN_USE
#elif (2 < __GNUC__ + (7 <= __GNUC_MINOR__) && !defined __STRICT_ANSI__ \
       && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# if defined __GNUC_GNU_INLINE__ && __GNUC_GNU_INLINE__
   /* __gnu_inline__ suppresses a GCC 4.2 diagnostic.  */
#  define _GL_INLINE extern inline __attribute__ ((__gnu_inline__))
# else
#  define _GL_INLINE extern inline
# endif
# define _GL_EXTERN_INLINE extern
# define _GL_EXTERN_INLINE_IN_USE
#else
# define _GL_INLINE _GL_UNUSED static
# define _GL_EXTERN_INLINE _GL_UNUSED static
#endif

/* In GCC 4.6 (inclusive) to 5.1 (exclusive),
   suppress bogus "no previous prototype for 'FOO'"
   and "no previous declaration for 'FOO'" diagnostics,
   when FOO is an inline function in the header; see
   <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54113> and
   <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63877>.  */
#if __GNUC__ == 4 && 6 <= __GNUC_MINOR__
# if defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__
#  define _GL_INLINE_HEADER_CONST_PRAGMA
# else
#  define _GL_INLINE_HEADER_CONST_PRAGMA \
     _Pragma ("GCC diagnostic ignored \"-Wsuggest-attribute=const\"")
# endif
# define _GL_INLINE_HEADER_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-prototypes\"") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-declarations\"") \
    _GL_INLINE_HEADER_CONST_PRAGMA
# define _GL_INLINE_HEADER_END \
    _Pragma ("GCC diagnostic pop")
#else
# define _GL_INLINE_HEADER_BEGIN
# define _GL_INLINE_HEADER_END
#endif

/* Define to `int' if <sys/types.h> doesn't define. */
#undef gid_t

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#undef inline
#endif

/* Define to long or long long if <stdint.h> and <inttypes.h> don't define. */
#undef intmax_t

/* Work around a bug in Apple GCC 4.0.1 build 5465: In C99 mode, it supports
   the ISO C 99 semantics of 'extern inline' (unlike the GNU C semantics of
   earlier versions), but does not display it by setting __GNUC_STDC_INLINE__.
   __APPLE__ && __MACH__ test for Mac OS X.
   __APPLE_CC__ tests for the Apple compiler and its version.
   __STDC_VERSION__ tests for the C99 mode.  */
#if defined __APPLE__ && defined __MACH__ && __APPLE_CC__ >= 5465 && !defined __cplusplus && __STDC_VERSION__ >= 199901L && !defined __GNUC_STDC_INLINE__
# define __GNUC_STDC_INLINE__ 1
#endif

/* Define to a type if <wchar.h> does not define. */
#undef mbstate_t

/* _GL_CMP (n1, n2) performs a three-valued comparison on n1 vs. n2, where
   n1 and n2 are expressions without side effects, that evaluate to real
   numbers (excluding NaN).
   It returns
     1  if n1 > n2
     0  if n1 == n2
     -1 if n1 < n2
   The naïve code   (n1 > n2 ? 1 : n1 < n2 ? -1 : 0)  produces a conditional
   jump with nearly all GCC versions up to GCC 10.
   This variant     (n1 < n2 ? -1 : n1 > n2)  produces a conditional with many
   GCC versions up to GCC 9.
   The better code  (n1 > n2) - (n1 < n2)  from Hacker's Delight § 2-9
   avoids conditional jumps in all GCC versions >= 3.4.  */
#define _GL_CMP(n1, n2) (((n1) > (n2)) - ((n1) < (n2)))


/* Define to `int' if <sys/types.h> does not define. */
#undef mode_t

/* Define to the type of st_nlink in struct stat, or a supertype. */
#undef nlink_t

/* Define as a signed integer type capable of holding a process identifier. */
#undef pid_t

/* Define as the type of the result of subtracting two pointers, if the system
   doesn't define it. */
#undef ptrdiff_t

/* Define to the equivalent of the C99 'restrict' keyword, or to
   nothing if this is not supported.  Do not define if restrict is
   supported only directly.  */
#undef restrict
/* Work around a bug in older versions of Sun C++, which did not
   #define __restrict__ or support _Restrict or __restrict__
   even though the corresponding Sun C compiler ended up with
   "#define restrict _Restrict" or "#define restrict __restrict__"
   in the previous line.  This workaround can be removed once
   we assume Oracle Developer Studio 12.5 (2016) or later.  */
#if defined __SUNPRO_CC && !defined __RESTRICT && !defined __restrict__
# define _Restrict
# define __restrict__
#endif

/* Define as an integer type suitable for memory locations that can be
   accessed atomically even in the presence of asynchronous signals. */
#undef sig_atomic_t

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t

/* Define as a signed type of the same size as size_t. */
#undef ssize_t

/* Define to `int' if <sys/types.h> doesn't define. */
#undef uid_t


  /* This definition is a duplicate of the one in unitypes.h.
     It is here so that we can cope with an older version of unitypes.h
     that does not contain this definition and that is pre-installed among
     the public header files.  */
  # if defined __restrict \
       || 2 < __GNUC__ + (95 <= __GNUC_MINOR__) \
       || __clang_major__ >= 3
  #  define _UC_RESTRICT __restrict
  # elif 199901L <= __STDC_VERSION__ || defined restrict
  #  define _UC_RESTRICT restrict
  # else
  #  define _UC_RESTRICT
  # endif
  
