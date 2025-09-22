/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2002
 * Free Software Foundation, Inc.
 * Copyright (C) 2003-2017 Colin Watson.
 *   Written for groff by James Clark (jjc@jclark.com)
 *   Adapted for man-db by Colin Watson.
 *
 * This file is part of libpipeline.
 *
 * libpipeline is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * libpipeline is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libpipeline; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#ifndef PIPELINE_H
#define PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>

/* GCC version checking borrowed from glibc. */
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#  define PIPELINE_GNUC_PREREQ(maj, min)                                      \
    ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#  define PIPELINE_GNUC_PREREQ(maj, min) 0
#endif

/* Does this compiler support format string checking? */
#if PIPELINE_GNUC_PREREQ(2, 0)
#  define PIPELINE_ATTR_FORMAT_PRINTF(a, b)                                   \
    __attribute__ ((__format__ (__printf__, a, b)))
#else
#  define PIPELINE_ATTR_FORMAT_PRINTF(a, b)
#endif

/* Does this compiler support marking variables as unused? */
/* This is no longer used internally and isn't documented as part of
 * libpipeline's public API, but is preserved just in case somebody used it
 * anyway.
 */
#if PIPELINE_GNUC_PREREQ(2, 4)
#  define PIPELINE_ATTR_UNUSED __attribute__ ((__unused__))
#else
#  define PIPELINE_ATTR_UNUSED
#endif

/* Does this compiler support marking functions as non-returning? */
#if PIPELINE_GNUC_PREREQ(2, 5)
#  define PIPELINE_ATTR_NORETURN __attribute__ ((__noreturn__))
#else
#  define PIPELINE_ATTR_NORETURN
#endif

/* Does this compiler support unused result checking? */
#if PIPELINE_GNUC_PREREQ(3, 4)
#  define PIPELINE_ATTR_WARN_UNUSED_RESULT                                    \
    __attribute__ ((__warn_unused_result__))
#else
#  define PIPELINE_ATTR_WARN_UNUSED_RESULT
#endif

/* Does this compiler support sentinel checking? */
#if PIPELINE_GNUC_PREREQ(4, 0)
#  define PIPELINE_ATTR_SENTINEL __attribute__ ((__sentinel__))
#else
#  define PIPELINE_ATTR_SENTINEL
#endif

typedef void pipecmd_function_type (void *);
typedef void pipecmd_function_free_type (void *);

struct pipecmd;
typedef struct pipecmd pipecmd;

struct pipeline;
typedef struct pipeline pipeline;

/* ---------------------------------------------------------------------- */

/* Functions to build individual commands. */

/* Construct a new command. */
pipecmd *pipecmd_new (const char *name);

/* Convenience constructors wrapping pipecmd_new() and pipecmd_arg().
 * Terminate arguments with NULL.
 */
pipecmd *pipecmd_new_argv (const char *name, va_list argv);
pipecmd *pipecmd_new_args (const char *name, ...) PIPELINE_ATTR_SENTINEL;

/* Split argstr on whitespace to construct a command and arguments,
 * honouring shell-style single-quoting, double-quoting, and backslashes,
 * but not other shell evil like wildcards, semicolons, or backquotes. This
 * is a backward-compatibility hack to support old configuration file
 * directives; please try to avoid using it in new code.
 */
pipecmd *pipecmd_new_argstr (const char *argstr);

/* Construct a new command that calls a given function rather than executing
 * a process. The data argument is passed as the function's only argument,
 * and will be freed before returning using free_func (if non-NULL).
 *
 * pipecmd_* functions that deal with arguments cannot be used with the
 * command returned by this function.
 */
pipecmd *pipecmd_new_function (const char *name, pipecmd_function_type *func,
                               pipecmd_function_free_type *free_func,
                               void *data);

/* Construct a new command that runs a sequence of commands. The commands
 * will be executed in forked children; if any exits non-zero then it will
 * terminate the sequence, as with "&&" in shell.
 *
 * pipecmd_* functions that deal with arguments cannot be used with the
 * command returned by this function.
 */
pipecmd *pipecmd_new_sequencev (const char *name, va_list cmdv);
pipecmd *pipecmd_new_sequence (const char *name, ...) PIPELINE_ATTR_SENTINEL;

/* Return a new command that just passes data from its input to its output. */
pipecmd *pipecmd_new_passthrough (void);

/* Return a duplicate of a command. */
pipecmd *pipecmd_dup (pipecmd *cmd);

/* Add an argument to a command. */
void pipecmd_arg (pipecmd *cmd, const char *arg);

/* Convenience function to add an argument with printf substitutions. */
void pipecmd_argf (pipecmd *cmd, const char *format, ...)
        PIPELINE_ATTR_FORMAT_PRINTF (2, 3);

/* Convenience functions wrapping pipecmd_arg().
 * Terminate arguments with NULL.
 */
void pipecmd_argv (pipecmd *cmd, va_list argv);
void pipecmd_args (pipecmd *cmd, ...) PIPELINE_ATTR_SENTINEL;

/* Split argstr on whitespace to add a list of arguments, honouring
 * shell-style single-quoting, double-quoting, and backslashes, but not
 * other shell evil like wildcards, semicolons, or backquotes. This is a
 * backward-compatibility hack to support old configuration file directives;
 * please try to avoid using it in new code.
 */
void pipecmd_argstr (pipecmd *cmd, const char *argstr);

/* Return the number of arguments to this command.  Note that this includes
 * the command name as the first argument, so the command 'echo foo bar' is
 * counted as having three arguments.
 */
int pipecmd_get_nargs (pipecmd *cmd);

/* Set the nice(3) value for this command.  Defaults to 0.  Errors while
 * attempting to set the nice value are ignored, aside from emitting a debug
 * message.
 */
void pipecmd_nice (pipecmd *cmd, int nice);

/* If discard_err is non-zero, redirect this command's standard error to
 * /dev/null.  Otherwise, and by default, pass it through.
 */
void pipecmd_discard_err (pipecmd *cmd, int discard_err);

/* Change to this directory while running this command. */
void pipecmd_chdir (pipecmd *cmd, const char *directory);

/* Change to the directory given by this open file descriptor while running
 * this command. */
void pipecmd_fchdir (pipecmd *cmd, int directory_fd);

/* Set an environment variable while running this command. */
void pipecmd_setenv (pipecmd *cmd, const char *name, const char *value);

/* Unset an environment variable while running this command. */
void pipecmd_unsetenv (pipecmd *cmd, const char *name);

/* Clear the environment while running this command.  (Note that environment
 * operations work in sequence; pipecmd_clearenv followed by pipecmd_setenv
 * causes the command to have just a single environment variable set.)
 * Beware that this may cause unexpected failures, for example if some of
 * the contents of the environment are necessary to execute programs at all
 * (say, PATH).
 */
void pipecmd_clearenv (pipecmd *cmd);

/* Install a pre-exec handler.  This will be run immediately before
 * executing the command's payload (process or function).  Pass NULL to
 * clear any existing pre-exec handler.  The data argument is passed as the
 * function's only argument, and will be freed before returning using
 * free_func (if non-NULL).
 *
 * This is similar to pipeline_install_post_fork, except that is specific to
 * a single command rather than installing a global handler, and it runs
 * slightly later (immediately before exec rather than immediately after
 * fork).
 */
void pipecmd_pre_exec (pipecmd *cmd, pipecmd_function_type *func,
                       pipecmd_function_free_type *free_func, void *data);

/* Add a command to a sequence. */
void pipecmd_sequence_command (pipecmd *cmd, pipecmd *child);

/* Dump a string representation of a command to stream. */
void pipecmd_dump (pipecmd *cmd, FILE *stream);

/* Return a string representation of a command. The caller should free the
 * result.
 */
char *pipecmd_tostring (pipecmd *cmd);

/* Execute a single command, replacing the current process.  Never returns,
 * instead exiting non-zero on failure.
 */
void pipecmd_exec (pipecmd *cmd) PIPELINE_ATTR_NORETURN;

/* Destroy a command. Safely does nothing on NULL. */
void pipecmd_free (pipecmd *cmd);

/* ---------------------------------------------------------------------- */

/* Functions to build pipelines. */

/* Construct a new pipeline. */
pipeline *pipeline_new (void);

/* Convenience constructors wrapping pipeline_new() and pipeline_command().
 * Terminate commands with NULL.
 */
pipeline *pipeline_new_commandv (pipecmd *cmd1, va_list cmdv);
pipeline *pipeline_new_commands (pipecmd *cmd1, ...) PIPELINE_ATTR_SENTINEL;

/* Construct a new pipeline and add a single command to it. */
pipeline *pipeline_new_command_argv (const char *name, va_list argv);
pipeline *pipeline_new_command_args (const char *name,
                                     ...) PIPELINE_ATTR_SENTINEL;

/* Joins two pipelines, neither of which are allowed to be started. Discards
 * want_out, want_outfile, and outfd from p1, and want_in, want_infile, and
 * infd from p2.
 */
pipeline *pipeline_join (pipeline *p1, pipeline *p2);

/* Connect the input of one or more sink pipelines to the output of a source
 * pipeline. The source pipeline may be started, but in that case want_out
 * must be negative; otherwise, discards want_out from source. In any event,
 * discards want_in from all sinks, none of which are allowed to be started.
 * Terminate arguments with NULL.
 *
 * This is an application-level connection; data may be intercepted between
 * the pipelines by the program before calling pipeline_pump(), which sets
 * data flowing from the source to the sinks. It is primarily useful when
 * more than one sink pipeline is involved, in which case the pipelines
 * cannot simply be concatenated into one.
 */
void pipeline_connect (pipeline *source, pipeline *sink,
                       ...) PIPELINE_ATTR_SENTINEL;

/* Add a command to a pipeline. */
void pipeline_command (pipeline *p, pipecmd *cmd);

/* Construct a new command and add it to a pipeline in one go. */
void pipeline_command_argv (pipeline *p, const char *name, va_list argv);
void pipeline_command_args (pipeline *p, const char *name,
                            ...) PIPELINE_ATTR_SENTINEL;

/* Construct a new command from a shell-quoted string and add it to a
 * pipeline in one go. See the comment against pipecmd_new_argstr() above if
 * you're tempted to use this function.
 */
void pipeline_command_argstr (pipeline *p, const char *argstr);

/* Convenience functions wrapping pipeline_command().
 * Terminate commands with NULL.
 */
void pipeline_commandv (pipeline *p, va_list cmdv);
void pipeline_commands (pipeline *p, ...) PIPELINE_ATTR_SENTINEL;

/* Return the number of commands in this pipeline. */
int pipeline_get_ncommands (pipeline *p);

/* Return command number n from this pipeline, counting from zero, or NULL
 * if n is out of range.
 */
pipecmd *pipeline_get_command (pipeline *p, int n);

/* Set command number n in this pipeline, counting from zero, to cmd, and
 * return the previous command in that position.  Do nothing and return NULL
 * if n is out of range.
 */
pipecmd *pipeline_set_command (pipeline *p, int n, pipecmd *cmd);

/* Return the process ID of command number n from this pipeline, counting
 * from zero.  The pipeline must be started.  Return -1 if n is out of range
 * or if the command has already exited and been reaped.
 */
pid_t pipeline_get_pid (pipeline *p, int n);

/* Set file descriptors to use as the input and output of the whole
 * pipeline.  If non-negative, fd is used directly as a file descriptor.  If
 * negative, pipeline_start will create pipes and store the input writing
 * half and the output reading half in the pipeline's infd or outfd field as
 * appropriate.  The default is to leave input and output as stdin and
 * stdout unless pipeline_want_infile or pipeline_want_outfile respectively
 * has been called.
 *
 * Calling these functions supersedes any previous call to
 * pipeline_want_infile or pipeline_want_outfile respectively.
 */
void pipeline_want_in (pipeline *p, int fd);
void pipeline_want_out (pipeline *p, int fd);

/* Set file names to open and use as the input and output of the whole
 * pipeline.  This may be more convenient than supplying file descriptors,
 * and guarantees that the files are opened with the same privileges under
 * which the pipeline is run.
 *
 * Calling these functions (even with NULL, which returns to the default of
 * leaving input and output as stdin and stdout) supersedes any previous
 * call to pipeline_want_in or pipeline_want_outfile respectively.
 *
 * The given files will be opened when the pipeline is started.  If an
 * output file does not already exist, it is created (with mode 0666
 * modified in the usual way by umask); if it does exist, then it is
 * truncated.
 */
void pipeline_want_infile (pipeline *p, const char *file);
void pipeline_want_outfile (pipeline *p, const char *file);

/* If ignore_signals is non-zero (which is the default), ignore SIGINT and
 * SIGQUIT while the pipeline is running, like system().  Otherwise, leave
 * their dispositions unchanged.
 */
void pipeline_ignore_signals (pipeline *p, int ignore_signals);

/* Get streams corresponding to infd and outfd respectively. The pipeline
 * must be started.
 */
FILE *pipeline_get_infile (pipeline *p);
FILE *pipeline_get_outfile (pipeline *p);

/* Dump a string representation of p to stream. */
void pipeline_dump (pipeline *p, FILE *stream);

/* Return a string representation of p. The caller should free the result. */
char *pipeline_tostring (pipeline *p);

/* Destroy a pipeline and all its commands. Safely does nothing on NULL.
 * May wait for the pipeline to complete if it has not already done so.
 */
void pipeline_free (pipeline *p);

/* ---------------------------------------------------------------------- */

/* Functions to run pipelines and handle signals. */

typedef void pipeline_post_fork_fn (void);

/* Install a post-fork handler.  This will be run in any child process
 * immediately after it is forked.  For instance, this may be used for
 * cleaning up application-specific signal handlers.  Pass NULL to clear any
 * existing post-fork handler.
 *
 * See pipecmd_pre_exec for a similar facility limited to a single command
 * rather than global to the calling process.
 */
void pipeline_install_post_fork (pipeline_post_fork_fn *fn);

/* Start the processes in a pipeline.  Installs this library's SIGCHLD
 * handler if not already installed.  Calls error(FATAL) on error.
 *
 * The standard file descriptors (0, 1, and 2) must be open before calling
 * this function.
 */
void pipeline_start (pipeline *p);

/* Wait for a pipeline to complete.  Set *statuses to a newly-allocated
 * array of wait statuses, as returned by waitpid, and *n_statuses to the
 * length of that array.  The return value is similar to the exit status
 * that a shell would return, with some modifications.  If the last command
 * exits with a signal (other than SIGPIPE, which is considered equivalent
 * to exiting zero), then the return value is 128 plus the signal number; if
 * the last command exits normally but non-zero, then the return value is
 * its exit status; if any other command exits non-zero, then the return
 * value is 127; otherwise, the return value is 0.  This means that the
 * return value is only 0 if all commands in the pipeline exit successfully.
 */
int pipeline_wait_all (pipeline *p, int **statuses, int *n_statuses);

/* Wait for a pipeline to complete and return its combined exit status,
 * calculated as for pipeline_wait_all().
 */
int pipeline_wait (pipeline *p);

/* Start a pipeline, wait for it to complete, and free it, all in one go. */
int pipeline_run (pipeline *p);

/* Pump data among one or more pipelines connected using pipeline_connect()
 * until all source pipelines have reached end-of-file and all data has been
 * written to all sinks (or failed). All relevant pipelines must be
 * supplied: that is, no pipeline that has been connected to a source
 * pipeline may be supplied unless that source pipeline is also supplied.
 * Automatically starts all pipelines if they are not already started, but
 * does not wait for them. Terminate arguments with NULL.
 */
void pipeline_pump (pipeline *p, ...) PIPELINE_ATTR_SENTINEL;

/* ---------------------------------------------------------------------- */

/* Functions to read output from pipelines. */

/* Read len bytes of data from the pipeline, returning the data block. len
 * is updated with the number of bytes read.
 */
const char *pipeline_read (pipeline *p, size_t *len);

/* Look ahead in the pipeline's output for len bytes of data, returning the
 * data block. len is updated with the number of bytes read. The starting
 * position of the next read or peek is not affected by this call.
 */
const char *pipeline_peek (pipeline *p, size_t *len);

/* Return the number of bytes of data that can be read using pipeline_read
 * or pipeline_peek solely from the peek cache, without having to read from
 * the pipeline itself (and thus potentially block).
 */
size_t pipeline_peek_size (pipeline *p);

/* Skip over and discard len bytes of data from the peek cache. Asserts that
 * enough data is available to skip, so you may want to check using
 * pipeline_peek_size first.
 */
void pipeline_peek_skip (pipeline *p, size_t len);

/* Read a line of data from the pipeline, returning it. */
const char *pipeline_readline (pipeline *p);

/* Look ahead in the pipeline's output for a line of data, returning it. The
 * starting position of the next read or peek is not affected by this call.
 */
const char *pipeline_peekline (pipeline *p);

#ifdef __cplusplus
}
#endif

#endif /* PIPELINE_H */
