libpipeline 1.5.7 (13 November 2022)
====================================

 * Add notes to libpipeline(3) of when functions were added.
 * Transferred Git repository to https://gitlab.com/libpipeline/libpipeline.
 * Make `socketpair` tests used by `./configure --enable-socketpair-pipe`
   compatible with C23.

libpipeline 1.5.6 (24 April 2022)
=================================

 * Fix handling of leading whitespace in `pipecmd_new_argstr` and
   `pipecmd_argstr`.

libpipeline 1.5.5 (3 January 2022)
==================================

 * Move release process to GitLab CI.

libpipeline 1.5.4 (7 November 2021)
===================================

 * Move Git repository to GitLab (https://gitlab.com/cjwatson/libpipeline).
 * Building libpipeline now requires Autoconf >= 2.64.

libpipeline 1.5.3 (13 August 2020)
==================================

 * Fix implementation-defined behaviour while handling read/write errors.
 * Port tests to the modern Check API.

libpipeline 1.5.2 (1 January 2020)
==================================

 * `pipecmd_exec` now calls `_exit` rather than `exit`, to avoid bugs such
   as functions registered using `atexit` being called multiple times.
 * Document that standard FDs must be open before calling `pipeline_start`.

libpipeline 1.5.1 (27 January 2019)
===================================

 * Building libpipeline now requires Autoconf >= 2.63 and Automake >=
   1.11.2.
 * Various portability improvements, via Gnulib.

libpipeline 1.5.0 (14 November 2017)
====================================

 * Add `pipecmd_pre_exec` to install a pre-exec handler for a single
   command.

libpipeline 1.4.2 (10 July 2017)
================================

 * Fix EOF detection in `get_line`.

libpipeline 1.4.1 (17 August 2015)
==================================

 * Fix test failure with Check 0.10.0.

libpipeline 1.4.0 (25 October 2014)
===================================

 * Various portability fixes for Solaris, mostly suggested by Peter Bray.
   This includes a `PIPELINE_QUIET` environment variable which suppresses
   the error message normally emitted when a subprocess is terminated by a
   signal.
 * Add `pipecmd_fchdir`, which is analogous to `pipecmd_chdir` but takes an
   open file descriptor rather than a path.

libpipeline 1.3.1 (22 September 2014)
=====================================

 * Fix test failures on Cygwin.
 * Fix build on systems with neither `setenv` nor `clearenv`, e.g. Solaris
   8.

libpipeline 1.3.0 (26 March 2014)
=================================

 * Add `pipecmd_chdir` to run a command with a different working directory.

libpipeline 1.2.6 (18 December 2013)
====================================

 * Fix `test_pump_tee` to wait for its child processes before testing their
   output, since otherwise not all their output might be flushed.

libpipeline 1.2.5 (3 December 2013)
===================================

 * `pipeline_want_infile` and `pipeline_want_outfile` now take copies of
   their file name arguments.
 * Restore compatibility with Automake 1.10.
 * Build with large file support on systems where it is available.

libpipeline 1.2.4 (6 June 2013)
===============================

 * Provide a replacement `clearenv` function in order to support non-glibc
   systems.  Now tested on FreeBSD.

libpipeline 1.2.3 (24 April 2013)
=================================

 * Don't read uninitialised memory when testing for the end of long lines
   (over 4096 bytes).
 * Flush stdio before starting a sequence.

libpipeline 1.2.2 (17 September 2012)
=====================================

 * Make `pipeline_start` flush stdio after emitting debugging output.
 * Build fixes for glibc 2.16 and Automake 1.12.

libpipeline 1.2.1 (2 March 2012)
================================

 * Retry reads and writes on `EINTR`.
 * Fix opening of output files requested by `pipeline_want_outfile`; these
   are now created if they do not already exist, and truncated if they do.
 * `<pipeline.h>` is now wrapped in `extern "C"` when used in a C++
   compilation unit.

libpipeline 1.2.0 (18 March 2011)
=================================

 * Add `pipeline_get_pid` to get the process ID of a command in a started
   pipeline.

libpipeline 1.1.0 (11 December 2010)
====================================

 * Add `pipecmd_exec` to execute a single command, replacing the current
   process; this is analogous to `execvp`.
 * Add `pipecmd_clearenv` to clear a command's environment; this is
   analogous to `clearenv`.
 * Add `pipecmd_get_nargs` to get the number of arguments to a command.

libpipeline 1.0.0 (29 October 2010)
===================================

This is a new C library for pipeline manipulation, broken out from man-db
2.5.8.
