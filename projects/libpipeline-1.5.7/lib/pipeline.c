/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002, 2003
 * Free Software Foundation, Inc.
 * Copyright (C) 2003-2017 Colin Watson.
 *   Written for groff by James Clark (jjc@jclark.com)
 *   Heavily adapted and extended for man-db by Colin Watson.
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "attribute.h"
#include "dirname.h"
#include "full-write.h"
#include "safe-read.h"
#include "safe-write.h"
#include "xalloc.h"
#include "xstrndup.h"
#include "xvasprintf.h"

#include "error.h"
#include "fatal.h"
#include "pipeline-private.h"
#include "pipeline.h"

#ifdef USE_SOCKETPAIR_PIPE
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  ifdef CORRECT_SOCKETPAIR_MODE
#    include <sys/stat.h>
#  endif
#  ifndef SHUT_RD
#    define SHUT_RD 0
#  endif
#  ifndef SHUT_WR
#    define SHUT_WR 1
#  endif
#  ifdef CORRECT_SOCKETPAIR_MODE
#    define pipe(p)                                                           \
      (((socketpair (AF_UNIX, SOCK_STREAM, 0, p) < 0) ||                      \
	(shutdown ((p)[1], SHUT_RD) < 0) || (fchmod ((p)[1], S_IWUSR) < 0) || \
	(shutdown ((p)[0], SHUT_WR) < 0) || (fchmod ((p)[0], S_IRUSR) < 0))   \
	       ? -1                                                           \
	       : 0)
#  else
#    define pipe(p)                                                           \
      (((socketpair (AF_UNIX, SOCK_STREAM, 0, p) < 0) ||                      \
	(shutdown ((p)[1], SHUT_RD) < 0) || (shutdown ((p)[0], SHUT_WR) < 0)) \
	       ? -1                                                           \
	       : 0)
#  endif
#endif

#if defined(HAVE_SETENV) && !defined(HAVE_CLEARENV)
int clearenv (void)
{
	/* According to:
	 *   https://github.com/dovecot/core/blob/2.3.14/src/lib/env-util.c#L63-L70
	 * simply setting "environ = NULL" crashes some systems.  Creating a
	 * new environment consisting of just a terminator is indeed
	 * probably the best we can do.
	 */
	environ = XCALLOC (1, char *);
	return 0;
}
#endif

/* ---------------------------------------------------------------------- */

/* Functions to build individual commands. */

pipecmd *pipecmd_new (const char *name)
{
	pipecmd *cmd = XMALLOC (pipecmd);
	struct pipecmd_process *cmdp;
	char *name_base;

	cmd->tag = PIPECMD_PROCESS;
	cmd->name = xstrdup (name);
	cmd->nice = 0;
	cmd->discard_err = 0;
	cmd->cwd_fd = -1;
	cmd->cwd = NULL;

	cmd->nenv = 0;
	cmd->env_max = 4;
	cmd->env = xnmalloc (cmd->env_max, sizeof *cmd->env);

	cmd->pre_exec_func = NULL;
	cmd->pre_exec_free_func = NULL;
	cmd->pre_exec_data = NULL;

	cmdp = &cmd->u.process;

	cmdp->argc = 0;
	cmdp->argv_max = 4;
	cmdp->argv = xnmalloc (cmdp->argv_max, sizeof *cmdp->argv);

	/* argv[0] is the basename of the command name. */
	name_base = base_name (name);
	pipecmd_arg (cmd, name_base);
	free (name_base);

	return cmd;
}

pipecmd *pipecmd_new_argv (const char *name, va_list argv)
{
	pipecmd *cmd = pipecmd_new (name);
	pipecmd_argv (cmd, argv);
	return cmd;
}

pipecmd *pipecmd_new_args (const char *name, ...)
{
	va_list argv;
	pipecmd *cmd;

	va_start (argv, name);
	cmd = pipecmd_new_argv (name, argv);
	va_end (argv);

	return cmd;
}

/* As suggested in the header file, this function (for pipecmd_new_argstr()
 * and pipecmd_argstr()) is really a wart. If we didn't have to worry about
 * old configuration files then it wouldn't be necessary. Worse, the
 * definition for tr in man_db.conf currently contains single-quoting, and
 * people probably took that as a licence to do similar things, so we're
 * obliged to worry about quoting as well!
 *
 * However, we can mitigate this; shell quoting alone is safe though
 * sometimes confusing, but it's other shell constructs that tend to cause
 * real security holes. Therefore, rather than punting to 'sh -c' or
 * whatever, we parse a safe subset manually. Environment variables are not
 * currently handled because of tricky word splitting issues, but in
 * principle they could be if there's demand for it.
 *
 * TODO: Support setting environment variables.
 */
static char *argstr_get_word (const char **argstr)
{
	char *out = NULL;
	const char *litstart;
	enum {
		NONE,
		SINGLE,
		DOUBLE
	} quotemode = NONE;

	/* Skip over any leading whitespace. */
	while (**argstr == ' ' || **argstr == '\t')
		++*argstr;

	litstart = *argstr;
	while (**argstr) {
		char backslashed[2];

		/* If it's just a literal character, go round again. */
		if ((quotemode == NONE && !strchr (" \t'\"\\", **argstr)) ||
		    /* nothing is special in '; terminated by ' */
		    (quotemode == SINGLE && **argstr != '\'') ||
		    /* \ is special in "; terminated by " */
		    (quotemode == DOUBLE && !strchr ("\"\\", **argstr))) {
			++*argstr;
			continue;
		}

		/* Within "", \ is only special when followed by $, `, ", or
		 * \ (or <newline> in a real shell, but we don't do that).
		 */
		if (quotemode == DOUBLE && **argstr == '\\' &&
		    !strchr ("$`\"\\", *(*argstr + 1))) {
			++*argstr;
			continue;
		}

		/* Copy any accumulated literal characters. */
		if (litstart < *argstr) {
			char *tmp = xstrndup (litstart, *argstr - litstart);
			out = appendstr (out, tmp, (void *) 0);
			free (tmp);
		}

		switch (**argstr) {
			case ' ':
			case '\t':
				/* End of word. */
				return out;

			case '\'':
				if (quotemode != NONE)
					quotemode = NONE;
				else
					quotemode = SINGLE;
				litstart = ++*argstr;
				break;

			case '"':
				if (quotemode != NONE)
					quotemode = NONE;
				else
					quotemode = DOUBLE;
				litstart = ++*argstr;
				break;

			case '\\':
				backslashed[0] = *++*argstr;
				if (!backslashed[0]) {
					/* Unterminated quoting; give up. */
					free (out);
					return NULL;
				}
				backslashed[1] = '\0';
				out = appendstr (out, backslashed, (void *) 0);
				litstart = ++*argstr;
				break;

			default:
				assert (!"unexpected state parsing argstr");
		}
	}

	if (quotemode != NONE) {
		/* Unterminated quoting; give up. */
		free (out);
		return NULL;
	}

	/* Copy any accumulated literal characters. */
	if (litstart < *argstr) {
		char *tmp = xstrndup (litstart, *argstr - litstart);
		out = appendstr (out, tmp, (void *) 0);
		free (tmp);
	}

	return out;
}

pipecmd *pipecmd_new_argstr (const char *argstr)
{
	pipecmd *cmd;
	char *arg;

	arg = argstr_get_word (&argstr);
	if (!arg)
		fatal (0, "badly formed configuration directive: '%s'",
		       argstr);
	if (!strcmp (arg, "exec")) {
		/* Some old configuration files have "exec command" rather
		 * than "command"; this worked fine when being evaluated by
		 * a shell, but since exec is a shell builtin it doesn't
		 * work when being executed directly. To work around this,
		 * we just drop "exec" if it appears at the start of argstr.
		 */
		free (arg);
		arg = argstr_get_word (&argstr);
		if (!arg)
			fatal (0, "badly formed configuration directive: '%s'",
			       argstr);
	}
	cmd = pipecmd_new (arg);
	free (arg);

	while ((arg = argstr_get_word (&argstr))) {
		pipecmd_arg (cmd, arg);
		free (arg);
	}

	return cmd;
}

pipecmd *pipecmd_new_function (const char *name, pipecmd_function_type *func,
                               pipecmd_function_free_type *free_func,
                               void *data)
{
	pipecmd *cmd = XMALLOC (pipecmd);
	struct pipecmd_function *cmdf;

	cmd->tag = PIPECMD_FUNCTION;
	cmd->name = xstrdup (name);
	cmd->nice = 0;
	cmd->discard_err = 0;
	cmd->cwd_fd = -1;
	cmd->cwd = NULL;

	cmd->nenv = 0;
	cmd->env_max = 4;
	cmd->env = xnmalloc (cmd->env_max, sizeof *cmd->env);

	cmd->pre_exec_func = NULL;
	cmd->pre_exec_free_func = NULL;
	cmd->pre_exec_data = NULL;

	cmdf = &cmd->u.function;

	cmdf->func = func;
	cmdf->free_func = free_func;
	cmdf->data = data;

	return cmd;
}

pipecmd *pipecmd_new_sequencev (const char *name, va_list cmdv)
{
	pipecmd *cmd = XMALLOC (pipecmd);
	struct pipecmd_sequence *cmds;
	pipecmd *child;

	cmd->tag = PIPECMD_SEQUENCE;
	cmd->name = xstrdup (name);
	cmd->nice = 0;
	cmd->discard_err = 0;
	cmd->cwd_fd = -1;
	cmd->cwd = NULL;

	cmd->nenv = 0;
	cmd->env_max = 4;
	cmd->env = xnmalloc (cmd->env_max, sizeof *cmd->env);

	cmd->pre_exec_func = NULL;
	cmd->pre_exec_free_func = NULL;
	cmd->pre_exec_data = NULL;

	cmds = &cmd->u.sequence;

	cmds->ncommands = 0;
	cmds->commands_max = 4;
	cmds->commands = xnmalloc (cmds->commands_max, sizeof *cmds->commands);

	child = va_arg (cmdv, pipecmd *);
	while (child) {
		pipecmd_sequence_command (cmd, child);
		child = va_arg (cmdv, pipecmd *);
	}

	return cmd;
}

pipecmd *pipecmd_new_sequence (const char *name, ...)
{
	va_list cmdv;
	pipecmd *cmd;

	va_start (cmdv, name);
	cmd = pipecmd_new_sequencev (name, cmdv);
	va_end (cmdv);

	return cmd;
}

static void passthrough (void *data _GL_UNUSED)
{
	for (;;) {
		char buffer[4096];
		size_t r = safe_read (STDIN_FILENO, buffer, 4096);
		if (r == SAFE_READ_ERROR)
			/* TODO: Function commands have no way to signal
			 * errors.
			 */
			break;
		if (r == 0)
			break;
		if (full_write (STDOUT_FILENO, buffer, (size_t) r) <
		    (size_t) r)
			break;
	}

	return;
}

pipecmd *pipecmd_new_passthrough (void)
{
	return pipecmd_new_function ("cat", &passthrough, NULL, NULL);
}

pipecmd *pipecmd_dup (pipecmd *cmd)
{
	pipecmd *newcmd = XMALLOC (pipecmd);
	int i;

	newcmd->tag = cmd->tag;
	newcmd->name = xstrdup (cmd->name);
	newcmd->nice = cmd->nice;
	newcmd->discard_err = cmd->discard_err;
	newcmd->cwd_fd = cmd->cwd_fd;
	newcmd->cwd = cmd->cwd ? xstrdup (cmd->cwd) : NULL;

	newcmd->nenv = cmd->nenv;
	newcmd->env_max = cmd->env_max;
	assert (newcmd->nenv <= newcmd->env_max);
	newcmd->env = xmalloc (newcmd->env_max * sizeof *newcmd->env);

	newcmd->pre_exec_func = cmd->pre_exec_func;
	newcmd->pre_exec_free_func = cmd->pre_exec_free_func;
	newcmd->pre_exec_data = cmd->pre_exec_data;

	for (i = 0; i < cmd->nenv; ++i) {
		newcmd->env[i].name =
		        cmd->env[i].name ? xstrdup (cmd->env[i].name) : NULL;
		newcmd->env[i].value =
		        cmd->env[i].value ? xstrdup (cmd->env[i].value) : NULL;
	}

	switch (newcmd->tag) {
		case PIPECMD_PROCESS: {
			struct pipecmd_process *cmdp = &cmd->u.process;
			struct pipecmd_process *newcmdp = &newcmd->u.process;

			newcmdp->argc = cmdp->argc;
			newcmdp->argv_max = cmdp->argv_max;
			assert (newcmdp->argc < newcmdp->argv_max);
			newcmdp->argv = xmalloc (newcmdp->argv_max *
			                         sizeof *newcmdp->argv);

			for (i = 0; i < cmdp->argc; ++i)
				newcmdp->argv[i] = xstrdup (cmdp->argv[i]);
			newcmdp->argv[cmdp->argc] = NULL;

			break;
		}

		case PIPECMD_FUNCTION: {
			struct pipecmd_function *cmdf = &cmd->u.function;
			struct pipecmd_function *newcmdf = &newcmd->u.function;

			newcmdf->func = cmdf->func;
			newcmdf->free_func = cmdf->free_func;
			newcmdf->data = cmdf->data;

			break;
		}

		case PIPECMD_SEQUENCE: {
			struct pipecmd_sequence *cmds = &cmd->u.sequence;
			struct pipecmd_sequence *newcmds = &newcmd->u.sequence;

			newcmds->ncommands = cmds->ncommands;
			newcmds->commands_max = cmds->commands_max;
			assert (newcmds->ncommands <= newcmds->commands_max);
			newcmds->commands =
			        xmalloc (newcmds->commands_max *
			                 sizeof *newcmds->commands);

			for (i = 0; i < cmds->ncommands; ++i)
				newcmds->commands[i] =
				        pipecmd_dup (cmds->commands[i]);

			break;
		}
	}

	return newcmd;
}

void pipecmd_arg (pipecmd *cmd, const char *arg)
{
	struct pipecmd_process *cmdp;

	assert (cmd->tag == PIPECMD_PROCESS);
	cmdp = &cmd->u.process;

	if (cmdp->argc + 1 >= cmdp->argv_max) {
		cmdp->argv_max *= 2;
		cmdp->argv = xrealloc (cmdp->argv,
		                       cmdp->argv_max * sizeof *cmdp->argv);
	}

	cmdp->argv[cmdp->argc++] = xstrdup (arg);
	assert (cmdp->argc < cmdp->argv_max);
	cmdp->argv[cmdp->argc] = NULL;
}

void pipecmd_argf (pipecmd *cmd, const char *format, ...)
{
	va_list argv;
	char *arg;

	va_start (argv, format);
	arg = xvasprintf (format, argv);
	pipecmd_arg (cmd, arg);
	free (arg);
	va_end (argv);
}

void pipecmd_argv (pipecmd *cmd, va_list argv)
{
	const char *arg = va_arg (argv, const char *);

	assert (cmd->tag == PIPECMD_PROCESS);

	while (arg) {
		pipecmd_arg (cmd, arg);
		arg = va_arg (argv, const char *);
	}
}

void pipecmd_args (pipecmd *cmd, ...)
{
	va_list argv;

	assert (cmd->tag == PIPECMD_PROCESS);

	va_start (argv, cmd);
	pipecmd_argv (cmd, argv);
	va_end (argv);
}

void pipecmd_argstr (pipecmd *cmd, const char *argstr)
{
	char *arg;

	assert (cmd->tag == PIPECMD_PROCESS);

	while ((arg = argstr_get_word (&argstr))) {
		pipecmd_arg (cmd, arg);
		free (arg);
	}
}

int ATTRIBUTE_PURE pipecmd_get_nargs (pipecmd *cmd)
{
	struct pipecmd_process *cmdp;

	assert (cmd->tag == PIPECMD_PROCESS);
	cmdp = &cmd->u.process;

	return cmdp->argc;
}

void pipecmd_nice (pipecmd *cmd, int value)
{
	cmd->nice = value;
}

void pipecmd_discard_err (pipecmd *cmd, int discard_err)
{
	cmd->discard_err = discard_err;
}

void pipecmd_chdir (pipecmd *cmd, const char *directory)
{
	free (cmd->cwd);
	cmd->cwd = xstrdup (directory);
}

void pipecmd_fchdir (pipecmd *cmd, int directory_fd)
{
	cmd->cwd_fd = directory_fd;
}

void pipecmd_setenv (pipecmd *cmd, const char *name, const char *value)
{
	if (cmd->nenv >= cmd->env_max) {
		cmd->env_max *= 2;
		cmd->env =
		        xrealloc (cmd->env, cmd->env_max * sizeof *cmd->env);
	}

	cmd->env[cmd->nenv].name = xstrdup (name);
	cmd->env[cmd->nenv].value = xstrdup (value);
	++cmd->nenv;
}

void pipecmd_unsetenv (pipecmd *cmd, const char *name)
{
	if (cmd->nenv >= cmd->env_max) {
		cmd->env_max *= 2;
		cmd->env =
		        xrealloc (cmd->env, cmd->env_max * sizeof *cmd->env);
	}

	cmd->env[cmd->nenv].name = xstrdup (name);
	cmd->env[cmd->nenv].value = NULL;
	++cmd->nenv;
}

void pipecmd_clearenv (pipecmd *cmd)
{
	if (cmd->nenv >= cmd->env_max) {
		cmd->env_max *= 2;
		cmd->env =
		        xrealloc (cmd->env, cmd->env_max * sizeof *cmd->env);
	}

	cmd->env[cmd->nenv].name = NULL;
	cmd->env[cmd->nenv].value = NULL;
	++cmd->nenv;
}

void pipecmd_pre_exec (pipecmd *cmd, pipecmd_function_type *func,
                       pipecmd_function_free_type *free_func, void *data)
{
	cmd->pre_exec_func = func;
	cmd->pre_exec_free_func = free_func;
	cmd->pre_exec_data = data;
}

void pipecmd_sequence_command (pipecmd *cmd, pipecmd *child)
{
	struct pipecmd_sequence *cmds;

	assert (cmd->tag == PIPECMD_SEQUENCE);
	cmds = &cmd->u.sequence;

	if (cmds->ncommands >= cmds->commands_max) {
		cmds->commands_max *= 2;
		cmds->commands =
		        xrealloc (cmds->commands,
		                  cmds->commands_max * sizeof *cmds->commands);
	}

	cmds->commands[cmds->ncommands++] = child;
}

void pipecmd_dump (pipecmd *cmd, FILE *stream)
{
	int i;

	if (cmd->cwd_fd >= 0)
		fprintf (stream, "(cd <fd %d> && ", cmd->cwd_fd);
	else if (cmd->cwd)
		fprintf (stream, "(cd %s && ", cmd->cwd);

	for (i = 0; i < cmd->nenv; ++i) {
		if (cmd->env[i].name)
			fprintf (stream, "%s=%s ", cmd->env[i].name,
			         cmd->env[i].value ? cmd->env[i].value
			                           : "<unset>");
		else
			fputs ("env -i ", stream);
	}

	switch (cmd->tag) {
		case PIPECMD_PROCESS: {
			struct pipecmd_process *cmdp = &cmd->u.process;

			fputs (cmd->name, stream);
			for (i = 1; i < cmdp->argc; ++i) {
				/* TODO: escape_shell()? */
				putc (' ', stream);
				fputs (cmdp->argv[i], stream);
			}

			break;
		}

		case PIPECMD_FUNCTION:
			fputs (cmd->name, stream);
			break;

		case PIPECMD_SEQUENCE: {
			struct pipecmd_sequence *cmds = &cmd->u.sequence;

			putc ('(', stream);
			for (i = 0; i < cmds->ncommands; ++i) {
				pipecmd_dump (cmds->commands[i], stream);
				if (i < cmds->ncommands - 1)
					fputs (" && ", stream);
			}
			putc (')', stream);

			break;
		}
	}

	if (cmd->cwd_fd >= 0 || cmd->cwd)
		putc (')', stream);
}

char *pipecmd_tostring (pipecmd *cmd)
{
	char *out = NULL;
	int i;

	if (cmd->cwd_fd >= 0) {
		char *cwd_fd_str = xasprintf ("%d", cmd->cwd_fd);
		out = appendstr (out, "(cd <fd ", cwd_fd_str, "> && ",
		                 (void *) 0);
		free (cwd_fd_str);
	} else if (cmd->cwd)
		out = appendstr (out, "(cd ", cmd->cwd, " && ", (void *) 0);

	for (i = 0; i < cmd->nenv; ++i) {
		if (cmd->env[i].name)
			out = appendstr (out, cmd->env[i].name, "=",
			                 cmd->env[i].value ? cmd->env[i].value
			                                   : "<unset>",
			                 " ", (void *) 0);
		else
			out = appendstr (out, "env -i ", (void *) 0);
	}

	switch (cmd->tag) {
		case PIPECMD_PROCESS: {
			struct pipecmd_process *cmdp = &cmd->u.process;

			out = appendstr (out, cmd->name, (void *) 0);
			for (i = 1; i < cmdp->argc; ++i)
				/* TODO: escape_shell()? */
				out = appendstr (out, " ", cmdp->argv[i],
				                 (void *) 0);

			break;
		}

		case PIPECMD_FUNCTION:
			out = appendstr (out, cmd->name, (void *) 0);
			break;

		case PIPECMD_SEQUENCE: {
			struct pipecmd_sequence *cmds = &cmd->u.sequence;

			out = appendstr (out, "(", (void *) 0);
			for (i = 0; i < cmds->ncommands; ++i) {
				char *subout =
				        pipecmd_tostring (cmds->commands[i]);
				out = appendstr (out, subout, (void *) 0);
				free (subout);
				if (i < cmds->ncommands - 1)
					out = appendstr (out, " && ",
					                 (void *) 0);
			}
			out = appendstr (out, ")", (void *) 0);

			break;
		}
	}

	if (cmd->cwd_fd >= 0 || cmd->cwd)
		out = appendstr (out, ")", (void *) 0);

	return out;
}

/* Children exit with this status if execvp fails. */
#define EXEC_FAILED_EXIT_STATUS 0xff

#ifdef SIGPIPE
#  define IS_SIGPIPE(sig) (sig == SIGPIPE)
#else
#  define IS_SIGPIPE(sig) 0
#endif

/* When called internally during pipeline execution, this is called in the
 * forked child process, with file descriptors already set up.
 */
void pipecmd_exec (pipecmd *cmd)
{
	int i;

	if (cmd->nice)
		if (nice (cmd->nice) < 0)
			/* Don't worry too much. */
			debug ("nice failed: %s\n", strerror (errno));

	if (cmd->discard_err) {
		int devnull = open ("/dev/null", O_WRONLY);
		if (devnull != -1) {
			dup2 (devnull, 2);
			close (devnull);
		}
	}

	if (cmd->cwd_fd >= 0) {
		if (fchdir (cmd->cwd_fd) < 0)
			error (EXEC_FAILED_EXIT_STATUS, errno,
			       "can't change directory to fd %d", cmd->cwd_fd);
	} else if (cmd->cwd) {
		if (chdir (cmd->cwd) < 0)
			error (EXEC_FAILED_EXIT_STATUS, errno,
			       "can't change directory to '%s'", cmd->cwd);
	}

	for (i = 0; i < cmd->nenv; ++i) {
		if (cmd->env[i].name) {
			if (cmd->env[i].value)
				setenv (cmd->env[i].name, cmd->env[i].value,
				        1);
			else
				unsetenv (cmd->env[i].name);
		} else
			clearenv ();
	}

	switch (cmd->tag) {
		case PIPECMD_PROCESS: {
			struct pipecmd_process *cmdp = &cmd->u.process;
			if (cmd->pre_exec_func)
				cmd->pre_exec_func (cmd->pre_exec_data);
			execvp (cmd->name, cmdp->argv);
			break;
		}

		/* TODO: ideally, could there be a facility
		 * to execute non-blocking functions without
		 * needing to fork?
		 */
		case PIPECMD_FUNCTION: {
			struct pipecmd_function *cmdf = &cmd->u.function;
			if (cmd->pre_exec_func)
				cmd->pre_exec_func (cmd->pre_exec_data);
			cmdf->func (cmdf->data);
			/* pacify valgrind et al */
			if (cmdf->free_func)
				cmdf->free_func (cmdf->data);
			if (cmd->pre_exec_free_func)
				cmd->pre_exec_free_func (cmd->pre_exec_data);
			fflush (NULL);
			_exit (0);
		}

		case PIPECMD_SEQUENCE: {
			struct pipecmd_sequence *cmds = &cmd->u.sequence;
			struct sigaction sa;

			/* Flush all pending output so that subprocesses
			 * don't inherit it.
			 */
			fflush (NULL);

			/* pipeline_start will have blocked SIGCHLD. We like
			 * it that way. Lose the parent's signal handler,
			 * though.
			 */
			memset (&sa, 0, sizeof sa);
			sa.sa_handler = SIG_DFL;
			sigemptyset (&sa.sa_mask);
			sa.sa_flags = 0;
			if (sigaction (SIGCHLD, &sa, NULL) == -1)
				fatal (errno, "can't install SIGCHLD handler");

			for (i = 0; i < cmds->ncommands; ++i) {
				pipecmd *child = cmds->commands[i];
				pid_t pid = fork ();
				int status;

				if (pid < 0)
					fatal (errno, "fork failed");
				if (pid == 0)
					pipecmd_exec (child);
				debug ("Started \"%s\", pid %d\n", child->name,
				       pid);

				while (waitpid (pid, &status, 0) < 0) {
					if (errno == EINTR)
						continue;
					fatal (errno, "waitpid failed");
				}

				debug ("  \"%s\" (%d) -> %d\n", child->name,
				       pid, status);

				if (WIFSIGNALED (status)) {
					int sig = WTERMSIG (status);
					if (IS_SIGPIPE (sig))
						status = 0;
					else if (getenv ("PIPELINE_QUIET"))
						;
					else if (WCOREDUMP (status))
						error (0, 0,
						       "%s: %s (core dumped)",
						       child->name,
						       strsignal (sig));
					else
						error (0, 0, "%s: %s",
						       child->name,
						       strsignal (sig));
				} else if (!WIFEXITED (status))
					error (0, 0, "unexpected status %d",
					       status);

				if (child->tag == PIPECMD_FUNCTION) {
					struct pipecmd_function *cmdf =
					        &child->u.function;
					if (cmdf->free_func)
						(*cmdf->free_func) (
						        cmdf->data);
				}

				if (WIFSIGNALED (status)) {
					raise (WTERMSIG (status));
					_exit (1); /* just to make sure */
				} else if (status && WIFEXITED (status))
					_exit (WEXITSTATUS (status));
			}

			_exit (0);
		}
	}

	error (EXEC_FAILED_EXIT_STATUS, errno, "can't execute %s", cmd->name);
	/* Never called, but gcc doesn't realise that error with non-zero
	 * status always exits.
	 */
	_exit (EXEC_FAILED_EXIT_STATUS);
}

void pipecmd_free (pipecmd *cmd)
{
	int i;

	if (!cmd)
		return;

	free (cmd->name);
	free (cmd->cwd);

	for (i = 0; i < cmd->nenv; ++i) {
		free (cmd->env[i].name);
		free (cmd->env[i].value);
	}
	free (cmd->env);

	switch (cmd->tag) {
		case PIPECMD_PROCESS: {
			struct pipecmd_process *cmdp = &cmd->u.process;

			for (i = 0; i < cmdp->argc; ++i)
				free (cmdp->argv[i]);
			free (cmdp->argv);

			break;
		}

		case PIPECMD_FUNCTION:
			break;

		case PIPECMD_SEQUENCE: {
			struct pipecmd_sequence *cmds = &cmd->u.sequence;

			for (i = 0; i < cmds->ncommands; ++i)
				pipecmd_free (cmds->commands[i]);
			free (cmds->commands);

			break;
		}
	}

	free (cmd);
}

/* ---------------------------------------------------------------------- */

/* Functions to build pipelines. */

pipeline *pipeline_new (void)
{
	pipeline *p = XMALLOC (pipeline);
	p->ncommands = 0;
	p->commands_max = 4;
	p->commands = xnmalloc (p->commands_max, sizeof *p->commands);
	p->pids = NULL;
	p->statuses = NULL;
	p->redirect_in = p->redirect_out = REDIRECT_NONE;
	p->want_in = p->want_out = 0;
	p->want_infile = p->want_outfile = NULL;
	p->infd = p->outfd = -1;
	p->infile = p->outfile = NULL;
	p->source = NULL;
	p->buffer = NULL;
	p->buflen = p->bufmax = 0;
	p->line_cache = NULL;
	p->peek_offset = 0;
	p->ignore_signals = 0;
	return p;
}

pipeline *pipeline_new_commandv (pipecmd *cmd1, va_list cmdv)
{
	pipeline *p = pipeline_new ();
	pipeline_command (p, cmd1);
	pipeline_commandv (p, cmdv);
	return p;
}

pipeline *pipeline_new_commands (pipecmd *cmd1, ...)
{
	va_list cmdv;
	pipeline *p;

	va_start (cmdv, cmd1);
	p = pipeline_new_commandv (cmd1, cmdv);
	va_end (cmdv);

	return p;
}

pipeline *pipeline_new_command_argv (const char *name, va_list argv)
{
	pipeline *p;
	pipecmd *cmd;

	p = pipeline_new ();
	cmd = pipecmd_new_argv (name, argv);
	pipeline_command (p, cmd);

	return p;
}

pipeline *pipeline_new_command_args (const char *name, ...)
{
	va_list argv;
	pipeline *p;

	va_start (argv, name);
	p = pipeline_new_command_argv (name, argv);
	va_end (argv);

	return p;
}

pipeline *pipeline_join (pipeline *p1, pipeline *p2)
{
	pipeline *p = XMALLOC (pipeline);
	int i;

	assert (!p1->pids);
	assert (!p2->pids);
	assert (!p1->statuses);
	assert (!p2->statuses);

	p->ncommands = p1->ncommands + p2->ncommands;
	p->commands_max = p1->ncommands + p2->ncommands;
	p->commands = xnmalloc (p->commands_max, sizeof *p->commands);
	p->pids = NULL;
	p->statuses = NULL;
	p->redirect_in = p1->redirect_in;
	p->want_in = p1->want_in;
	p->want_infile = p1->want_infile ? xstrdup (p1->want_infile) : NULL;
	p->redirect_out = p2->redirect_out;
	p->want_out = p2->want_out;
	p->want_outfile = p2->want_outfile ? xstrdup (p2->want_outfile) : NULL;
	p->infd = p1->infd;
	p->outfd = p2->outfd;
	p->infile = p1->infile;
	p->outfile = p2->outfile;
	p->source = NULL;
	p->buffer = NULL;
	p->buflen = p->bufmax = 0;
	p->line_cache = NULL;
	p->peek_offset = 0;
	p->ignore_signals = (p1->ignore_signals || p2->ignore_signals);

	for (i = 0; i < p1->ncommands; ++i)
		p->commands[i] = pipecmd_dup (p1->commands[i]);
	for (i = 0; i < p2->ncommands; ++i)
		p->commands[p1->ncommands + i] = pipecmd_dup (p2->commands[i]);

	return p;
}

void pipeline_connect (pipeline *source, pipeline *sink, ...)
{
	va_list argv;
	pipeline *arg;

	/* We must be in control of output from the source pipeline. If the
	 * source isn't started, we can force this.
	 */
	if (!source->pids)
		pipeline_want_out (source, -1);
	assert (source->redirect_out == REDIRECT_FD);
	assert (source->want_out < 0);

	va_start (argv, sink);
	for (arg = sink; arg; arg = va_arg (argv, pipeline *)) {
		assert (!arg->pids); /* not started */
		arg->source = source;
		pipeline_want_in (arg, -1);

		/* Zero-command sinks should represent data being passed
		 * straight through from the input to the output.
		 * Unfortunately pipeline_start and pipeline_pump don't
		 * handle this very well between them; a zero-command
		 * pipeline has the write end of its input pipe wrongly
		 * stashed in outfd and then pipeline_pump can't handle it
		 * because it has nowhere to send output. Until this is
		 * fixed, this kludge is necessary.
		 */
		if (arg->ncommands == 0)
			pipeline_command (arg, pipecmd_new_passthrough ());
	}
	va_end (argv);
}

void pipeline_command (pipeline *p, pipecmd *cmd)
{
	if (p->ncommands >= p->commands_max) {
		p->commands_max *= 2;
		p->commands = xrealloc (p->commands,
		                        p->commands_max * sizeof *p->commands);
	}

	p->commands[p->ncommands++] = cmd;
}

void pipeline_command_argv (pipeline *p, const char *name, va_list argv)
{
	pipecmd *cmd;

	cmd = pipecmd_new_argv (name, argv);
	pipeline_command (p, cmd);
}

void pipeline_command_args (pipeline *p, const char *name, ...)
{
	va_list argv;

	va_start (argv, name);
	pipeline_command_argv (p, name, argv);
	va_end (argv);
}

void pipeline_command_argstr (pipeline *p, const char *argstr)
{
	pipeline_command (p, pipecmd_new_argstr (argstr));
}

void pipeline_commandv (pipeline *p, va_list cmdv)
{
	pipecmd *cmd = va_arg (cmdv, pipecmd *);

	while (cmd) {
		pipeline_command (p, cmd);
		cmd = va_arg (cmdv, pipecmd *);
	}
}

void pipeline_commands (pipeline *p, ...)
{
	va_list cmdv;

	va_start (cmdv, p);
	pipeline_commandv (p, cmdv);
	va_end (cmdv);
}

int ATTRIBUTE_PURE pipeline_get_ncommands (pipeline *p)
{
	return p->ncommands;
}

pipecmd *ATTRIBUTE_PURE pipeline_get_command (pipeline *p, int n)
{
	if (n < 0 || n >= p->ncommands)
		return NULL;
	return p->commands[n];
}

pipecmd *pipeline_set_command (pipeline *p, int n, pipecmd *cmd)
{
	pipecmd *prev;
	if (n < 0 || n >= p->ncommands)
		return NULL;
	prev = p->commands[n];
	p->commands[n] = cmd;
	return prev;
}

pid_t ATTRIBUTE_PURE pipeline_get_pid (pipeline *p, int n)
{
	assert (p->pids); /* pipeline started */
	if (n < 0 || n >= p->ncommands)
		return -1;
	return p->pids[n];
}

void pipeline_want_in (pipeline *p, int fd)
{
	p->redirect_in = REDIRECT_FD;
	p->want_in = fd;
	p->want_infile = NULL;
}

void pipeline_want_out (pipeline *p, int fd)
{
	p->redirect_out = REDIRECT_FD;
	p->want_out = fd;
	p->want_outfile = NULL;
}

void pipeline_want_infile (pipeline *p, const char *file)
{
	p->redirect_in = (file != NULL) ? REDIRECT_FILE_NAME : REDIRECT_NONE;
	p->want_in = 0;
	p->want_infile = file ? xstrdup (file) : NULL;
}

void pipeline_want_outfile (pipeline *p, const char *file)
{
	p->redirect_out = (file != NULL) ? REDIRECT_FILE_NAME : REDIRECT_NONE;
	p->want_out = 0;
	p->want_outfile = file ? xstrdup (file) : NULL;
}

void pipeline_ignore_signals (pipeline *p, int ignore_signals)
{
	p->ignore_signals = ignore_signals;
}

FILE *pipeline_get_infile (pipeline *p)
{
	assert (p->pids); /* pipeline started */
	assert (p->statuses);
	if (p->infile)
		return p->infile;
	else if (p->infd == -1) {
		error (0, 0, "pipeline input not open");
		return NULL;
	} else
		return p->infile = fdopen (p->infd, "w");
}

FILE *pipeline_get_outfile (pipeline *p)
{
	assert (p->pids); /* pipeline started */
	assert (p->statuses);
	if (p->outfile)
		return p->outfile;
	else if (p->outfd == -1) {
		error (0, 0, "pipeline output not open");
		return NULL;
	} else
		return p->outfile = fdopen (p->outfd, "r");
}

void pipeline_dump (pipeline *p, FILE *stream)
{
	int i;

	for (i = 0; i < p->ncommands; ++i) {
		pipecmd_dump (p->commands[i], stream);
		if (i < p->ncommands - 1)
			fputs (" | ", stream);
	}
	/* clang-format off */
	fprintf (stream, " [input: {%d, %s}, output: {%d, %s}]\n",
	         p->want_in, p->want_infile ? p->want_infile : "NULL",
	         p->want_out, p->want_outfile ? p->want_outfile : "NULL");
	/* clang-format on */
}

char *pipeline_tostring (pipeline *p)
{
	char *out = NULL;
	int i;

	for (i = 0; i < p->ncommands; ++i) {
		char *cmdout = pipecmd_tostring (p->commands[i]);
		out = appendstr (out, cmdout, (void *) 0);
		free (cmdout);
		if (i < p->ncommands - 1)
			out = appendstr (out, " | ", (void *) 0);
	}

	return out;
}

void pipeline_free (pipeline *p)
{
	int i;

	if (!p)
		return;
	if (p->pids)
		pipeline_wait (p);

	for (i = 0; i < p->ncommands; ++i)
		pipecmd_free (p->commands[i]);
	free (p->commands);
	free (p->pids);
	free (p->statuses);
	free (p->want_infile);
	free (p->want_outfile);
	free (p->buffer);
	free (p->line_cache);
	free (p);
}

/* ---------------------------------------------------------------------- */

/* Functions to run pipelines and handle signals. */

static pipeline **active_pipelines = NULL;
static int n_active_pipelines = 0, max_active_pipelines = 0;

static int sigchld = 0;
static int queue_sigchld = 0;

static int reap_children (int block)
{
	pid_t pid;
	int status;
	int collected = 0;

	do {
		int i;

		if (sigchld) {
			/* Deal with a SIGCHLD delivery. */
			pid = waitpid (-1, &status, WNOHANG);
			--sigchld;
		} else
			pid = waitpid (-1, &status, block ? 0 : WNOHANG);

		if (pid < 0 && errno == EINTR) {
			/* Try again. */
			pid = 0;
			continue;
		}

		if (pid <= 0)
			/* We've run out of children to reap. */
			break;

		++collected;

		/* Deliver the command status if possible. */
		for (i = 0; i < n_active_pipelines; ++i) {
			pipeline *p = active_pipelines[i];
			int j;

			if (!p || !p->pids || !p->statuses)
				continue;

			for (j = 0; j < p->ncommands; ++j) {
				if (p->pids[j] == pid) {
					p->statuses[j] = status;
					i = n_active_pipelines;
					break;
				}
			}
		}
	} while ((sigchld || block == 0) && pid >= 0);

	if (collected)
		return collected;
	else
		return -1;
}

static void pipeline_sigchld (int signum)
{
	/* really an assert, but that's not async-signal-safe */
	if (signum == SIGCHLD) {
		++sigchld;

		if (!queue_sigchld) {
			int save_errno = errno;
			reap_children (0);
			errno = save_errno;
		}
	}
}

static void pipeline_install_sigchld (void)
{
	struct sigaction act;
	static int installed = 0;

	if (installed)
		return;

	memset (&act, 0, sizeof act);
	act.sa_handler = &pipeline_sigchld;
	sigemptyset (&act.sa_mask);
	sigaddset (&act.sa_mask, SIGINT);
	sigaddset (&act.sa_mask, SIGTERM);
	sigaddset (&act.sa_mask, SIGHUP);
	sigaddset (&act.sa_mask, SIGCHLD);
	act.sa_flags = 0;
#ifdef SA_NOCLDSTOP
	act.sa_flags |= SA_NOCLDSTOP;
#endif
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif
	if (sigaction (SIGCHLD, &act, NULL) == -1)
		fatal (errno, "can't install SIGCHLD handler");

	installed = 1;
}

static pipeline_post_fork_fn *post_fork = NULL;

void pipeline_install_post_fork (pipeline_post_fork_fn *fn)
{
	post_fork = fn;
}

static int ignored_signals = 0;
static struct sigaction osa_sigint, osa_sigquit;

void pipeline_start (pipeline *p)
{
	int i, j;
	int last_input = -1;
	int infd[2];
	sigset_t set, oset;

	/* Make sure our SIGCHLD handler is installed. */
	pipeline_install_sigchld ();

	assert (!p->pids); /* pipeline not started already */
	assert (!p->statuses);

	init_debug ();
	if (debug_level) {
		debug ("Starting pipeline: ");
		pipeline_dump (p, stderr);
	}

	/* Flush all pending output so that subprocesses don't inherit it. */
	fflush (NULL);

	if (p->ignore_signals && !ignored_signals++) {
		struct sigaction sa;

		/* Ignore SIGINT and SIGQUIT while subprocesses are running,
		 * just like system().
		 */
		memset (&sa, 0, sizeof sa);
		sa.sa_handler = SIG_IGN;
		sigemptyset (&sa.sa_mask);
		sa.sa_flags = 0;
		if (sigaction (SIGINT, &sa, &osa_sigint) < 0)
			fatal (errno, "Couldn't ignore SIGINT");
		if (sigaction (SIGQUIT, &sa, &osa_sigquit) < 0)
			fatal (errno, "Couldn't ignore SIGQUIT");
	}

	/* Add to the table of active pipelines, so that signal handlers
	 * know what to do with exit statuses. Block SIGCHLD so that we can
	 * do this safely.
	 */
	sigemptyset (&set);
	sigaddset (&set, SIGCHLD);
	sigemptyset (&oset);
	while (sigprocmask (SIG_BLOCK, &set, &oset) == -1 && errno == EINTR)
		;

	/* Grow the table if necessary. */
	if (n_active_pipelines >= max_active_pipelines) {
		int filled = max_active_pipelines;
		if (max_active_pipelines)
			max_active_pipelines *= 2;
		else
			max_active_pipelines = 4;
		/* reduces to xmalloc (...) if active_pipelines == NULL */
		active_pipelines = xrealloc (active_pipelines,
		                             max_active_pipelines *
		                                     sizeof *active_pipelines);
		memset (active_pipelines + filled, 0,
		        (max_active_pipelines - filled) *
		                sizeof *active_pipelines);
	}

	for (i = 0; i < max_active_pipelines; ++i)
		if (!active_pipelines[i]) {
			active_pipelines[i] = p;
			break;
		}
	assert (i < max_active_pipelines);
	++n_active_pipelines;

	p->pids = xcalloc (p->ncommands, sizeof *p->pids);
	p->statuses = xcalloc (p->ncommands, sizeof *p->statuses);

	/* Unblock SIGCHLD. */
	while (sigprocmask (SIG_SETMASK, &oset, NULL) == -1 && errno == EINTR)
		;

	if (p->redirect_in == REDIRECT_FD && p->want_in < 0) {
		if (pipe (infd) < 0)
			fatal (errno, "pipe failed");
		last_input = infd[0];
		p->infd = infd[1];
	} else if (p->redirect_in == REDIRECT_FD)
		last_input = p->want_in;
	else if (p->redirect_in == REDIRECT_FILE_NAME) {
		assert (p->want_infile);
		last_input = open (p->want_infile, O_RDONLY);
		if (last_input < 0)
			fatal (errno, "can't open %s", p->want_infile);
	}

	for (i = 0; i < p->ncommands; i++) {
		int pdes[2];
		pid_t pid;
		int output_read = -1, output_write = -1;

		if (i != p->ncommands - 1 ||
		    (p->redirect_out == REDIRECT_FD && p->want_out < 0)) {
			if (pipe (pdes) < 0)
				fatal (errno, "pipe failed");
			if (i == p->ncommands - 1)
				p->outfd = pdes[0];
			output_read = pdes[0];
			output_write = pdes[1];
		} else {
			if (p->redirect_out == REDIRECT_FD)
				output_write = p->want_out;
			else if (p->redirect_out == REDIRECT_FILE_NAME) {
				assert (p->want_outfile);
				output_write = open (
				        p->want_outfile,
				        O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (output_write < 0)
					fatal (errno, "can't open %s",
					       p->want_outfile);
			}
		}

		/* Block SIGCHLD so that the signal handler doesn't collect
		 * the exit status before we've filled in the pids array.
		 */
		sigemptyset (&set);
		sigaddset (&set, SIGCHLD);
		sigemptyset (&oset);
		while (sigprocmask (SIG_BLOCK, &set, &oset) == -1 &&
		       errno == EINTR)
			;

		pid = fork ();
		if (pid < 0)
			fatal (errno, "fork failed");
		if (pid == 0) {
			/* child */
			if (post_fork)
				post_fork ();

			/* input, reading side */
			if (last_input != -1) {
				if (dup2 (last_input, 0) < 0)
					fatal (errno, "dup2 failed");
				if (close (last_input) < 0)
					fatal (errno, "close failed");
			}

			/* output, writing side */
			if (output_write != -1) {
				if (dup2 (output_write, 1) < 0)
					fatal (errno, "dup2 failed");
				if (close (output_write) < 0)
					fatal (errno, "close failed");
			}

			/* output, reading side */
			if (output_read != -1)
				if (close (output_read))
					fatal (errno, "close failed");

			/* input from first command, writing side; must close
			 * it in every child because it has to be created
			 * before forking anything
			 */
			if (p->infd != -1)
				if (close (p->infd))
					fatal (errno, "close failed");

			/* inputs and outputs from other active pipelines */
			for (j = 0; j < n_active_pipelines; ++j) {
				pipeline *active = active_pipelines[j];
				if (!active || active == p)
					continue;
				/* ignore failures */
				if (active->infd != -1)
					close (active->infd);
				if (active->outfd != -1)
					close (active->outfd);
			}

			/* Restore signals. */
			if (p->ignore_signals) {
				sigaction (SIGINT, &osa_sigint, NULL);
				sigaction (SIGQUIT, &osa_sigquit, NULL);
			}

			pipecmd_exec (p->commands[i]);
			/* never returns */
		}

		/* in the parent */
		if (last_input != -1) {
			if (close (last_input) < 0)
				fatal (errno, "close failed");
		}
		if (output_write != -1) {
			if (close (output_write) < 0)
				fatal (errno, "close failed");
		}
		if (output_read != -1)
			last_input = output_read;
		p->pids[i] = pid;
		p->statuses[i] = -1;

		/* Unblock SIGCHLD. */
		while (sigprocmask (SIG_SETMASK, &oset, NULL) == -1 &&
		       errno == EINTR)
			;

		debug ("Started \"%s\", pid %d\n", p->commands[i]->name, pid);
	}

	if (p->ncommands == 0)
		p->outfd = last_input;
}

int pipeline_wait_all (pipeline *p, int **statuses, int *n_statuses)
{
	int ret = 0;
	int proc_count = p->ncommands;
	int i;
	int raise_signal = 0;

	init_debug ();
	if (debug_level) {
		debug ("Waiting for pipeline: ");
		pipeline_dump (p, stderr);
	}

	assert (p->pids); /* pipeline started */
	assert (p->statuses);

	if (p->infile) {
		if (fclose (p->infile))
			error (0, errno,
			       "closing pipeline input stream failed");
		p->infile = NULL;
		p->infd = -1;
	} else if (p->infd != -1) {
		if (close (p->infd))
			error (0, errno, "closing pipeline input failed");
		p->infd = -1;
	}

	if (p->outfile) {
		if (fclose (p->outfile)) {
			error (0, errno,
			       "closing pipeline output stream failed");
			ret = 127;
		}
		p->outfile = NULL;
		p->outfd = -1;
	} else if (p->outfd != -1) {
		if (close (p->outfd)) {
			error (0, errno, "closing pipeline output failed");
			ret = 127;
		}
		p->outfd = -1;
	}

	/* Tell the SIGCHLD handler not to get in our way. */
	queue_sigchld = 1;

	while (proc_count > 0) {
		int r;

		debug ("Active processes (%d):\n", proc_count);

		/* Check for any statuses already collected by SIGCHLD
		 * handlers or the previous iteration before calling
		 * reap_children() again.
		 */
		for (i = 0; i < p->ncommands; ++i) {
			int status;

			if (p->pids[i] == -1)
				continue;

			debug ("  \"%s\" (%d) -> %d\n", p->commands[i]->name,
			       p->pids[i], p->statuses[i]);

			if (p->statuses[i] == -1)
				continue;

			status = p->statuses[i];
			p->pids[i] = -1;
			--proc_count;
			if (WIFSIGNALED (status)) {
				int sig = WTERMSIG (status);
				if (IS_SIGPIPE (sig))
					status = 0;
				else if (sig == SIGINT || sig == SIGQUIT)
					/* signals currently blocked,
					 * re-raise later
					 */
					raise_signal = sig;
				else if (getenv ("PIPELINE_QUIET"))
					;
				else if (WCOREDUMP (status))
					error (0, 0, "%s: %s (core dumped)",
					       p->commands[i]->name,
					       strsignal (sig));
				else
					error (0, 0, "%s: %s",
					       p->commands[i]->name,
					       strsignal (sig));
			} else if (!WIFEXITED (status))
				error (0, 0, "unexpected status %d", status);

			if (p->commands[i]->tag == PIPECMD_FUNCTION) {
				struct pipecmd_function *cmdf =
				        &p->commands[i]->u.function;
				if (cmdf->free_func)
					(*cmdf->free_func) (cmdf->data);
			}

			if (i == p->ncommands - 1) {
				if (WIFSIGNALED (status))
					ret = 128 + WTERMSIG (status);
				else if (WEXITSTATUS (status))
					ret = WEXITSTATUS (status);
			} else if (!ret && (WIFSIGNALED (status) ||
			                    WEXITSTATUS (status)))
				ret = 127;
		}

		assert (proc_count >= 0);
		if (proc_count == 0)
			break;

		errno = 0;
		r = reap_children (1);

		if (r == -1 && errno == ECHILD)
			/* Eh? The pipeline was allegedly still running, so
			 * we shouldn't have got ECHILD.
			 */
			fatal (errno, "waitpid failed");
	}

	queue_sigchld = 0;

	for (i = 0; i < n_active_pipelines; ++i)
		if (active_pipelines[i] == p)
			active_pipelines[i] = NULL;

	/* It isn't normally safe to compactify active_pipelines as we go,
	 * because it's used by a signal handler.  However, if it's entirely
	 * empty, we can safely clean it up now.  This prevents the table
	 * growing without bound, not to mention pacifying valgrind.
	 */
	for (i = 0; i < n_active_pipelines; ++i)
		if (active_pipelines[i])
			break;
	if (i == n_active_pipelines) {
		n_active_pipelines = 0;
		max_active_pipelines = 0;
		free (active_pipelines);
		active_pipelines = NULL;
	}

	if (statuses && n_statuses) {
		*statuses = xnmalloc (p->ncommands, sizeof **statuses);
		*n_statuses = p->ncommands;
		for (i = 0; i < p->ncommands; ++i)
			(*statuses)[i] = p->statuses[i];
	}

	free (p->pids);
	p->pids = NULL;
	free (p->statuses);
	p->statuses = NULL;

	if (p->ignore_signals && !--ignored_signals) {
		/* Restore signals. */
		sigaction (SIGINT, &osa_sigint, NULL);
		sigaction (SIGQUIT, &osa_sigquit, NULL);
	}

	if (raise_signal)
		raise (raise_signal);

	return ret;
}

int pipeline_wait (pipeline *p)
{
	return pipeline_wait_all (p, NULL, NULL);
}

int pipeline_run (pipeline *p)
{
	int status;

	pipeline_start (p);
	status = pipeline_wait (p);
	pipeline_free (p);

	return status;
}

void pipeline_pump (pipeline *p, ...)
{
	va_list argv;
	int argc, i, j;
	pipeline *arg, **pieces;
	size_t *pos;
	int *known_source, *blocking_in, *blocking_out, *dying_source,
	        *waiting, *write_error;
	struct sigaction sa, osa_sigpipe;

	/* Count pipelines and allocate space for arrays. */
	va_start (argv, p);
	argc = 0;
	for (arg = p; arg; arg = va_arg (argv, pipeline *))
		++argc;
	va_end (argv);
	pieces = xnmalloc (argc, sizeof *pieces);
	pos = xnmalloc (argc, sizeof *pos);
	known_source = xcalloc (argc, sizeof *known_source);
	blocking_in = xcalloc (argc, sizeof *blocking_in);
	blocking_out = xcalloc (argc, sizeof *blocking_out);
	dying_source = xcalloc (argc, sizeof *dying_source);
	waiting = xcalloc (argc, sizeof *waiting);
	write_error = xcalloc (argc, sizeof *write_error);

	/* Set up arrays of pipelines and their read positions. Start all
	 * pipelines if necessary.
	 */
	va_start (argv, p);
	for (arg = p, i = 0; i < argc; arg = va_arg (argv, pipeline *), ++i) {
		pieces[i] = arg;
		pos[i] = 0;
		if (!pieces[i]->pids)
			pipeline_start (pieces[i]);
	}
	assert (arg == NULL);
	va_end (argv);

	/* All source pipelines must be supplied as arguments. */
	for (i = 0; i < argc; ++i) {
		int found = 0;
		if (!pieces[i]->source)
			continue;
		for (j = 0; j < argc; ++j) {
			if (pieces[i]->source == pieces[j]) {
				known_source[j] = found = 1;
				break;
			}
		}
		assert (found);
	}

	for (i = 0; i < argc; ++i) {
		int flags;
		if (pieces[i]->infd != -1) {
			flags = fcntl (pieces[i]->infd, F_GETFL);
			if (!(flags & O_NONBLOCK)) {
				blocking_in[i] = 1;
				fcntl (pieces[i]->infd, F_SETFL,
				       flags | O_NONBLOCK);
			}
		}
		if (pieces[i]->outfd != -1) {
			flags = fcntl (pieces[i]->outfd, F_GETFL);
			if (!(flags & O_NONBLOCK)) {
				blocking_out[i] = 1;
				fcntl (pieces[i]->outfd, F_SETFL,
				       flags | O_NONBLOCK);
			}
		}
	}

#ifdef SIGPIPE
	memset (&sa, 0, sizeof sa);
	sa.sa_handler = SIG_IGN;
	sigemptyset (&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction (SIGPIPE, &sa, &osa_sigpipe);
#endif

#ifdef SA_RESTART
	/* We rely on getting EINTR from select. */
	sigaction (SIGCHLD, NULL, &sa);
	sa.sa_flags &= ~SA_RESTART;
	sigaction (SIGCHLD, &sa, NULL);
#endif

	for (;;) {
		fd_set rfds, wfds;
		int maxfd = -1;
		int ret;

		/* If a source dies and all data from it has been written to
		 * all sinks, close the writing end of the pipe to each of
		 * its sinks.
		 */
		for (i = 0; i < argc; ++i) {
			if (!known_source[i] || pieces[i]->outfd != -1 ||
			    pipeline_peek_size (pieces[i]))
				continue;
			for (j = 0; j < argc; ++j) {
				if (pieces[j]->source == pieces[i] &&
				    pieces[j]->infd != -1) {
					if (close (pieces[j]->infd))
						error (0, errno,
						       "closing pipeline "
						       "input failed");
					pieces[j]->infd = -1;
				}
			}
		}

		/* If all sinks on a source have died, close the reading end
		 * of the pipe from that source.
		 */
		for (i = 0; i < argc; ++i) {
			int got_sink = 0;
			if (!known_source[i] || pieces[i]->outfd == -1)
				continue;
			for (j = 0; j < argc; ++j) {
				if (pieces[j]->source == pieces[i] &&
				    pieces[j]->infd != -1) {
					got_sink = 1;
					break;
				}
			}
			if (got_sink)
				continue;
			if (close (pieces[i]->outfd))
				error (0, errno,
				       "closing pipeline output failed");
			pieces[i]->outfd = -1;
		}

		FD_ZERO (&rfds);
		FD_ZERO (&wfds);
		for (i = 0; i < argc; ++i) {
			/* Input to sink pipeline. */
			if (pieces[i]->source && pieces[i]->infd != -1 &&
			    !waiting[i]) {
				FD_SET (pieces[i]->infd, &wfds);
				if (pieces[i]->infd > maxfd)
					maxfd = pieces[i]->infd;
			}
			/* Output from source pipeline. */
			if (known_source[i] && pieces[i]->outfd != -1) {
				FD_SET (pieces[i]->outfd, &rfds);
				if (pieces[i]->outfd > maxfd)
					maxfd = pieces[i]->outfd;
			}
		}
		if (maxfd == -1)
			break; /* nothing meaningful left to do */

		ret = select (maxfd + 1, &rfds, &wfds, NULL, NULL);
		if (ret < 0 && errno == EINTR) {
			/* Did a source or sink pipeline die? */
			for (i = 0; i < argc; ++i) {
				if (pieces[i]->ncommands == 0)
					continue;
				if (known_source[i] && !dying_source[i] &&
				    pieces[i]->outfd != -1) {
					int last = pieces[i]->ncommands - 1;
					assert (pieces[i]->statuses);
					if (pieces[i]->statuses[last] != -1) {
						debug ("source pipeline %d "
						       "died\n",
						       i);
						dying_source[i] = 1;
					}
				}
				if (pieces[i]->source &&
				    pieces[i]->infd != -1) {
					assert (pieces[i]->statuses);
					if (pieces[i]->statuses[0] != -1) {
						debug ("sink pipeline %d "
						       "died\n",
						       i);
						close (pieces[i]->infd);
						pieces[i]->infd = -1;
					}
				}
			}
			continue;
		} else if (ret < 0)
			fatal (errno, "select");

		/* Read a block of data from each available source pipeline. */
		for (i = 0; i < argc; ++i) {
			size_t peek_size, len;

			if (!known_source[i] || pieces[i]->outfd == -1)
				continue;
			if (!FD_ISSET (pieces[i]->outfd, &rfds))
				continue;

			peek_size = pipeline_peek_size (pieces[i]);
			len = peek_size + 4096;
			if (!pipeline_peek (pieces[i], &len) ||
			    len == peek_size) {
				/* Error or end-of-file; skip this pipeline
				 * from now on.
				 */
				debug ("source pipeline %d returned error "
				       "or EOF\n",
				       i);
				close (pieces[i]->outfd);
				pieces[i]->outfd = -1;
			} else
				/* This is rather a large hammer. Whenever
				 * any data is read from any source
				 * pipeline, we go through and retry all
				 * sink pipelines, even if they aren't
				 * receiving data from the source in
				 * question. This probably results in a few
				 * more passes around the select() loop, but
				 * it eliminates some annoyingly fiddly
				 * bookkeeping.
				 */
				memset (waiting, 0, argc * sizeof *waiting);
		}

		/* Write as much data as we can to each available sink
		 * pipeline.
		 */
		for (i = 0; i < argc; ++i) {
			const char *block;
			size_t peek_size;
			size_t w;
			size_t minpos;

			if (!pieces[i]->source || pieces[i]->infd == -1)
				continue;
			if (!FD_ISSET (pieces[i]->infd, &wfds))
				continue;
			peek_size = pipeline_peek_size (pieces[i]->source);
			if (peek_size <= pos[i]) {
				/* Disable reading until data is read from a
				 * source fd or a child process exits, so
				 * that we neither spin nor block if the
				 * source is slow.
				 */
				waiting[i] = 1;
				continue;
			}

			/* peek a block from the source */
			block = pipeline_peek (pieces[i]->source, &peek_size);
			/* should all already be in the peek cache */
			assert (block);
			assert (peek_size);

			/* write as much of it as will fit to the sink */
			for (;;) {
				w = safe_write (pieces[i]->infd,
				                block + pos[i],
				                peek_size - pos[i]);
				if (w != SAFE_WRITE_ERROR)
					break;
				if (errno == EAGAIN) {
					w = 0;
					break;
				}
				/* It may be useful for other processes to
				 * continue even though this one fails, so
				 * don't FATAL yet.
				 */
				if (errno != EPIPE)
					write_error[i] = errno;
				close (pieces[i]->infd);
				pieces[i]->infd = -1;
				goto next_sink;
			}
			pos[i] += w;
			minpos = pos[i];

			/* check other sinks on the same source, and update
			 * the source's read position if earlier data is no
			 * longer needed by any sink
			 */
			for (j = 0; j < argc; ++j) {
				if (pieces[i]->source != pieces[j]->source ||
				    pieces[j]->infd == -1)
					continue;
				if (pos[j] < minpos)
					minpos = pos[j];
				/* If the source is dead and all data has
				 * been written to this sink, close the
				 * writing end of the pipe to the sink.
				 */
				if (pieces[j]->source->outfd == -1 &&
				    pos[j] >= peek_size) {
					close (pieces[j]->infd);
					pieces[j]->infd = -1;
				}
			}

			/* If some data has been written to all sinks,
			 * discard it from the source's peek cache.
			 */
			pipeline_peek_skip (pieces[i]->source, minpos);
			for (j = 0; j < argc; ++j) {
				if (pieces[i]->source == pieces[j]->source)
					pos[j] -= minpos;
			}
next_sink:;
		}
	}

#ifdef SA_RESTART
	sigaction (SIGCHLD, NULL, &sa);
	sa.sa_flags |= SA_RESTART;
	sigaction (SIGCHLD, &sa, NULL);
#endif

#ifdef SIGPIPE
	sigaction (SIGPIPE, &osa_sigpipe, NULL);
#endif

	for (i = 0; i < argc; ++i) {
		int flags;
		if (blocking_in[i] && pieces[i]->infd != -1) {
			flags = fcntl (pieces[i]->infd, F_GETFL);
			fcntl (pieces[i]->infd, F_SETFL, flags & ~O_NONBLOCK);
		}
		if (blocking_out[i] && pieces[i]->outfd != -1) {
			flags = fcntl (pieces[i]->outfd, F_GETFL);
			fcntl (pieces[i]->outfd, F_SETFL, flags & ~O_NONBLOCK);
		}
	}

	for (i = 0; i < argc; ++i) {
		if (write_error[i])
			fatal (write_error[i], "write to sink %d", i);
	}

	free (write_error);
	free (waiting);
	free (dying_source);
	free (blocking_out);
	free (blocking_in);
	free (known_source);
	free (pieces);
	free (pos);
}

/* ---------------------------------------------------------------------- */

/* Functions to read output from pipelines. */

static const char *get_block (pipeline *p, size_t *len, int peek)
{
	size_t readstart = 0, retstart = 0;
	size_t space = p->bufmax;
	size_t toread = *len;
	size_t r;

	if (p->buffer && p->peek_offset) {
		if (p->peek_offset >= toread) {
			/* We've got the whole thing in the peek cache; just
			 * return it.
			 */
			const char *buffer;
			assert (p->peek_offset <= p->buflen);
			buffer = p->buffer + p->buflen - p->peek_offset;
			if (!peek)
				p->peek_offset -= toread;
			return buffer;
		} else {
			readstart = p->buflen;
			retstart = p->buflen - p->peek_offset;
			space -= p->buflen;
			toread -= p->peek_offset;
		}
	}

	if (toread > space) {
		if (p->buffer)
			p->bufmax = readstart + toread;
		else
			p->bufmax = toread;
		p->buffer = xrealloc (p->buffer, p->bufmax + 1);
	}

	if (!peek)
		p->peek_offset = 0;

	assert (p->outfd != -1);
	r = safe_read (p->outfd, p->buffer + readstart, toread);
	if (r == SAFE_READ_ERROR)
		return NULL;
	p->buflen = readstart + r;
	if (peek)
		p->peek_offset += r;
	*len -= (toread - r);

	return p->buffer + retstart;
}

const char *pipeline_read (pipeline *p, size_t *len)
{
	return get_block (p, len, 0);
}

const char *pipeline_peek (pipeline *p, size_t *len)
{
	return get_block (p, len, 1);
}

size_t ATTRIBUTE_PURE pipeline_peek_size (pipeline *p)
{
	if (!p->buffer)
		return 0;
	return p->peek_offset;
}

void pipeline_peek_skip (pipeline *p, size_t len)
{
	if (len > 0) {
		assert (p->buffer);
		assert (len <= p->peek_offset);
		p->peek_offset -= len;
	}
}

/* readline and peekline repeatedly peek larger and larger buffers until
 * they find a newline or they fail. readline then adjusts the peek offset.
 */

static const char *get_line (pipeline *p, size_t *outlen)
{
	const size_t block = 4096;
	const char *buffer = NULL, *end = NULL;
	int i;
	size_t previous_len = 0;

	if (p->line_cache) {
		free (p->line_cache);
		p->line_cache = NULL;
	}

	if (outlen)
		*outlen = 0;

	for (i = 0;; ++i) {
		size_t len = block * (i + 1);

		buffer = get_block (p, &len, 1);
		if (!buffer || len == 0)
			return NULL;

		if (len == previous_len)
			/* end of file, no newline found */
			end = buffer + len - 1;
		else
			end = memchr (buffer + previous_len, '\n',
			              len - previous_len);
		if (end)
			break;
		previous_len = len;
	}

	if (end) {
		p->line_cache = xstrndup (buffer, end - buffer + 1);
		if (outlen)
			*outlen = end - buffer + 1;
		return p->line_cache;
	} else
		return NULL;
}

const char *pipeline_readline (pipeline *p)
{
	size_t buflen;
	const char *buffer = get_line (p, &buflen);
	if (buffer)
		p->peek_offset -= buflen;
	return buffer;
}

const char *pipeline_peekline (pipeline *p)
{
	return get_line (p, NULL);
}
