/*
 * Copyright (C) 2001-2017 Colin Watson.
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

#ifndef PIPELINE_PRIVATE_H
#define PIPELINE_PRIVATE_H

#include "pipeline.h"

/* exit codes */
#define OK 0    /* success */
#define FAIL 1  /* usage or syntax error */
#define FATAL 2 /* operational error */

extern char *appendstr (char *, ...)
        PIPELINE_ATTR_SENTINEL PIPELINE_ATTR_WARN_UNUSED_RESULT;

extern void init_debug (void);
extern int debug_level;
extern void debug (const char *message, ...)
        PIPELINE_ATTR_FORMAT_PRINTF (1, 2);

#if defined(HAVE_SETENV) && !defined(HAVE_CLEARENV)
extern int clearenv (void);
#endif

enum pipecmd_tag {
	PIPECMD_PROCESS,
	PIPECMD_FUNCTION,
	PIPECMD_SEQUENCE
};

struct pipecmd_env {
	char *name;
	char *value;
};

struct pipecmd {
	enum pipecmd_tag tag;
	char *name;
	int nice;
	int discard_err; /* discard stderr? */
	int cwd_fd;
	char *cwd;
	int nenv;
	int env_max; /* size of allocated array */
	struct pipecmd_env *env;
	pipecmd_function_type *pre_exec_func;
	pipecmd_function_type *pre_exec_free_func;
	void *pre_exec_data;
	union {
		struct pipecmd_process {
			int argc;
			int argv_max; /* size of allocated array */
			char **argv;
		} process;
		struct pipecmd_function {
			pipecmd_function_type *func;
			pipecmd_function_free_type *free_func;
			void *data;
		} function;
		struct pipecmd_sequence {
			int ncommands;
			int commands_max;
			struct pipecmd **commands;
		} sequence;
	} u;
};

enum pipeline_redirect {
	REDIRECT_NONE,
	REDIRECT_FD,
	REDIRECT_FILE_NAME
};

struct pipeline {
	int ncommands;
	int commands_max; /* size of allocated array */
	pipecmd **commands;
	pid_t *pids;
	int *statuses; /* -1 until command exits */

	/* REDIRECT_NONE for no redirection; REDIRECT_FD for redirection
	 * from/to file descriptor; REDIRECT_FILE_NAME for redirection
	 * from/to file name.
	 */
	enum pipeline_redirect redirect_in, redirect_out;

	/* If non-negative, these contain caller-supplied file descriptors
	 * for the input and output of the whole pipeline.  If negative,
	 * pipeline_start() will create pipes and store the input writing
	 * half and the output reading half in infd and outfd as
	 * appropriate.
	 */
	int want_in, want_out;

	/* If non-NULL, these contain files to open and use as the input and
	 * output of the whole pipeline.  These are only used if want_in or
	 * want_out respectively is zero.  The value of using these rather
	 * than simply opening the files before starting the pipeline is
	 * that the files will be opened with the same privileges under
	 * which the pipeline is being run.
	 */
	char *want_infile, *want_outfile;

	/* See above. Default to -1. The caller should consider these
	 * read-only.
	 */
	int infd, outfd;

	/* Set by pipeline_get_infile() and pipeline_get_outfile()
	 * respectively. Default to NULL.
	 */
	FILE *infile, *outfile;

	/* Set by pipeline_connect() to record that this pipeline reads its
	 * input from another pipeline. Defaults to NULL.
	 */
	struct pipeline *source;

	/* Private buffer for use by read/peek functions. */
	char *buffer;
	size_t buflen, bufmax;

	/* The last line returned by readline/peekline. Private. */
	char *line_cache;

	/* The amount of data at the end of buffer which has been
	 * read-ahead, either by an explicit peek or by readline/peekline
	 * reading a block at a time to save work. Private.
	 */
	size_t peek_offset;

	/* If set, ignore SIGINT and SIGQUIT while the pipeline is running,
	 * like system(). Defaults to 1.
	 */
	int ignore_signals;
};

#endif /* PIPELINE_PRIVATE_H */
